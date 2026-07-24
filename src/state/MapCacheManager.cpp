#include "state/MapCacheManager.h"
#include "mod/ChiyanMap.h"
#include "ll/api/coro/SleepAwaiter.h"
#include "ll/api/thread/ThreadPoolExecutor.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace MapCacheManager {
    std::unordered_map<uint64_t, RegionData*> g_loadedRegions;
    std::vector<uint64_t> g_loadQueue;
    std::mutex g_cacheMutex;
    std::atomic<bool> g_running{false};
    // 协程退出同步信号：Init 时为 0（被占用），协程结束时 release；Shutdown 时 acquire 等待退出
    std::binary_semaphore g_ioDone{0};
    
    // 初始化为空，等待进入世界时分配
    std::string g_cacheDir = "";

    // ==========================================
    // [生物群系段读写辅助] 文件格式：colors + heights + biomeCount(1) + biomeTable + biomeCells(4096)
    // 旧文件无此段（fileSize <= colors+heights），新文件包含
    // ==========================================
    static void WriteBiomeSection(std::ofstream& out, const RegionData& region) {
        uint8_t biomeCount = (uint8_t)std::min<size_t>(region.biomeTable.size(), 255);
        out.write((char*)&biomeCount, 1);
        for (uint8_t i = 0; i < biomeCount; i++) {
            const std::string& name = region.biomeTable[i];
            uint8_t len = (uint8_t)std::min<size_t>(name.size(), 255);
            out.write((char*)&len, 1);
            if (len > 0) out.write(name.data(), len);
        }
        out.write((char*)region.biomeCells, sizeof(region.biomeCells));
    }

    static void ReadBiomeSection(std::ifstream& in, RegionData& region, std::streamoff fileSize, std::streamoff sectionStart) {
        if (fileSize <= sectionStart) return;  // 旧文件，无生物群系段
        uint8_t biomeCount = 0;
        in.read((char*)&biomeCount, 1);
        region.biomeTable.clear();
        region.biomeTable.reserve(biomeCount);
        for (uint8_t i = 0; i < biomeCount; i++) {
            uint8_t len = 0;
            in.read((char*)&len, 1);
            std::string name(len, '\0');
            if (len > 0) in.read(&name[0], len);
            region.biomeTable.push_back(std::move(name));
        }
        // 仅当剩余字节足够时读取 biomeCells（防止截断文件）
        std::streamoff curPos = in.tellg();
        if (curPos >= 0 && fileSize - curPos >= (std::streamoff)sizeof(region.biomeCells)) {
            in.read((char*)region.biomeCells, sizeof(region.biomeCells));
        }
    }

    ll::coro::CoroTask<> IOWorkerCoro() {
        auto lastSaveTime = std::chrono::steady_clock::now();
        
        while (g_running) {
            co_await ll::coro::SleepAwaiter{std::chrono::milliseconds(20)};

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

            // 通道 1：极速读取（地表）
            for (uint64_t hash : toLoad) {
                int rx = (int)(hash >> 32); int rz = (int)(hash & 0xFFFFFFFF);
                std::string filePath = currentDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";

                RegionData* newRegion = new RegionData();
                std::ifstream in(filePath, std::ios::binary | std::ios::ate);
                if (in) {
                    auto fileSize = in.tellg();
                    in.seekg(0, std::ios::beg);
                    in.read((char*)newRegion->colors, sizeof(newRegion->colors));
                    // 新格式：colors + heights；旧格式仅 colors（heights 保持 HEIGHT_UNKNOWN）
                    if (fileSize >= (std::streamoff)(sizeof(newRegion->colors) + sizeof(newRegion->heights))) {
                        in.read((char*)newRegion->heights, sizeof(newRegion->heights));
                    }
                    // [新增] 生物群系段（colors+heights 之后）
                    ReadBiomeSection(in, *newRegion, fileSize,
                                     (std::streamoff)(sizeof(newRegion->colors) + sizeof(newRegion->heights)));
                }
                newRegion->textureDirty = true;
                
                {
                    std::lock_guard<std::mutex> lock(g_cacheMutex);
                    if (g_loadedRegions[hash] == nullptr) g_loadedRegions[hash] = newRegion;
                    else delete newRegion; 
                }
            }

            // 通道 2：低频保存（地表）
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
                    if (out) {
                        out.write((char*)item.second.colors, sizeof(item.second.colors));
                        out.write((char*)item.second.heights, sizeof(item.second.heights));
                        // [新增] 生物群系段
                        WriteBiomeSection(out, item.second);
                    }
                }
            }
        }
        co_return;
    }

    void SwitchWorld(const std::string& worldId, int dimensionId) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        
        if (!g_cacheDir.empty()) {
            for (auto& pair : g_loadedRegions) {
                if (pair.second && pair.second->dirty) {
                    int rx = (int)(pair.first >> 32); int rz = (int)(pair.first & 0xFFFFFFFF);
                    std::string filePath = g_cacheDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                    std::ofstream out(filePath, std::ios::binary);
                    if (out) {
                        out.write((char*)pair.second->colors, sizeof(pair.second->colors));
                        out.write((char*)pair.second->heights, sizeof(pair.second->heights));
                        // [新增] 生物群系段
                        WriteBiomeSection(out, *pair.second);
                    }
                }
                if (pair.second) delete pair.second;
            }
        }
        
        g_loadedRegions.clear();
        g_loadQueue.clear();

        // 路径示例：mods/ChiyanMap/data/cache/<worldId>/dim_<n>/
        auto cacheDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir() / "cache" / worldId / ("dim_" + std::to_string(dimensionId));
        std::filesystem::create_directories(cacheDir);
        // 末尾保留分隔符以兼容现有 currentDir + "region_..." 字符串拼接
        g_cacheDir = cacheDir.string() + "/";
    }

    void Init() {
        g_running = true;
        // 在线程池上启动 IO 协程；协程退出时通过 callback 释放信号量
        ll::coro::keepThis(IOWorkerCoro).launch(
            ll::thread::ThreadPoolExecutor::getDefault(),
            [](auto&&) { g_ioDone.release(); }
        );
    }

    void Shutdown() {
        g_running = false;
        // 等待协程退出（最多 20ms 一次循环 + 一次 IO 处理时间）
        g_ioDone.acquire();

        // [持久化] 游戏关闭时保存所有脏区域 (含下界/末地), 避免退出时数据丢失
        if (!g_cacheDir.empty()) {
            for (auto& pair : g_loadedRegions) {
                if (pair.second && pair.second->dirty) {
                    int rx = (int)(pair.first >> 32); int rz = (int)(pair.first & 0xFFFFFFFF);
                    std::string filePath = g_cacheDir + "region_" + std::to_string(rx) + "_" + std::to_string(rz) + ".bin";
                    std::ofstream out(filePath, std::ios::binary);
                    if (out) {
                        out.write((char*)pair.second->colors, sizeof(pair.second->colors));
                        out.write((char*)pair.second->heights, sizeof(pair.second->heights));
                        WriteBiomeSection(out, *pair.second);
                    }
                }
            }
        }

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
                    std::ifstream in(filePath, std::ios::binary | std::ios::ate);
                    if (in) {
                        auto fileSize = in.tellg();
                        in.seekg(0, std::ios::beg);
                        in.read((char*)newRegion->colors, sizeof(newRegion->colors));
                        if (fileSize >= (std::streamoff)(sizeof(newRegion->colors) + sizeof(newRegion->heights))) {
                            in.read((char*)newRegion->heights, sizeof(newRegion->heights));
                        }
                        // [新增] 生物群系段（colors+heights 之后）
                        ReadBiomeSection(in, *newRegion, fileSize,
                                         (std::streamoff)(sizeof(newRegion->colors) + sizeof(newRegion->heights)));
                    }
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

                // [地表Y缓存] 记录此列的地表高度，供未加载区域传送时查询
                int heightIndex = localZ * REGION_SIZE + localX;
                region->heights[heightIndex] = (int16_t)std::clamp(currentY, -64.0f, 320.0f);

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

    int16_t GetCachedSurfaceHeight(int worldX, int worldZ) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        int rx = (worldX < 0 ? (worldX + 1) / REGION_SIZE - 1 : worldX / REGION_SIZE);
        int rz = (worldZ < 0 ? (worldZ + 1) / REGION_SIZE - 1 : worldZ / REGION_SIZE);
        uint64_t hash = GetRegionHash(rx, rz);

        auto it = g_loadedRegions.find(hash);
        if (it == g_loadedRegions.end()) {
            // 区域未加载 → 排队异步加载（与 FetchRegionTextureData 一致的行为）
            g_loadedRegions[hash] = nullptr;
            g_loadQueue.push_back(hash);
            return HEIGHT_UNKNOWN;
        }
        if (it->second == nullptr) {
            return HEIGHT_UNKNOWN; // 已排队但尚未加载完成
        }

        RegionData* region = it->second;
        int localX = worldX - (rx * REGION_SIZE);
        int localZ = worldZ - (rz * REGION_SIZE);
        return region->heights[localZ * REGION_SIZE + localX];
    }

    // ==========================================
    // [生物群系缓存写入] 从扫描数据批量写入 biomeCells + biomeTable
    // 线程安全：单次锁获取，处理所有条目
    // ==========================================
    void UpdateBiomesFromScan(const std::vector<BiomeEntry>& entries) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        if (g_cacheDir.empty()) return;
        for (const auto& e : entries) {
            // 世界单元格坐标 → region 坐标
            int blockX = e.cellWorldX * BIOME_CELL_SIZE;
            int blockZ = e.cellWorldZ * BIOME_CELL_SIZE;
            int rx = (blockX < 0 ? (blockX + 1) / REGION_SIZE - 1 : blockX / REGION_SIZE);
            int rz = (blockZ < 0 ? (blockZ + 1) / REGION_SIZE - 1 : blockZ / REGION_SIZE);
            uint64_t hash = GetRegionHash(rx, rz);

            // 确保 region 在内存中（若未加载则跳过，下次扫描会补）
            auto it = g_loadedRegions.find(hash);
            if (it == g_loadedRegions.end() || it->second == nullptr) continue;

            RegionData* region = it->second;
            int localCellX = (blockX - rx * REGION_SIZE) / BIOME_CELL_SIZE;
            int localCellZ = (blockZ - rz * REGION_SIZE) / BIOME_CELL_SIZE;
            if (localCellX < 0 || localCellX >= BIOME_CELLS_PER_REGION ||
                localCellZ < 0 || localCellZ >= BIOME_CELLS_PER_REGION) continue;

            // 查找或插入 biomeTable
            uint8_t idx = BIOME_INDEX_UNKNOWN;
            for (size_t i = 0; i < region->biomeTable.size() && i < 255; i++) {
                if (region->biomeTable[i] == e.name) { idx = (uint8_t)i; break; }
            }
            if (idx == BIOME_INDEX_UNKNOWN) {
                if (region->biomeTable.size() >= 255) continue; // 表满，跳过（极少见）
                region->biomeTable.push_back(e.name);
                idx = (uint8_t)(region->biomeTable.size() - 1);
            }
            region->biomeCells[localCellZ * BIOME_CELLS_PER_REGION + localCellX] = idx;
            region->dirty = true;
        }
    }

    // ==========================================
    // [生物群系缓存查询] 供大地图悬停显示调用
    // 已扫描区域即使区块卸载也能命中（核心修复点）
    // ==========================================
    bool GetCachedBiomeName(int worldX, int worldZ, std::string& outName) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        int rx = (worldX < 0 ? (worldX + 1) / REGION_SIZE - 1 : worldX / REGION_SIZE);
        int rz = (worldZ < 0 ? (worldZ + 1) / REGION_SIZE - 1 : worldZ / REGION_SIZE);
        uint64_t hash = GetRegionHash(rx, rz);

        auto it = g_loadedRegions.find(hash);
        if (it == g_loadedRegions.end()) {
            // 区域未加载 → 排队异步加载，下次查询可命中
            g_loadedRegions[hash] = nullptr;
            g_loadQueue.push_back(hash);
            return false;
        }
        if (it->second == nullptr) return false;  // 已排队但尚未加载完成

        RegionData* region = it->second;
        int localCellX = (worldX - rx * REGION_SIZE) / BIOME_CELL_SIZE;
        int localCellZ = (worldZ - rz * REGION_SIZE) / BIOME_CELL_SIZE;
        if (localCellX < 0 || localCellX >= BIOME_CELLS_PER_REGION ||
            localCellZ < 0 || localCellZ >= BIOME_CELLS_PER_REGION) return false;

        uint8_t idx = region->biomeCells[localCellZ * BIOME_CELLS_PER_REGION + localCellX];
        if (idx == BIOME_INDEX_UNKNOWN || idx >= region->biomeTable.size()) return false;
        outName = region->biomeTable[idx];
        return !outName.empty();
    }
}
