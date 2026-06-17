#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include "state/MapRenderState.h"
#include <mc/deps/core/math/Color.h>

namespace MapCacheManager {
    constexpr int REGION_SIZE = 256;

    struct RegionData {
        uint8_t colors[REGION_SIZE * REGION_SIZE * 4] = {0}; 
        bool dirty = false; 
        bool textureDirty = true;
    };

    inline uint64_t GetRegionHash(int rx, int rz) {
        return ((uint64_t)(uint32_t)rx << 32) | (uint32_t)rz;
    }

    extern std::unordered_map<uint64_t, RegionData*> g_loadedRegions;
    extern std::vector<uint64_t> g_loadQueue; // [性能核心] 异步加载请求队列
    extern std::mutex g_cacheMutex;
    extern std::atomic<bool> g_running;
    extern std::thread g_ioThread;

    void Init();
    void Shutdown();
    void UpdateFromScan(int centerX, int centerZ, mce::Color scanColors[MAP_DATA_SIZE][MAP_DATA_SIZE], float scanHeights[MAP_DATA_SIZE][MAP_DATA_SIZE]);
    bool FetchRegionTextureData(uint64_t hash, uint8_t* outBuffer);
    
    // [新增] 当 GPU 繁忙时，退回纹理更新请求
    void MarkTextureDirty(uint64_t hash);
    
    // [新增] 跨界热重载引擎
    void SwitchWorld(const std::string& worldId, int dimensionId);
}