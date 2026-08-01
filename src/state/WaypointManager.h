#pragma once
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <mc/deps/core/math/Color.h>

struct Waypoint {
    std::string id;      // 内部唯一 ID
    std::string name;    // 玩家自定义名称
    int x;
    int y;
    int z;
    float r, g, b;       // 颜色 (0.0~1.0)
    bool enabled;        // 是否在地图上显示
    int dimId;           // 维度: 0=主世界 1=下界 2=末地

    // 构造函数
    Waypoint() : x(0), y(0), z(0), r(1.f), g(1.f), b(1.f), enabled(true), dimId(0) {}
};

namespace WaypointManager {
    extern std::vector<Waypoint> g_waypoints;
    extern std::mutex g_wpMutex;
    extern Waypoint g_lastDeletedWaypoint;
    extern bool g_hasDeletedWaypoint;

    void Init();
    void SaveWaypoints();
    void LoadWaypoints();

    // 增删查改接口
    void AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b, int dimId = -1);
    void RemoveWaypoint(const std::string& id);
    void RemoveWaypoints(const std::set<std::string>& ids);
    void ToggleWaypoint(const std::string& id);
    void UpdateWaypoint(const std::string& id, const std::string& name, int x, int y, int z, float r, float g, float b, bool enabled);
    bool RestoreLastDeletedWaypoint();

    // [新增] 跨界热重载引擎
    void SwitchWorld(const std::string& worldId, int dimensionId);
}
