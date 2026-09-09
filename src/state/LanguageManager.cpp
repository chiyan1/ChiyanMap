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
        try {
            auto dir = chiyan_map::ChiyanMap::getInstance().getSelf().getLangDir();
            if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
                return dir;
            }
        } catch (...) {}

        // Fallbacks
        if (std::filesystem::exists("mods/ChiyanMap/lang")) {
            return "mods/ChiyanMap/lang";
        }
        if (std::filesystem::exists("lang")) {
            return "lang";
        }
        return "mods/ChiyanMap/lang";
    }

    void Init() {
        g_langDir = GetLanguageDirectory();

        // 加载语言包目录至 LeviLamina 官方 I18n 实例
        if (auto res = ll::i18n::getInstance().load(g_langDir); !res) {
            // 尝试备用路径
            if (g_langDir != "lang" && std::filesystem::exists("lang")) {
                (void)ll::i18n::getInstance().load("lang");
            }
        }

        ScanLanguages();
        LoadConfig();
    }

    void ScanLanguages() {
        g_availableLanguages.clear();

        static const std::unordered_map<std::string, std::string> knownLangs = {
            {"de", "Deutsch"},
            {"de_DE", "Deutsch"},
            {"en", "English"},
            {"en_US", "English"},
            {"es", "Español"},
            {"es_ES", "Español"},
            {"fr", "Français"},
            {"fr_FR", "Français"},
            {"id", "Bahasa Indonesia"},
            {"id_ID", "Bahasa Indonesia"},
            {"it", "Italiano"},
            {"it_IT", "Italiano"},
            {"ja", "日本語"},
            {"ja_JP", "日本語"},
            {"ko", "한국어"},
            {"ko_KR", "한국어"},
            {"pt_BR", "Português (Brasil)"},
            {"ru", "Русский"},
            {"ru_RU", "Русский"},
            {"th", "ไทย"},
            {"th_TH", "ไทย"},
            {"tr", "Türkçe"},
            {"tr_TR", "Türkçe"},
            {"uk", "Українська"},
            {"uk_UA", "Українська"},
            {"vi", "Tiếng Việt"},
            {"vi_VN", "Tiếng Việt"},
            {"zh_CN", "简体中文"},
            {"zh_TW", "繁體中文"}
        };

        try {
            if (std::filesystem::exists(g_langDir) && std::filesystem::is_directory(g_langDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(g_langDir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".json") {
                        std::string stem = entry.path().stem().string();
                        auto it = knownLangs.find(stem);
                        std::string displayName = (it != knownLangs.end()) ? it->second : stem;

                        bool exists = false;
                        for (const auto& p : g_availableLanguages) {
                            if (p.first == stem) { exists = true; break; }
                        }
                        if (!exists) {
                            g_availableLanguages.push_back({stem, displayName});
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
