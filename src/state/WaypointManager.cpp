#include "state/WaypointManager.h"
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WaypointManager {
    std::vector<Waypoint> g_waypoints;
    std::set<std::string> g_customFolders;
    std::mutex g_wpMutex;
    Waypoint g_lastDeletedWaypoint;
    bool g_hasDeletedWaypoint = false;
    
    std::string g_worldId = "";
    int g_currentDim = -999; // 玩家当前所处维度 (0=主世界 1=下界 2=末地)

    std::string GenerateID() {
        static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static std::mt19937 rng(std::random_device{}());
        static std::uniform_int_distribution<int> dist(0, (int)(sizeof(alphanum) - 2));
        std::string tmp_s;
        tmp_s.reserve(8);
        for (int i = 0; i < 8; ++i) {
            tmp_s += alphanum[dist(rng)];
        }
        return tmp_s;
    }

    // 生成某维度的路径点存档文件名
    std::string DimensionFile(int dim) {
        return "mods/ChiyanMap/waypoints/" + g_worldId + "_dim" + std::to_string(dim) + ".json";
    }

    // 生成自定义文件夹集合存档文件名
    std::string FoldersFile() {
        return "mods/ChiyanMap/waypoints/" + g_worldId + "_folders.json";
    }

    // 读取单个维度的路径点并标记所属维度
    void LoadDimension(int dim) {
        std::string f = DimensionFile(dim);
        if (!std::filesystem::exists(f)) {
            // 模糊前缀匹配 (应对由于出生点或种子提取延迟导致的文件名微调)
            std::error_code ec;
            std::string prefix = g_worldId.substr(0, g_worldId.find("_S"));
            if (!prefix.empty() && std::filesystem::exists("mods/ChiyanMap/waypoints", ec)) {
                for (const auto& entry : std::filesystem::directory_iterator("mods/ChiyanMap/waypoints", ec)) {
                    if (entry.is_regular_file(ec)) {
                        std::string fname = entry.path().filename().string();
                        if (fname.rfind(prefix, 0) == 0 && fname.find("_dim" + std::to_string(dim) + ".json") != std::string::npos) {
                            f = entry.path().string();
                            break;
                        }
                    }
                }
            }
        }
        if (!std::filesystem::exists(f)) return;
        std::ifstream in(f);
        if (!in.is_open()) return;
        try {
            json j;
            in >> j;
            for (const auto& item : j) {
                Waypoint wp;
                wp.id = item.value("id", GenerateID());
                wp.name = item.value("name", "New Waypoint");
                wp.x = item.value("x", 0);
                wp.y = item.value("y", 0);
                wp.z = item.value("z", 0);
                wp.r = item.value("r", 1.0f);
                wp.g = item.value("g", 1.0f);
                wp.b = item.value("b", 1.0f);
                wp.enabled = item.value("enabled", true);
                wp.dimId = item.value("dimId", dim); // 兼容旧存档
                wp.pinned = item.value("pinned", false);
                wp.folder = item.value("folder", "");
                wp.createdAt = item.value("createdAt", (uint64_t)0);
                wp.order = item.value("order", (int)g_waypoints.size());
                if (!wp.folder.empty()) {
                    g_customFolders.insert(wp.folder);
                }
                g_waypoints.push_back(wp);
            }
        } catch (...) {
            // 防止因 json 损坏导致游戏崩溃
            printf("[ChiyanMap] Waypoints JSON 解析失败！\n");
        }
    }

    void SaveWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        if (g_worldId.empty()) return;
        // 按维度分组后分别写入对应文件
        std::vector<json> groups(3);
        for (int d = 0; d < 3; ++d) groups[d] = json::array();
        for (const auto& wp : g_waypoints) {
            json obj;
            obj["id"] = wp.id;
            obj["name"] = wp.name;
            obj["x"] = wp.x;
            obj["y"] = wp.y;
            obj["z"] = wp.z;
            obj["r"] = wp.r;
            obj["g"] = wp.g;
            obj["b"] = wp.b;
            obj["enabled"] = wp.enabled;
            obj["pinned"] = wp.pinned;
            obj["folder"] = wp.folder;
            obj["createdAt"] = wp.createdAt;
            obj["order"] = wp.order;
            int d = (wp.dimId >= 0 && wp.dimId < 3) ? wp.dimId : g_currentDim;
            if (d >= 0 && d < 3) {
                groups[d].push_back(obj);
            }
        }

        std::filesystem::create_directories("mods/ChiyanMap/waypoints");
        for (int d = 0; d < 3; ++d) {
            std::ofstream out(DimensionFile(d));
            if (out.is_open()) {
                out << groups[d].dump(4); // 格式化为带有 4 个空格缩进的漂亮 JSON
                out.close();
            }
        }

        // 保存独立文件夹配置
        std::set<std::string> allFolders = g_customFolders;
        for (const auto& wp : g_waypoints) {
            if (!wp.folder.empty()) allFolders.insert(wp.folder);
        }
        json fList = json::array();
        for (const auto& f : allFolders) {
            if (!f.empty()) fList.push_back(f);
        }
        std::ofstream fOut(FoldersFile());
        if (fOut.is_open()) {
            fOut << fList.dump(4);
            fOut.close();
        }
    }

    void LoadWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        g_waypoints.clear();
        g_customFolders.clear();

        // 加载文件夹元数据
        std::string fPath = FoldersFile();
        if (!std::filesystem::exists(fPath)) {
            std::error_code ec;
            std::string prefix = g_worldId.substr(0, g_worldId.find("_S"));
            if (!prefix.empty() && std::filesystem::exists("mods/ChiyanMap/waypoints", ec)) {
                for (const auto& entry : std::filesystem::directory_iterator("mods/ChiyanMap/waypoints", ec)) {
                    if (entry.is_regular_file(ec)) {
                        std::string fname = entry.path().filename().string();
                        if (fname.rfind(prefix, 0) == 0 && fname.find("_folders.json") != std::string::npos) {
                            fPath = entry.path().string();
                            break;
                        }
                    }
                }
            }
        }
        if (std::filesystem::exists(fPath)) {
            try {
                std::ifstream fIn(fPath);
                if (fIn.is_open()) {
                    json fList;
                    fIn >> fList;
                    for (const auto& item : fList) {
                        if (item.is_string()) {
                            std::string s = item.get<std::string>();
                            if (!s.empty()) g_customFolders.insert(s);
                        }
                    }
                }
            } catch (...) {}
        }

        // 一次性载入全部三个维度，由 UI 标签筛选显示
        for (int d = 0; d < 3; ++d) LoadDimension(d);
    }

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/waypoints");
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            std::filesystem::create_directories("mods/ChiyanMap/waypoints");
            g_worldId = worldId;
            g_currentDim = dimensionId; // 记录“玩家当前所处维度”，用于新路径点归属
        }
        LoadWaypoints();
    }

    void AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b, int dimId, bool pinned, const std::string& folder) {
        Waypoint wp;
        wp.id = GenerateID();
        wp.name = name;
        wp.x = x; wp.y = y; wp.z = z;
        wp.r = r; wp.g = g; wp.b = b;
        wp.enabled = true;
        wp.dimId = (dimId >= 0 && dimId < 3) ? dimId : g_currentDim; // 默认归属玩家当前维度
        wp.pinned = pinned;
        wp.folder = folder;
        wp.createdAt = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            wp.order = (int)g_waypoints.size();
            if (!folder.empty()) g_customFolders.insert(folder);
            g_waypoints.push_back(wp);
        }
        SaveWaypoints();
    }

    void RemoveWaypoint(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it != g_waypoints.end()) {
                g_lastDeletedWaypoint = *it;
                g_hasDeletedWaypoint = true;
                g_waypoints.erase(it);
                changed = true;
            }
        }
        // 在锁外执行持久化，避免底层死锁
        if (changed) SaveWaypoints();
    }

    void RemoveWaypoints(const std::set<std::string>& ids) {
        if (ids.empty()) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::remove_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) {
                    return std::find(ids.begin(), ids.end(), w.id) != ids.end();
                });
            if (it != g_waypoints.end()) {
                g_waypoints.erase(it, g_waypoints.end());
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    void ToggleWaypoint(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        for (auto& wp : g_waypoints) {
            if (wp.id == id) {
                wp.enabled = !wp.enabled;
                break;
            }
        }
    }

    void ToggleWaypointPin(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            for (auto& wp : g_waypoints) {
                if (wp.id == id) {
                    wp.pinned = !wp.pinned;
                    changed = true;
                    break;
                }
            }
        }
        if (changed) SaveWaypoints();
    }

    void UpdateWaypoint(const std::string& id, const std::string& name, int x, int y, int z, float r, float g, float b, bool enabled, bool pinned, const std::string& folder) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it == g_waypoints.end()) return;
            it->name = name;
            it->x = x; it->y = y; it->z = z;
            it->r = r; it->g = g; it->b = b;
            it->enabled = enabled;
            it->pinned = pinned;
            it->folder = folder;
            if (!folder.empty()) g_customFolders.insert(folder);
        }
        SaveWaypoints();
    }

    bool RestoreLastDeletedWaypoint() {
        if (!g_hasDeletedWaypoint) return false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_waypoints.push_back(g_lastDeletedWaypoint);
            g_hasDeletedWaypoint = false;
        }
        SaveWaypoints();
        return true;
    }

    void SwapWaypointOrder(const std::string& id1, const std::string& id2) {
        if (id1.empty() || id2.empty() || id1 == id2) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it1 = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == id1; });
            auto it2 = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == id2; });
            if (it1 != g_waypoints.end() && it2 != g_waypoints.end()) {
                std::iter_swap(it1, it2);
                for (size_t i = 0; i < g_waypoints.size(); ++i) {
                    g_waypoints[i].order = (int)i;
                }
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    void MoveWaypointToIndex(const std::string& srcId, const std::string& targetId) {
        if (srcId.empty() || targetId.empty() || srcId == targetId) return;
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto itSrc = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == srcId; });
            auto itTgt = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == targetId; });
            if (itSrc != g_waypoints.end() && itTgt != g_waypoints.end()) {
                Waypoint item = *itSrc;
                g_waypoints.erase(itSrc);
                itTgt = std::find_if(g_waypoints.begin(), g_waypoints.end(), [&](const Waypoint& w) { return w.id == targetId; });
                g_waypoints.insert(itTgt, item);
                for (size_t i = 0; i < g_waypoints.size(); ++i) {
                    g_waypoints[i].order = (int)i;
                }
                changed = true;
            }
        }
        if (changed) SaveWaypoints();
    }

    std::vector<std::string> GetFolders() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        std::set<std::string> folderSet = g_customFolders;
        for (const auto& wp : g_waypoints) {
            if (!wp.folder.empty()) folderSet.insert(wp.folder);
        }
        return std::vector<std::string>(folderSet.begin(), folderSet.end());
    }

    void AddFolder(const std::string& folderName) {
        if (folderName.empty()) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.insert(folderName);
        }
        SaveWaypoints();
    }

    void RenameFolder(const std::string& oldName, const std::string& newName) {
        if (oldName.empty() || newName.empty() || oldName == newName) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.erase(oldName);
            g_customFolders.insert(newName);
            for (auto& wp : g_waypoints) {
                if (wp.folder == oldName) {
                    wp.folder = newName;
                }
            }
        }
        SaveWaypoints();
    }

    void DeleteFolder(const std::string& folderName) {
        if (folderName.empty()) return;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_customFolders.erase(folderName);
            for (auto& wp : g_waypoints) {
                if (wp.folder == folderName) {
                    wp.folder = ""; // 移至未分类
                }
            }
        }
        SaveWaypoints();
    }
}
