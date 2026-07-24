#pragma once
#include <atomic>
#include <chrono>
#include <vector>
#include <mc/deps/core/math/Color.h>

namespace MapRenderState {
    inline std::atomic<int> frameCallCount{0};
    inline int lastFrameTotalCalls{1};
    inline bool showBigMap = false; // 大地图开启状态
    inline float bigMapOffsetX = 0.0f;
    inline float bigMapOffsetZ = 0.0f;
    inline float bigMapZoom = 3.0f; // 默认放大 3 倍

    // 当前玩家所处生物群系名称 (拆分为原始命名空间ID与本地化名称)
    inline std::string rawBiomeName = "minecraft:unknown";
    inline std::string translatedBiomeName = "Unknown Biome";

    // [大地图鼠标悬停生物群系] 独立于玩家当前生物群系，不影响小地图
    inline std::string hoverBiomeRawName = "";          // 鼠标位置原始生物群系名
    inline std::string hoverBiomeTranslatedName = "";   // 鼠标位置翻译后生物群系名
    inline float hoverBiomeAlpha = 0.0f;                 // 当前显示透明度 (0=隐藏, 1=完全显示)
    inline float hoverBiomeTargetAlpha = 0.0f;           // 目标透明度
    inline int hoverBiomeFrameCounter = 0;               // 节流帧计数器
    inline float hoverBiomeLastQueryX = 0.0f;            // 上次查询的世界 X
    inline float hoverBiomeLastQueryZ = 0.0f;            // 上次查询的世界 Z
    inline bool hoverBiomeHasValidResult = false;        // 是否有有效查询结果

    // [新增] 世界与维度隔离系统状态
    inline std::string currentWorldId = "";
    inline int currentDimensionId = -999;
    inline std::atomic<bool> clearGPUCache{false}; // 用于通知 GPU 清理旧世界的贴图残留

    // [新增] 路径点 UI 开启状态
    inline bool showWaypointUI = false;

    // [新增] 小地图路径点显示开关 (独立于路径点 enabled 属性，控制是否在小地图上绘制)
    inline bool showWaypointsOnMinimap = true;

    // [新增] 快捷键设置面板开启状态
    inline bool showHotkeySettings = false;
    inline bool showCaveSettings = false; // [洞穴地图] 洞穴设置面板

    // [新增] 快捷键绑定结构 (虚拟键码，参考 Win32 VK_*)
    // 默认值: M=0x4D, U=0x55, N=0x4E, Y=0x59, J=0x4A
    // 0 表示已禁用 (清除设置)，不匹配任何 WM_KEYDOWN 的 wParam
    struct HotkeyBindings {
        int openBigMap        = 0x4D; // M: 切换大地图
        int openWaypointMgr   = 0x55; // U: 切换路径点管理器
        int toggleMinimap     = 0x4E; // N: 切换小地图显示
        int toggleMinimapShape= 0x59; // Y: 切换小地图形状
        int toggleMinimapRot  = 0x4A; // J: 切换小地图旋转

        // 系统预设默认值 (单一来源，供"单行重置"与"全部重置"共用)
        static constexpr HotkeyBindings Defaults() {
            return { 0x4D, 0x55, 0x4E, 0x59, 0x4A };
        }
    };
    inline HotkeyBindings g_hotkeys;
    // 正在监听按键的重绑目标 (nullptr=未在监听)
    inline int* g_listeningHotkey = nullptr;
    // [快捷键增强] Ctrl+Z 撤销请求标志 (由 WndProc 设置，渲染线程消费)
    inline std::atomic<bool> g_hotkeyUndoRequested{false};

    // [新增] 小地图偏移与位置设置
    inline float miniMapOffsetX = 0.0f; // 小地图 X 偏移
    inline float miniMapOffsetY = 0.0f; // 小地图 Y 偏移
    inline bool showMiniMapPosSettings = false; // 小地图位置设置面板

    // [统一拦截枢纽] 判断是否有任何全屏 UI 处于活动状态
    inline bool IsUIActive() {
        return showBigMap || showWaypointUI || showMiniMapPosSettings || showHotkeySettings || showCaveSettings;
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

    // [两阶段地表探测] 未访问区域的分阶段传送状态
    // Phase 0: 已 tp 到 Y=320 触发服务器下发目标区块，等待客户端区块就绪
    // Phase 1: 区块就绪后 re-tp 到实际地表（/tp 重置坠落距离，无需缓降）
    // 仅在缓存与实时探测均未命中（玩家从未到访）时启用
    inline std::atomic<bool> pendingSurfaceProbe{false};
    inline std::atomic<int> probeTargetX{0};
    inline std::atomic<int> probeTargetZ{0};
    inline float probeOriginalX = 0.0f;  // 探测超时回退坐标
    inline float probeOriginalY = 0.0f;
    inline float probeOriginalZ = 0.0f;
    inline std::chrono::steady_clock::time_point probeStartTime;

    // [维度感知传送] 探测模式: 0=地表(主世界非洞穴), 1=洞穴/下界, 2=末地
    // Phase 1 轮询时根据此值选择 SafeFindSafeSpawnY(地表) 或 SafeFindSafeSpawnYNearY(洞穴/下界/末地)
    inline int probeMode = 0;

    // [Y范围限制] 探测时 SafeFindSafeSpawnYNearY 的扫描范围
    // 下界 [2,125] 避开基岩层; 洞穴 [refY-48, refY+48] 避免穿过天花板到地表
    inline int probeMinY = -64;
    inline int probeMaxY = 319;

    // [探测参考Y] Phase 1 搜索的起始Y参考值
    // 下界=125(从顶部往下搜); 洞穴/末地=玩家原始Y
    inline int probeRefY = 64;

    // [安全传送增强] 探测稳定性检查：连续 N 帧返回相同 Y 才视为"区块加载完成"
    // 防止部分加载区块返回临时错误 Y（如地下结构）导致地下传送
    // v2: 从 2 帧升级到 3 帧，进一步过滤部分加载区块的"稳定但错误"Y 值
    inline short probeLastY = -32000;         // 上一帧探测到的 Y
    inline int  probeStableCount = 0;         // 连续一致帧数
    constexpr static int kProbeStableThreshold = 3;  // 需要的连续一致帧数（v2: 2→3）

    // [传送状态机] 用于 UI 加载提示与异常反馈
    // Idle: 无传送 / Loading: 等待区块加载 / Validating: 验证地表 / Failed: 异常回退
    enum class TeleportState : int { Idle = 0, Loading = 1, Validating = 2, Done = 3, Failed = 4 };
    inline std::atomic<int> teleportState{(int)TeleportState::Idle};
    inline std::string teleportStatusMsg;     // 给 UI 显示的简短状态文本（如"加载地形中..."）
    inline std::string teleportFailReason;    // 失败原因（供日志与调试）

    inline bool showMiniMap = true;  // 是否显示小地图
    inline bool isSquareMap = false; // 是否为方形小地图
    inline bool rotateMiniMap = false; // 小地图跟随视角旋转
    inline float uiTextScale = 1.0f; // UI 文本缩放比例
    inline float miniMapScale = 1.0f; // 小地图本身大小缩放
    inline float miniMapZoomRadius = 50.0f; // 小地图可视范围（玩家周围方块半径，10-200）

    // [关闭期安全标志] disable() 入口处置 true，所有钩子和后台线程在入口检查此标志并提前返回，
    // 防止进程退出阶段访问已释放的 D3D/ImGui 资源导致 0xC0000005 退出崩溃
    inline std::atomic<bool> g_isShuttingDown{false};

    // ==================== 洞穴地图系统 (Xaero's Cave Map 1:1 复刻) ====================
    // 洞穴模式类型 (对应 Xaero's DEFAULT_CAVE_MODE_TYPE)
    // 0=Off(关闭), 1=Layered(分层)
    enum class CaveModeType : int { Off = 0, Layered = 1 };

    // 洞穴模式设置 (持久化到 config) — 对齐 Xaero's 设置面板
    inline int g_caveModeType = (int)CaveModeType::Layered;  // 默认 Layered (Xaero 默认值)
    inline bool g_caveTopYAuto = true;                        // Cave Mode Top Y: auto=自动检测, false=手动指定
    inline int g_caveTopY = 64;                               // Top Y: 手动模式下的洞穴顶层Y (默认 64=海平面, 有效范围 [-64,320])
    inline int g_caveDepth = 30;                               // 洞穴扫描深度 (Xaero: 1-64, 默认 30)
    inline bool g_legibleCaveMaps = false;                     // 清晰洞穴地图 (Xaero: 默认 false)

    // 洞穴模式运行时状态 (每帧计算)
    inline bool g_caveModeActive = false;       // 当前是否处于洞穴模式 (玩家在地下)
    inline int g_caveStartY = 0;                // 当前洞穴起始 Y (扫描顶部)
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

// [洞穴地图] 洞穴模式前台缓冲 (渲染线程读取)
extern mce::Color g_caveColors[MAP_DATA_SIZE][MAP_DATA_SIZE];
extern float g_caveHeights[MAP_DATA_SIZE][MAP_DATA_SIZE];

// 记录最后一次生成贴图时的绝对中心坐标
inline int g_lastRenderX = 0;
inline int g_lastRenderZ = 0;

