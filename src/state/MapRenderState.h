#pragma once
#include <atomic>
#include <vector>
#include <mc/deps/core/math/Color.h>

namespace MapRenderState {
    inline std::atomic<int> frameCallCount{0};
    inline int lastFrameTotalCalls{1};
    inline bool showBigMap = false; // 大地图开启状态
    inline float bigMapOffsetX = 0.0f;
    inline float bigMapOffsetZ = 0.0f;
    inline float bigMapZoom = 3.0f; // 默认放大 3 倍

    // [微步骤 11] 当前玩家所处生物群系名称
    inline std::string currentBiomeName = "Unknown Biome";

    // [新增] 世界与维度隔离系统状态
    inline std::string currentWorldId = "";
    inline int currentDimensionId = -999;
    inline std::atomic<bool> clearGPUCache{false}; // 用于通知 GPU 清理旧世界的贴图残留

    // [新增] 路径点 UI 开启状态
    inline bool showWaypointUI = false;

    // [统一拦截枢纽] 判断是否有任何全屏 UI 处于活动状态
    inline bool IsUIActive() {
        return showBigMap || showWaypointUI;
    }

    // [新增] 跨菜单桥接：大地图右键唤起新建地标的预设坐标
    inline bool triggerAddWaypoint = false;
    inline int addWaypointX = -999999;
    inline int addWaypointY = -999999;
    inline int addWaypointZ = -999999;

    // [新增] 原生瞬间传送信号器
    inline std::atomic<bool> triggerTeleport{false};
    inline float tpTargetX = 0.0f;
    inline float tpTargetY = 0.0f;
    inline float tpTargetZ = 0.0f;

    inline bool showMiniMap = true;  // 是否显示小地图
    inline bool isSquareMap = false; // 是否为方形小地图
    inline float uiTextScale = 1.0f; // UI 文本缩放比例
    inline float miniMapScale = 1.0f; // 小地图本身大小缩放
}

// 【全球探索级】：匹配 16 区块能见度的究极扫描半径（513x513个方块）！
constexpr int MAP_DATA_RADIUS = 256; 
constexpr int MAP_DATA_SIZE   = 513;

struct RadarEntity {
    float x;
    float y;
    float z;
    int type; 
};

extern std::atomic<bool> g_radarUpdated;
extern std::vector<RadarEntity> g_radarEntities;
inline std::atomic<bool> g_mapDataUpdated{true}; 

// 前台缓冲（仅供显卡渲染读取，绝不闪烁）
extern mce::Color g_mapColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
extern float g_mapHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];

// 后台缓冲（供 CPU 高速扫描写入）
inline mce::Color g_mapColorsBack[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline float g_mapHeightsBack[MAP_DATA_SIZE][MAP_DATA_SIZE];

// 记录最后一次生成贴图时的绝对中心坐标
inline int g_lastRenderX = 0;
inline int g_lastRenderZ = 0;

