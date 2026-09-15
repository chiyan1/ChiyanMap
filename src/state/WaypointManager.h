#pragma once
#include <string>
#include <vector>
#include <set>
#include <mutex>
#include <cstdint>
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
    bool pinned = false; // 是否置顶
    std::string folder = ""; // 所属文件夹（空表示未分类）
    uint64_t createdAt = 0;  // 创建时间戳（毫秒）
    int order = 0;           // 手动排序次序

    // 构造函数
    Waypoint() : x(0), y(0), z(0), r(1.f), g(1.f), b(1.f), enabled(true), dimId(0), pinned(false), folder(""), createdAt(0), order(0) {}
};

namespace WaypointManager {
    extern std::vector<Waypoint> g_waypoints;
    extern std::set<std::string> g_customFolders;
    extern std::mutex g_wpMutex;
    extern Waypoint g_lastDeletedWaypoint;
    extern bool g_hasDeletedWaypoint;

    void Init();
    void SaveWaypoints();
    void LoadWaypoints();

    // 增删查改接口
    void AddWaypoint(const std::string& name, int x, int y, int z, float r, float g, float b, int dimId = -1, bool pinned = false, const std::string& folder = "");
    void RemoveWaypoint(const std::string& id);
    void RemoveWaypoints(const std::set<std::string>& ids);
    void ToggleWaypoint(const std::string& id);
    void ToggleWaypointPin(const std::string& id);
    void UpdateWaypoint(const std::string& id, const std::string& name, int x, int y, int z, float r, float g, float b, bool enabled, bool pinned = false, const std::string& folder = "");
    bool RestoreLastDeletedWaypoint();

    // 手动排序接口
    void SwapWaypointOrder(const std::string& id1, const std::string& id2);
    void MoveWaypointToIndex(const std::string& srcId, const std::string& targetId);

    // 文件夹管理接口
    std::vector<std::string> GetFolders();
    void AddFolder(const std::string& folderName);
    void RenameFolder(const std::string& oldName, const std::string& newName);
    void DeleteFolder(const std::string& folderName);

    // [新增] 跨界热重载引擎
    void SwitchWorld(const std::string& worldId, int dimensionId);
}

