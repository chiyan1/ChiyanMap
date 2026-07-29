#include "state/WaypointManager.h"
#include <fstream>
#include <filesystem>
#include <random>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WaypointManager {
    std::vector<Waypoint> g_waypoints;
    std::mutex g_wpMutex;
    
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

    // 读取单个维度的路径点并标记所属维度
    void LoadDimension(int dim) {
        std::string f = DimensionFile(dim);
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
                wp.dimId = dim; // 旧存档无 dimId 字段，按文件所属维度补齐
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
            int d = (wp.dimId >= 0 && wp.dimId < 3) ? wp.dimId : g_currentDim;
            groups[d].push_back(obj);
        }

        for (int d = 0; d < 3; ++d) {
            std::ofstream out(DimensionFile(d));
            if (out.is_open()) {
                out << groups[d].dump(4); // 格式化为带有 4 个空格缩进的漂亮 JSON
                out.close();
            }
        }
    }

    void LoadWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        g_waypoints.clear();
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

    void AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b, int dimId) {
        Waypoint wp;
        wp.id = GenerateID();
        wp.name = name;
        wp.x = x; wp.y = y; wp.z = z;
        wp.r = r; wp.g = g; wp.b = b;
        wp.enabled = true;
        wp.dimId = (dimId >= 0 && dimId < 3) ? dimId : g_currentDim; // 默认归属玩家当前维度

        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            g_waypoints.push_back(wp);
        }
        SaveWaypoints();
    }

    void RemoveWaypoint(const std::string& id) {
        bool changed = false;
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::remove_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it != g_waypoints.end()) {
                g_waypoints.erase(it, g_waypoints.end());
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

    void UpdateWaypoint(const std::string& id, const std::string& name, int x, int y, int z, float r, float g, float b, bool enabled) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            auto it = std::find_if(g_waypoints.begin(), g_waypoints.end(),
                [&](const Waypoint& w) { return w.id == id; });
            if (it == g_waypoints.end()) return;
            it->name = name;
            it->x = x; it->y = y; it->z = z;
            it->r = r; it->g = g; it->b = b;
            it->enabled = enabled;
        }
        SaveWaypoints();
    }
}
