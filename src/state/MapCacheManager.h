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

    inline uint64_t GetRegionHash(int rx, int rz) {
        return ((uint64_t)(uint32_t)rx << 32) | (uint32_t)rz;
    }

    extern std::unordered_map<uint64_t, RegionData*> g_loadedRegions;
    extern std::vector<uint64_t> g_loadQueue; // [性能核心] 异步加载请求队列
    extern std::mutex g_cacheMutex;
    extern std::atomic<bool> g_running;
    extern std::thread* g_ioThread;
    extern std::binary_semaphore g_ioDone;
    extern std::string g_cacheDir;

    void Init();
    void Shutdown();
    void UpdateFromScan(int centerX, int centerZ, mce::Color scanColors[MAP_DATA_SIZE][MAP_DATA_SIZE], float scanHeights[MAP_DATA_SIZE][MAP_DATA_SIZE]);
    bool FetchRegionTextureData(uint64_t hash, uint8_t* outBuffer);

    // [新增] 当 GPU 繁忙时，退回纹理更新请求
    void MarkTextureDirty(uint64_t hash);

    // [新增] 跨界热重载引擎
    void SwitchWorld(const std::string& worldId, int dimensionId);

    // [新增] 生物群系缓存写入 (从扫描数据批量写入)
    void UpdateBiomesFromScan(const std::vector<BiomeEntry>& entries);

    // [新增] 地表Y缓存查询 (供传送时使用)
    int16_t GetCachedSurfaceHeight(int worldX, int worldZ);

    // [新增] 生物群系缓存查询 (供大地图悬停显示)
    bool GetCachedBiomeName(int worldX, int worldZ, std::string& outName);
}
