#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <semaphore>
#include "state/MapRenderState.h"
#include <mc/deps/core/math/Color.h>

namespace MapCacheManager {
    constexpr int REGION_SIZE = 256;

    // [生物群系单元格] 4x4 方块为一个生物群系单元格 (与基岩版 biome 数据分辨率一致)
    constexpr int BIOME_CELL_SIZE = 4;
    constexpr int BIOME_CELLS_PER_REGION = REGION_SIZE / BIOME_CELL_SIZE; // 64
    constexpr uint8_t BIOME_INDEX_UNKNOWN = 255;
    constexpr int16_t HEIGHT_UNKNOWN = -32000;

    struct RegionData {
        uint8_t colors[REGION_SIZE * REGION_SIZE * 4] = {0};
        int16_t heights[REGION_SIZE * REGION_SIZE] = {0};       // 地表Y缓存, 供传送时查询
        std::vector<std::string> biomeTable;                    // 生物群系名称表 (去重)
        uint8_t biomeCells[BIOME_CELLS_PER_REGION * BIOME_CELLS_PER_REGION] = {0}; // 生物群系索引
        bool dirty = false;
        bool textureDirty = true;
    };

    // [生物群系条目] 扫描时采集, 批量写入缓存
    struct BiomeEntry {
        int cellWorldX;
        int cellWorldZ;
        std::string name;
    };

    // [洞穴隔离] 用最高位(bit63)区分地表/洞穴, 同一区域在两种模式下
    // 使用独立的缓存条目与独立磁盘文件(surface/ 与 cave/), 避免相互污染。
    // rx 被限制为 31 位 (bits 0-30, 符号位=bit30), 支持到 ±2^30 区域坐标,
    // 远超 Minecraft 最大 ±117000 区域需求, 不会与 bit63 冲突。
    inline uint64_t GetRegionHash(int rx, int rz, bool isCave = false) {
        uint64_t h = ((uint64_t)((uint32_t)rx & 0x7FFFFFFFu) << 32) | (uint32_t)rz;
        if (isCave) h |= 0x8000000000000000ULL;
        return h;
    }
    inline bool IsCaveHash(uint64_t h) {
        return (h & 0x8000000000000000ULL) != 0;
    }
    // 从哈希还原 (rx,rz)，并清除洞穴位(bit63)
    inline void DecodeRegionHash(uint64_t h, int& rx, int& rz) {
        uint32_t rx_raw = (uint32_t)((h >> 32) & 0x7FFFFFFFu);
        if (rx_raw & 0x40000000u) rx_raw |= 0x80000000u; // 符号扩展 31→32 位
        rx = (int)(int32_t)rx_raw;
        rz = (int)(int32_t)(uint32_t)(h & 0xFFFFFFFFu);
    }

    extern std::unordered_map<uint64_t, RegionData*> g_loadedRegions;
    extern std::vector<uint64_t> g_loadQueue; // [性能核心] 异步加载请求队列
    extern std::mutex g_cacheMutex;
    extern std::atomic<bool> g_running;
    extern std::thread* g_ioThread;
    extern std::binary_semaphore g_ioDone;
    extern std::string g_cacheDir;

    // 构造 region 文件所在子目录：
    //   - 所有维度的地表 (非 cave): 直接存放在 dim_<n>/ 下 (region_*.bin 位于 dim_<n>/)
    //     主世界历史文件即已按此布局，迁移/读取统一走 dim_<n>/ 以避免旧地表地图失效
    //   - 洞穴: 存放在 dim_<n>/cave/ 下
    inline std::string GetRegionSubdir(bool isCave) {
        return isCave ? "cave/" : "";
    }

    void Init();
    void Shutdown();
    void UpdateFromScan(int centerX, int centerZ, mce::Color scanColors[MAP_DATA_SIZE][MAP_DATA_SIZE], float scanHeights[MAP_DATA_SIZE][MAP_DATA_SIZE], bool isCave = false);
    bool FetchRegionTextureData(uint64_t hash, uint8_t* outBuffer);

    // [新增] 当 GPU 繁忙时，退回纹理更新请求
    void MarkTextureDirty(uint64_t hash);

    // [新增] 跨界热重载引擎
    void SwitchWorld(const std::string& worldId, int dimensionId);

    // [新增] 生物群系缓存写入 (从扫描数据批量写入)
    void UpdateBiomesFromScan(const std::vector<BiomeEntry>& entries);

    // [新增] 地表Y缓存查询 (供传送时使用)
    // isCave=true 时查询洞穴/下界缓存数据 (dim_<n>/cave/)
    int16_t GetCachedSurfaceHeight(int worldX, int worldZ, bool isCave = false);

    // [新增] 生物群系缓存查询 (供大地图悬停显示)
    bool GetCachedBiomeName(int worldX, int worldZ, std::string& outName);
}
