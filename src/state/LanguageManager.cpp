#include "state/LanguageManager.h"
#include "state/MapRenderState.h"
#include "mod/ChiyanMap.h"
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <nlohmann/json.hpp>
#include <ll/api/i18n/I18n.h>
#include <mutex>

using json = nlohmann::json;

namespace LanguageManager {
    std::string g_currentLanguage = "en_US";
    std::vector<std::pair<std::string, std::string>> g_availableLanguages;
    static std::unordered_map<std::string, std::string> g_translationCache;
    static std::mutex g_cacheMutex;
    static std::filesystem::path g_langDir;

    static std::filesystem::path GetLanguageDirectory() {
        // 1. 优先通过模块句柄获取 ChiyanMap.dll 所在的绝对路径下的 lang 目录
        // 彻底免疫不同启动器/游戏工作路径 (CWD) 差异导致的相对路径失效
        HMODULE hMod = NULL;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)&Init, &hMod)) {
            wchar_t modPath[MAX_PATH];
            if (GetModuleFileNameW(hMod, modPath, MAX_PATH)) {
                std::filesystem::path dllDir = std::filesystem::path(modPath).parent_path();
                auto langDir = dllDir / "lang";
                std::error_code ec;
                if (std::filesystem::exists(langDir, ec) && std::filesystem::is_directory(langDir, ec)) {
                    return langDir;
                }
            }
        }

        // 2. 尝试 LeviLamina API 获取的 lang 目录
        try {
            auto dir = chiyan_map::ChiyanMap::getInstance().getSelf().getLangDir();
            std::error_code ec;
            if (std::filesystem::exists(dir, ec) && std::filesystem::is_directory(dir, ec)) {
                return dir;
            }
        } catch (...) {}

        // 3. 常见 Fallbacks
        std::error_code ec;
        if (std::filesystem::exists("mods/ChiyanMap/lang", ec)) {
            return "mods/ChiyanMap/lang";
        }
        if (std::filesystem::exists("plugins/ChiyanMap/lang", ec)) {
            return "plugins/ChiyanMap/lang";
        }
        if (std::filesystem::exists("lang", ec)) {
            return "lang";
        }
        return "mods/ChiyanMap/lang";
    }

    void Init() {
        g_langDir = GetLanguageDirectory();

        // 1. 优先尝试 LeviLamina 官方 I18n 批量加载
        if (auto res = ll::i18n::getInstance().load(g_langDir); !res) {
            if (g_langDir != "lang" && std::filesystem::exists("lang")) {
                (void)ll::i18n::getInstance().load("lang");
            }
        }

        // 2. 安全逐文件加载并注册至 ll::i18n，防止目录批量加载中某文件解析异常导致后续语言包漏载
        try {
            std::error_code ec;
            if (std::filesystem::exists(g_langDir, ec) && std::filesystem::is_directory(g_langDir, ec)) {
                for (const auto& entry : std::filesystem::directory_iterator(g_langDir, ec)) {
                    if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
                        std::string stem = entry.path().stem().string();
                        try {
                            std::ifstream ifs(entry.path());
                            if (ifs.is_open()) {
                                json j;
                                ifs >> j;
                                for (auto& [k, v] : j.items()) {
                                    if (v.is_string()) {
                                        ll::i18n::getInstance().set(stem, k, v.get<std::string>());
                                    }
                                }
                            }
                        } catch (...) {}
                    }
                }
            }
        } catch (...) {}

        ScanLanguages();
        LoadConfig();
    }

    void ScanLanguages() {
        g_availableLanguages.clear();

        static const std::vector<std::pair<std::string, std::string>> orderedLangs = {
            {"zh_CN", "简体中文"},
            {"zh_TW", "繁體中文"},
            {"en_US", "English"},
            {"de", "Deutsch"},
            {"es", "Español"},
            {"fr", "Français"},
            {"id", "Bahasa Indonesia"},
            {"it", "Italiano"},
            {"ja", "日本語"},
            {"ko", "한국어"},
            {"pt_BR", "Português (Brasil)"},
            {"ru", "Русский"},
            {"th", "ไทย"},
            {"tr", "Türkçe"},
            {"uk", "Українська"},
            {"vi", "Tiếng Việt"}
        };

        try {
            std::error_code ec;
            if (std::filesystem::exists(g_langDir, ec) && std::filesystem::is_directory(g_langDir, ec)) {
                // 先按照推荐顺序添加已知语言
                for (const auto& item : orderedLangs) {
                    auto p = g_langDir / (item.first + ".json");
                    if (std::filesystem::exists(p, ec)) {
                        g_availableLanguages.push_back(item);
                    }
                }

                // 再扫描并补充其余第三方/自制语言包
                for (const auto& entry : std::filesystem::directory_iterator(g_langDir, ec)) {
                    if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
                        std::string stem = entry.path().stem().string();
                        bool exists = false;
                        for (const auto& p : g_availableLanguages) {
                            if (p.first == stem) { exists = true; break; }
                        }
                        if (!exists) {
                            g_availableLanguages.push_back({stem, stem});
                        }
                    }
                }
            }
        } catch (...) {}

        if (g_availableLanguages.empty()) {
            g_availableLanguages.push_back({"en_US", "English"});
        }
    }

    void LoadLanguage(const std::string& langCode) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_currentLanguage = langCode;
        g_translationCache.clear();
    }

    void LoadConfig() {
        std::string filePath = "mods/ChiyanMap/config.json";
        if (!std::filesystem::exists(filePath)) {
            LANGID langId = GetUserDefaultUILanguage();
            WORD primary = PRIMARYLANGID(langId);
            if (primary == LANG_CHINESE) {
                WORD sub = (WORD)(langId & 0x3ff);
                if (sub == 0x0404 || sub == 0x0c04 || sub == 0x1404) {
                    g_currentLanguage = "zh_TW";
                } else {
                    g_currentLanguage = "zh_CN";
                }
            }
            else if (primary == LANG_GERMAN) g_currentLanguage = "de";
            else if (primary == LANG_FRENCH) g_currentLanguage = "fr";
            else if (primary == LANG_INDONESIAN) g_currentLanguage = "id";
            else if (primary == LANG_ITALIAN) g_currentLanguage = "it";
            else if (primary == LANG_JAPANESE) g_currentLanguage = "ja";
            else if (primary == LANG_KOREAN) g_currentLanguage = "ko";
            else if (primary == LANG_PORTUGUESE) g_currentLanguage = "pt_BR";
            else if (primary == LANG_RUSSIAN) g_currentLanguage = "ru";
            else if (primary == LANG_THAI) g_currentLanguage = "th";
            else if (primary == LANG_TURKISH) g_currentLanguage = "tr";
            else if (primary == LANG_UKRAINIAN) g_currentLanguage = "uk";
            else if (primary == LANG_VIETNAMESE) g_currentLanguage = "vi";
            else if (primary == LANG_SPANISH) g_currentLanguage = "es";
            else g_currentLanguage = "en_US";

            SaveConfig();
            LoadLanguage(g_currentLanguage);
            return;
        }

        std::ifstream in(filePath);
        if (in.is_open()) {
            try {
                json j;
                in >> j;
                g_currentLanguage = j.value("language", "en_US");
                if (g_currentLanguage == "en") {
                    g_currentLanguage = "en_US";
                }
                MapRenderState::showMiniMap = j.value("showMiniMap", true);
                MapRenderState::isSquareMap = j.value("isSquareMap", false);
                MapRenderState::rotateMiniMap = j.value("rotateMiniMap", false);
                MapRenderState::uiTextScale = j.value("uiTextScale", 1.0f);
                MapRenderState::miniMapScale = j.value("miniMapScale", 1.0f);
                MapRenderState::miniMapOffsetX = j.value("miniMapOffsetX", 0.0f);
                MapRenderState::miniMapOffsetY = j.value("miniMapOffsetY", 0.0f);
                MapRenderState::showWaypointsOnMinimap = j.value("showWaypointsOnMinimap", true);
                MapRenderState::g_caveModeType = j.value("caveModeType", (int)MapRenderState::CaveModeType::Layered);
                MapRenderState::g_caveTopYAuto = j.value("caveTopYAuto", true);
                MapRenderState::g_caveTopY = j.value("caveTopY", 64);
                MapRenderState::g_caveDepth = j.value("caveDepth", 30);
                MapRenderState::g_legibleCaveMaps = j.value("legibleCaveMaps", false);
                // 读取快捷键绑定 (持久化保存)
                // [防误操作] openBigMap (M 键) 固定不可配置, 不从配置读取,
                // 避免历史配置中误清除的 0 值导致无法打开操作面板
                if (j.contains("hotkeys") && j["hotkeys"].is_object()) {
                    auto const& hk = j["hotkeys"];
                    auto def = MapRenderState::HotkeyBindings::Defaults();
                    MapRenderState::g_hotkeys.openWaypointMgr = hk.value("openWaypointMgr", def.openWaypointMgr);
                    MapRenderState::g_hotkeys.toggleMinimap   = hk.value("toggleMinimap", def.toggleMinimap);
                    MapRenderState::g_hotkeys.toggleMinimapShape = hk.value("toggleMinimapShape", def.toggleMinimapShape);
                    MapRenderState::g_hotkeys.toggleMinimapRot = hk.value("toggleMinimapRot", def.toggleMinimapRot);
                }
            } catch (...) {
                g_currentLanguage = "en_US";
            }
            in.close();
        }
        LoadLanguage(g_currentLanguage);
    }

    void SaveConfig() {
        std::string filePath = "mods/ChiyanMap/config.json";
        json j;
        j["language"] = g_currentLanguage;
        j["showMiniMap"] = MapRenderState::showMiniMap;
        j["isSquareMap"] = MapRenderState::isSquareMap;
        j["rotateMiniMap"] = MapRenderState::rotateMiniMap;
        j["uiTextScale"] = MapRenderState::uiTextScale;
        j["miniMapScale"] = MapRenderState::miniMapScale;
        j["miniMapOffsetX"] = MapRenderState::miniMapOffsetX;
        j["miniMapOffsetY"] = MapRenderState::miniMapOffsetY;
        j["showWaypointsOnMinimap"] = MapRenderState::showWaypointsOnMinimap;
        j["caveModeType"] = MapRenderState::g_caveModeType;
        j["caveTopYAuto"] = MapRenderState::g_caveTopYAuto;
        j["caveTopY"] = MapRenderState::g_caveTopY;
        j["caveDepth"] = MapRenderState::g_caveDepth;
        j["legibleCaveMaps"] = MapRenderState::g_legibleCaveMaps;

        // 保存快捷键绑定 (持久化保存; openBigMap 固定为默认 M 键, 不保存)
        j["hotkeys"]["openWaypointMgr"] = MapRenderState::g_hotkeys.openWaypointMgr;
        j["hotkeys"]["toggleMinimap"] = MapRenderState::g_hotkeys.toggleMinimap;
        j["hotkeys"]["toggleMinimapShape"] = MapRenderState::g_hotkeys.toggleMinimapShape;
        j["hotkeys"]["toggleMinimapRot"] = MapRenderState::g_hotkeys.toggleMinimapRot;

        std::ofstream out(filePath);
        if (out.is_open()) {
            out << j.dump(4);
            out.close();
        }
    }

    const char* GetText(const std::string& key) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_translationCache.find(key);
        if (it != g_translationCache.end()) {
            return it->second.c_str();
        }

        std::string_view sv = ll::i18n::getInstance().get(key, g_currentLanguage);
        g_translationCache[key] = std::string(sv);
        return g_translationCache[key].c_str();
    }
}
