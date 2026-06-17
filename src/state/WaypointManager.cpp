#include "state/WaypointManager.h"
#include <fstream>
#include <filesystem>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace WaypointManager {
    std::vector<Waypoint> g_waypoints;
    std::mutex g_wpMutex;
    
    std::string g_waypointFile = "";

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

    void SaveWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        if (g_waypointFile.empty()) return;
        json j = json::array();
        
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
            j.push_back(obj);
        }

        std::ofstream out(g_waypointFile);
        if (out.is_open()) {
            out << j.dump(4); // 格式化为带有 4 个空格缩进的漂亮 JSON
            out.close();
        }
    }

    void LoadWaypoints() {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        g_waypoints.clear();
        if (g_waypointFile.empty()) return;

        if (!std::filesystem::exists(g_waypointFile)) return;

        std::ifstream in(g_waypointFile);
        if (in.is_open()) {
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
                    g_waypoints.push_back(wp);
                }
            } catch (...) {
                // 防止因 json 损坏导致游戏崩溃
                printf("[ChiyanMap] Waypoints JSON 解析失败！\n");
            }
        }
    }

    void Init() {
        std::filesystem::create_directories("mods/ChiyanMap/waypoints");
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        {
            std::lock_guard<std::mutex> lock(g_wpMutex);
            std::filesystem::create_directories("mods/ChiyanMap/waypoints");
            g_waypointFile = "mods/ChiyanMap/waypoints/" + worldId + "_dim" + std::to_string(dimensionId) + ".json";
        }
        LoadWaypoints();
    }

    void AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b) {
        Waypoint wp;
        wp.id = GenerateID();
        wp.name = name;
        wp.x = x; wp.y = y; wp.z = z;
        wp.r = r; wp.g = g; wp.b = b;
        wp.enabled = true;
        
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

    void ToggleWaypoint(const std::string& id) {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        for (auto& wp : g_waypoints) {
            if (wp.id == id) {
                wp.enabled = !wp.enabled;
                break;
            }
        }
    }
}
