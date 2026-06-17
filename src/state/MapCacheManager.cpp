#include "state/MapCacheManager.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace MapCacheManager {
    std::unordered_map<uint64_t, RegionData*> g_loadedRegions;
    std::vector<uint64_t> g_loadQueue;
    std::mutex g_cacheMutex;
    std::atomic<bool> g_running{false};
    std::thread g_ioThread;
    
    // 初始化为空，等待进入世界时分配
    std::string g_cacheDir = "";

    void IOWorker() {
        auto lastSaveTime = std::chrono::steady_clock::now();
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));

            std::vector<uint64_t> toLoad;
            std::string currentDir;
            {
                std::lock_guard<std::mutex> lock(g_cacheMutex);
                if (g_cacheDir.empty()) continue; // 没有世界路径时挂起
                currentDir = g_cacheDir;
                if (!g_loadQueue.empty()) {
                    toLoad = g_loadQueue;
                    g_loadQueue.clear();
                }
            }

            // 通道 1：极速读取
            for (uint64_t hash : toLoad) {
                int rx = (int)(hash >> 32); int rz = (int)(hash & 0xFFFFFFFF);
                std::string filePath = currentDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                
                RegionData* newRegion = new RegionData();
                std::ifstream in(filePath, std::ios::binary);
                if (in) in.read((char*)newRegion->colors, sizeof(newRegion->colors));
                newRegion->textureDirty = true; 
                
                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    if (g_loadedRegions[hash] == nullptr) g_loadedRegions[hash] = newRegion;
                    else delete newRegion; 
                }
            }

            // 通道 2：低频保存
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - lastSaveTime).count() >= 2) {
                lastSaveTime = now;
                std::vector<std::pair<uint64_t, RegionData>> regionsToSave;
                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    for (auto& pair : g_loadedRegions) {
                        if (pair.second && pair.second->dirty) {
                            regionsToSave.push_back({pair.first, *pair.second});
                            pair.second->dirty = false;
                        }
                    }
                }
                for (auto& item : regionsToSave) {
                    int rx = (int)(item.first >> 32); int rz = (int)(item.first & 0xFFFFFFFF);
                    std::string filePath = currentDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                    std::ofstream out(filePath, std::ios::binary);
                    if (out) out.write((char*)item.second.colors, sizeof(item.second.colors));
                }
            }
        }
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        
        if (!g_cacheDir.empty()) {
            for (auto& pair : g_loadedRegions) {
                if (pair.second && pair.second->dirty) {
                    int rx = (int)(pair.first >> 32); int rz = (int)(pair.first & 0xFFFFFFFF);
                    std::string filePath = g_cacheDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                    std::ofstream out(filePath, std::ios::binary);
                    if (out) out.write((char*)pair.second->colors, sizeof(pair.second->colors));
                }
                if (pair.second) delete pair.second;
            }
        }
        
        g_loadedRegions.clear();
        g_loadQueue.clear();

        g_cacheDir = "mods/ChiyanMap/cache/" + worldId + "/dim_" + std::to_string(dimensionId) + "/";
        std::filesystem::create_directories(g_cacheDir);
    }

    void Init() {
        g_running = true;
        g_ioThread = std::thread(IOWorker);
    }

    void Shutdown() {
        g_running = false;
        if (g_ioThread.joinable()) g_ioThread.join();
        for (auto& pair : g_loadedRegions) if(pair.second) delete pair.second;
        g_loadedRegions.clear();
        g_loadQueue.clear();
    }

    void UpdateFromScan(int centerX, int centerZ, mce::Color scanColors[MAP_DATA_SIZE][MAP_DATA_SIZE], float scanHeights[MAP_DATA_SIZE][MAP_DATA_SIZE]) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheDir.empty()) return; // 未进世界前禁止写入
        int startX = centerX - MAP_DATA_RADIUS; int startZ = centerZ - MAP_DATA_RADIUS;

        for (int z = 0; z < MAP_DATA_SIZE; z++) {
            for (int x = 0; x < MAP_DATA_SIZE; x++) {
                mce::Color c = scanColors[x][z];
                if (c.a <= 0.01f) continue;

                int worldX = startX + x; int worldZ = startZ + z;
                int rx = (worldX < 0 ? (worldX + 1) / REGION_SIZE - 1 : worldX / REGION_SIZE);
                int rz = (worldZ < 0 ? (worldZ + 1) / REGION_SIZE - 1 : worldZ / REGION_SIZE);
                uint64_t hash = GetRegionHash(rx, rz);
                
                if (g_loadedRegions.find(hash) == g_loadedRegions.end() || g_loadedRegions[hash] == nullptr) {
                    RegionData* newRegion = new RegionData();
                    std::string filePath = g_cacheDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                    std::ifstream in(filePath, std::ios::binary);
                    if (in) in.read((char*)newRegion->colors, sizeof(newRegion->colors));
                    newRegion->textureDirty = true;
                    g_loadedRegions[hash] = newRegion;
                }

                RegionData* region = g_loadedRegions[hash];
                int localX = worldX - (rx * REGION_SIZE); int localZ = worldZ - (rz * REGION_SIZE);

                float currentY = scanHeights[x][z];
                int index = (localZ * REGION_SIZE + localX) * 4;

                float northY = currentY;
                if (z > 0 && scanColors[x][z - 1].a > 0.01f) {
                    float h = scanHeights[x][z - 1];
                    if (std::abs(currentY - h) < 64.0f) northY = h;
                }
                
                float westY = currentY;
                if (x > 0 && scanColors[x - 1][z].a > 0.01f) {
                    float h = scanHeights[x - 1][z];
                    if (std::abs(currentY - h) < 64.0f) westY = h;
                }

                float diff = (currentY - northY) * 0.15f + (currentY - westY) * 0.15f;
                float shade = std::clamp(1.0f + diff, 0.65f, 1.25f);

                region->colors[index + 0] = (uint8_t)(std::clamp(c.r * shade, 0.0f, 1.0f) * 255.0f);
                region->colors[index + 1] = (uint8_t)(std::clamp(c.g * shade, 0.0f, 1.0f) * 255.0f);
                region->colors[index + 2] = (uint8_t)(std::clamp(c.b * shade, 0.0f, 1.0f) * 255.0f);
                region->colors[index + 3] = (uint8_t)(c.a * 255.0f);
                
                region->dirty = true;
                region->textureDirty = true;
            }
        }
    }

    bool FetchRegionTextureData(uint64_t hash, uint8_t* outBuffer) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_loadedRegions.find(hash);
        if (it == g_loadedRegions.end()) {
            g_loadedRegions[hash] = nullptr;
            g_loadQueue.push_back(hash);
            return false;
        } else {
            RegionData* region = it->second;
            if (region && region->textureDirty) {
                std::memcpy(outBuffer, region->colors, REGION_SIZE * REGION_SIZE * 4);
                region->textureDirty = false;
                return true;
            }
        }
        return false;
    }

    void MarkTextureDirty(uint64_t hash) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        auto it = g_loadedRegions.find(hash);
        if (it != g_loadedRegions.end() && it->second) it->second->textureDirty = true;
    }
}
