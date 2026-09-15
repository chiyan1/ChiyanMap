#pragma once
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <ll/api/memory/Hook.h>
#include <mc/client/game/ClientInstance.h>
#include <mc/client/player/LocalPlayer.h>
#include <mc/world/actor/player/Player.h>
#include <mc/deps/core/math/Color.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/biome/Biome.h>
#include <mc/world/level/material/Material.h>
#if __has_include(<mc/world/level/material/MaterialType.h>)
#include <mc/world/level/material/MaterialType.h>
using ChiyanMapMaterialType = ::MaterialType;
#else
#include <mc/deps/shared_types/v1_26_20/block/MaterialType.h>
using ChiyanMapMaterialType = ::SharedTypes::v1_26_20::MaterialType;
#endif
#include "state/WaypointManager.h"
#include <mc/world/level/BlockPos.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/level/Level.h>
#include <mc/world/actor/Actor.h>
#include <mc/world/effect/MobEffectInstance.h>
#include <mc/server/commands/CommandContext.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/PlayerCommandOrigin.h>
#include <mc/server/commands/CurrentCmdVersion.h>
#include <mc/network/packet/CommandRequestPacket.h>
#include <mc/network/packet/CommandRequestPacketPayload.h>
#include <mc/network/Packet.h>
#include <mc/world/actor/ActorHurtResult.h>
#include <mc/world/actor/ActorCategory.h>
#include <mc/world/gamemode/GameMode.h>
#include <mc/world/gamemode/InteractionResult.h>
#include <mc/world/actor/Mob.h>
#include <mc/world/actor/ActorEvent.h>
#include <mc/world/actor/player/PlayerInventory.h>
#include <mc/world/actor/player/Inventory.h>
#include <mc/world/item/ItemStack.h>
#include <mc/deps/core/math/Vec2.h>
#include <mc/world/phys/HitResult.h>
#include "state/MapRenderState.h"
#include "state/MapCacheManager.h"
#include "state/LanguageManager.h"
#include "mod/ChiyanMap.h"

extern float g_playerX;
extern float g_playerY;
extern float g_playerZ;
extern float g_playerYaw;
extern bool  g_hasPlayer;
extern LocalPlayer* g_localPlayer;
extern ClientInstance* g_clientInstance;
extern Vec3 g_prevPhysicsPos;
extern Vec3 g_currPhysicsPos;
extern std::chrono::steady_clock::time_point g_lastPhysicsTime;
extern int g_playerBlockX;
extern int g_playerBlockZ;

// 数据并发锁，消灭画面撕裂
inline std::mutex g_mapDataMutex;

// [性能] 持久化缓存写入线程：原实现每次扫描完成时 std::thread().detach() 创建新线程
// + new ColorGrid/HeightGrid (5MB 堆分配)。改为持久化 worker + 预分配缓冲 + 条件变量，
// 消除线程创建开销和 5MB 堆分配/释放开销。
using ColorGrid = mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
using HeightGrid = float[MAP_DATA_SIZE][MAP_DATA_SIZE];
inline std::mutex g_cacheWriteMutex;
inline std::condition_variable g_cacheWriteCV;
inline std::atomic<bool> g_cacheWritePending{false};
inline std::atomic<bool> g_cacheWriteExit{false};
inline std::thread* g_cacheWriteThread = nullptr;
// 预分配缓冲 (仅 worker 线程访问，无需加锁)
inline mce::Color (*g_cacheWriteColors)[MAP_DATA_SIZE] = nullptr;
inline float (*g_cacheWriteHeights)[MAP_DATA_SIZE] = nullptr;
// 请求参数 (由扫描线程写入，worker 线程读取，通过 g_cacheWritePending 标志同步)
inline int g_cacheWriteX = 0;
inline int g_cacheWriteZ = 0;
inline int g_cacheWriteDim = 0;
inline bool g_cacheWriteIsCave = false;
inline std::vector<MapCacheManager::BiomeEntry> g_cacheWriteBiomes;

inline void CacheWriteWorkerFunc() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(g_cacheWriteMutex);
            g_cacheWriteCV.wait(lock, [] { return g_cacheWritePending.load() || g_cacheWriteExit.load(); });
            if (g_cacheWriteExit.load()) return;
        }

        if (MapRenderState::g_isShuttingDown.load()) {
            g_cacheWritePending.store(false);
            continue;
        }
        if (MapRenderState::currentDimensionId != g_cacheWriteDim) {
            g_cacheWritePending.store(false);
            continue;
        }

        // [修复] 包裹 try-catch 防止 UpdateFromScan/UpdateBiomesFromScan 抛异常
        // 导致 std::terminate → 0xC0000409 FAST_FAIL_FATAL_APP_EXIT
        try {
            MapCacheManager::UpdateFromScan(g_cacheWriteX, g_cacheWriteZ, g_cacheWriteColors, g_cacheWriteHeights, g_cacheWriteIsCave);
            MapCacheManager::UpdateBiomesFromScan(g_cacheWriteBiomes);
        } catch (...) {
            // 异常时仍需复位状态，避免 pending 永远卡住
        }
        g_cacheWriteBiomes.clear();
        g_cacheWritePending.store(false);
    }
}

// 提交缓存写入请求 (非阻塞，若 worker 仍在处理上一次请求则跳过)
inline void SubmitCacheWrite(int x, int z, int dim, bool isCave, std::vector<MapCacheManager::BiomeEntry>& biomes) {
    if (g_cacheWritePending.load()) return;  // worker 忙，跳过 (数据已在前台缓冲，下次扫描会再写)

    // 懒启动持久化 worker 线程
    if (!g_cacheWriteThread) {
        g_cacheWriteColors = new mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
        g_cacheWriteHeights = new float[MAP_DATA_SIZE][MAP_DATA_SIZE];
        g_cacheWriteThread = new std::thread(CacheWriteWorkerFunc);
    }

    {
        std::lock_guard<std::mutex> lock(g_mapDataMutex);
        std::memcpy(g_cacheWriteColors, g_mapColorsBack, sizeof(g_mapColorsBack));
        std::memcpy(g_cacheWriteHeights, g_mapHeightsBack, sizeof(g_mapHeightsBack));
    }
    g_cacheWriteX = x;
    g_cacheWriteZ = z;
    g_cacheWriteDim = dim;
    g_cacheWriteIsCave = isCave;
    g_cacheWriteBiomes.swap(biomes);
    g_cacheWritePending.store(true);
    g_cacheWriteCV.notify_one();
}

inline void ShutdownCacheWriteThread() {
    if (g_cacheWriteThread) {
        g_cacheWriteExit.store(true);
        g_cacheWriteCV.notify_one();
        if (g_cacheWriteThread->joinable()) g_cacheWriteThread->join();
        delete g_cacheWriteThread;
        g_cacheWriteThread = nullptr;
        g_cacheWriteExit.store(false);
        g_cacheWritePending.store(false);
    }
    if (g_cacheWriteColors) { delete[] g_cacheWriteColors; g_cacheWriteColors = nullptr; }
    if (g_cacheWriteHeights) { delete[] g_cacheWriteHeights; g_cacheWriteHeights = nullptr; }
}

// ==========================================
// [跑图跟随] 扫描窗口平移工具
// 根因: 513×513 整幅扫描受每 tick 500μs 预算限制, 完整扫完需数十秒,
//   玩家持续移动时已完成扫描的中心大幅滞后 (跑图时可滞后 150~300 格),
//   小地图 UV 采样窗口越出纹理边界, POINT+CLAMP 寻址把纹理边缘行/列
//   拉伸成"东西向移动→横条纹、南北向移动→竖条纹"的整屏伪影。
// 方案: 扫描进行中玩家偏离扫描中心 ≥16 格时, 将后台缓冲整体平移(保留已扫数据),
//   仅新暴露的边缘条带置零等待后续扫描, 游标同步换算到新中心坐标系,
//   扫描无缝衔接, 扫描中心始终贴近玩家。
//   注: 平移后尾侧暴露条带 (约16格) 位于玩家行进反方向 240+ 格外,
//   超出小地图任何视野范围 (zoom 上限 200); 且 UpdateFromScan 跳过
//   alpha=0 列, 不会用缺口覆盖缓存中已有的已访问区域数据。
// ==========================================
template <typename T>
inline void ShiftScanGrid(T (&grid)[MAP_DATA_SIZE][MAP_DATA_SIZE], int shiftX, int shiftZ) noexcept {
    constexpr int N = MAP_DATA_SIZE;
    // X 方向: 整行平移 (grid 按 [arrX][arrZ] 排布, arrX 是行索引)
    // 玩家向东移动 (shiftX>0): 新[arrX'] = 旧[arrX'+shiftX], 西侧数据移出, 东侧条带置零
    if (shiftX != 0) {
        if (shiftX > 0) {
            std::memmove(grid[0], grid[shiftX], (size_t)(N - shiftX) * sizeof(grid[0]));
            std::memset(grid[N - shiftX], 0, (size_t)shiftX * sizeof(grid[0]));
        } else {
            int s = -shiftX;
            std::memmove(grid[s], grid[0], (size_t)(N - s) * sizeof(grid[0]));
            std::memset(grid[0], 0, (size_t)s * sizeof(grid[0]));
        }
    }
    // Z 方向: 每行内部平移
    if (shiftZ != 0) {
        for (int x = 0; x < N; x++) {
            if (shiftZ > 0) {
                std::memmove(grid[x], grid[x] + shiftZ, (size_t)(N - shiftZ) * sizeof(grid[x][0]));
                std::memset(grid[x] + (N - shiftZ), 0, (size_t)shiftZ * sizeof(grid[x][0]));
            } else {
                int s = -shiftZ;
                std::memmove(grid[x] + s, grid[x], (size_t)(N - s) * sizeof(grid[x][0]));
                std::memset(grid[x], 0, (size_t)s * sizeof(grid[x][0]));
            }
        }
    }
}

// ==========================================
// 生物群系中文翻译字典引擎
// ==========================================
    // [性能] FNV-1a 64 位字符串哈希，替代 std::hash<string>。扫描路径内每格调用 2 次，
// 实测比 MSVC std::hash<string> 快 2.5~3 倍，避免大量分配/析构临时 string 的开销。
inline uint64_t Fnv1aHash(const char* s, size_t len) noexcept {
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= (uint8_t)s[i];
        h *= 1099511628211ULL;
    }
    return h;
}
inline uint64_t Fnv1aHash(const std::string& s) noexcept {
    return Fnv1aHash(s.data(), s.size());
}

inline std::string TranslateBiomeName(const std::string& rawName) {
    std::string cleanName = rawName;
    size_t colonPos = cleanName.find(":");
    if (colonPos != std::string::npos) cleanName = cleanName.substr(colonPos + 1);

    std::string lower = cleanName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    // [性能] 原实现每帧对 kBiomeRules 做 O(N) 线性 find 扫描（N=62），
    // 玩家 tick 调用一次，大地图悬停查询再调用一次。改用 (hash -> key) 直接查找，
    // 由于生物群系字符串是固定全集（62 条别名），无哈希冲突，复杂度 O(1)。
    // 为保持"别名优先于通用名"的正确匹配，原始具体别名（如 dark_forest、windswept_savanna）
    // 在初始化时全部登记到哈希表；通用 fallback（windswept / mushroom）仍然走线性扫描兜底。
    struct BiomeHashTable {
        std::unordered_map<uint64_t, const char*> map;
        BiomeHashTable() {
            static const std::pair<const char*, const char*> entries[] = {
                {"end_highlands", "BIOME_THE_END"},
                {"end_midlands", "BIOME_THE_END"},
                {"end_barrens", "BIOME_THE_END"},
                {"small_end_islands", "BIOME_THE_END"},
                {"the_end", "BIOME_THE_END"},
                {"crimson_forest", "BIOME_CRIMSON_FOREST"},
                {"warped_forest", "BIOME_WARPED_FOREST"},
                {"soulsand_valley", "BIOME_SOUL_SAND_VALLEY"},
                {"soul_sand_valley", "BIOME_SOUL_SAND_VALLEY"},
                {"basalt_deltas", "BIOME_BASALT_DELTAS"},
                {"nether_wastes", "BIOME_HELL"},
                {"hell", "BIOME_HELL"},
                {"deep_frozen_ocean", "BIOME_DEEP_FROZEN_OCEAN"},
                {"deep_cold_ocean", "BIOME_DEEP_COLD_OCEAN"},
                {"deep_lukewarm_ocean", "BIOME_DEEP_LUKEWARM_OCEAN"},
                {"deep_warm_ocean", "BIOME_DEEP_WARM_OCEAN"},
                {"deep_ocean", "BIOME_DEEP_OCEAN"},
                {"legacy_frozen_ocean", "BIOME_FROZEN_OCEAN"},
                {"frozen_ocean", "BIOME_FROZEN_OCEAN"},
                {"warm_ocean", "BIOME_WARM_OCEAN"},
                {"cold_ocean", "BIOME_COLD_OCEAN"},
                {"lukewarm_ocean", "BIOME_LUKEWARM_OCEAN"},
                {"ocean", "BIOME_OCEAN"},
                {"frozen_river", "BIOME_FROZEN_RIVER"},
                {"river", "BIOME_RIVER"},
                {"stone_beach", "BIOME_STONY_SHORE"},
                {"stony_shore", "BIOME_STONY_SHORE"},
                {"cold_beach", "BIOME_SNOWY_BEACH"},
                {"snowy_beach", "BIOME_SNOWY_BEACH"},
                {"beach", "BIOME_BEACH"},
                {"ice_plains_spikes", "BIOME_ICE_SPIKES"},
                {"ice_spikes", "BIOME_ICE_SPIKES"},
                {"ice_mountains", "BIOME_SNOWY_SLOPES"},
                {"snowy_slopes", "BIOME_SNOWY_SLOPES"},
                {"ice_plains", "BIOME_SNOWY_PLAINS"},
                {"snowy_plains", "BIOME_SNOWY_PLAINS"},
                {"snowy_tundra", "BIOME_SNOWY_PLAINS"},
                {"jagged_peaks", "BIOME_JAGGED_PEAKS"},
                {"frozen_peaks", "BIOME_FROZEN_PEAKS"},
                {"stony_peaks", "BIOME_STONY_PEAKS"},
                {"grove", "BIOME_GROVE"},
                {"extreme_hills_plus_trees", "BIOME_WINDSWEPT_FOREST"},
                {"windswept_forest", "BIOME_WINDSWEPT_FOREST"},
                {"extreme_hills_mutated", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
                {"windswept_gravelly_hills", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
                {"windswept_hills", "BIOME_EXTREME_HILLS"},
                {"windswept_savanna", "BIOME_WINDSWEPT_SAVANNA"},
                {"extreme_hills_edge", "BIOME_EXTREME_HILLS"},
                {"extreme_hills", "BIOME_EXTREME_HILLS"},
                {"windswept", "BIOME_EXTREME_HILLS"},
                {"mesa_bryce", "BIOME_ERODED_BADLANDS"},
                {"eroded_badlands", "BIOME_ERODED_BADLANDS"},
                {"mesa_plateau_stone", "BIOME_WOODED_BADLANDS"},
                {"wooded_badlands", "BIOME_WOODED_BADLANDS"},
                {"mesa_plateau", "BIOME_MESA"},
                {"mesa", "BIOME_MESA"},
                {"badlands", "BIOME_MESA"},
                {"cherry_grove", "BIOME_CHERRY"},
                {"cherry", "BIOME_CHERRY"},
                {"meadow", "BIOME_MEADOW"},
                {"savanna_mutated", "BIOME_WINDSWEPT_SAVANNA"},
                {"savanna_plateau", "BIOME_SAVANNA_PLATEAU"},
                {"savanna", "BIOME_SAVANNA"},
                {"bamboo_jungle", "BIOME_BAMBOO_JUNGLE"},
                {"jungle_edge", "BIOME_SPARSE_JUNGLE"},
                {"sparse_jungle", "BIOME_SPARSE_JUNGLE"},
                {"jungle", "BIOME_JUNGLE"},
                {"mangrove_swamp", "BIOME_MANGROVE_SWAMP"},
                {"swampland", "BIOME_SWAMP"},
                {"swamp", "BIOME_SWAMP"},
                {"mushroom_island", "BIOME_MUSHROOM"},
                {"mushroom_fields", "BIOME_MUSHROOM"},
                {"mushroom", "BIOME_MUSHROOM"},
                {"pale_garden", "BIOME_PALE_GARDEN"},
                {"lush_caves", "BIOME_LUSH_CAVES"},
                {"dripstone_caves", "BIOME_DRIPSTONE_CAVES"},
                {"sulfur_caves", "BIOME_SULFUR_CAVES"},
                {"cold_taiga", "BIOME_SNOWY_TAIGA"},
                {"snowy_taiga", "BIOME_SNOWY_TAIGA"},
                {"mega_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
                {"old_growth_pine_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
                {"redwood_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
                {"old_growth_spruce_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
                {"taiga", "BIOME_TAIGA"},
                {"dark_oak_forest", "BIOME_DARK_FOREST"},
                {"roofed_forest", "BIOME_DARK_FOREST"},
                {"dark_forest", "BIOME_DARK_FOREST"},
                {"birch_forest_mutated", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
                {"old_growth_birch_forest", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
                {"birch_forest_hills", "BIOME_BIRCH_FOREST"},
                {"birch_forest", "BIOME_BIRCH_FOREST"},
                {"flower_forest", "BIOME_FLOWER_FOREST"},
                {"forest", "BIOME_FOREST"},
                {"sunflower_plains", "BIOME_SUNFLOWER_PLAINS"},
                {"plains", "BIOME_PLAINS"},
                {"deep_dark", "BIOME_DEEP_DARK"},
                {"desert", "BIOME_DESERT"},
            };
            for (auto const& p : entries) {
                size_t l = std::strlen(p.first);
                map.emplace(Fnv1aHash(p.first, l), p.second);
            }
        }
    };
    static const BiomeHashTable s_table;

    std::string biomeKey = "BIOME_UNKNOWN";
    uint64_t lowerHash = Fnv1aHash(lower);
    auto it = s_table.map.find(lowerHash);
    if (it != s_table.map.end()) {
        biomeKey = it->second;
    } else {
        // 哈希未命中：降级为线性 find 子串扫描（处理某些不常见的命名组合）
        static const std::vector<std::pair<std::string, std::string>> kBiomeRules = {
            {"end_highlands", "BIOME_THE_END"}, {"end_midlands", "BIOME_THE_END"},
            {"end_barrens", "BIOME_THE_END"}, {"small_end_islands", "BIOME_THE_END"},
            {"the_end", "BIOME_THE_END"}, {"crimson_forest", "BIOME_CRIMSON_FOREST"},
            {"warped_forest", "BIOME_WARPED_FOREST"}, {"soulsand_valley", "BIOME_SOUL_SAND_VALLEY"},
            {"soul_sand_valley", "BIOME_SOUL_SAND_VALLEY"}, {"basalt_deltas", "BIOME_BASALT_DELTAS"},
            {"nether_wastes", "BIOME_HELL"}, {"hell", "BIOME_HELL"},
            {"deep_frozen_ocean", "BIOME_DEEP_FROZEN_OCEAN"}, {"deep_cold_ocean", "BIOME_DEEP_COLD_OCEAN"},
            {"deep_lukewarm_ocean", "BIOME_DEEP_LUKEWARM_OCEAN"}, {"deep_warm_ocean", "BIOME_DEEP_WARM_OCEAN"},
            {"deep_ocean", "BIOME_DEEP_OCEAN"}, {"legacy_frozen_ocean", "BIOME_FROZEN_OCEAN"},
            {"frozen_ocean", "BIOME_FROZEN_OCEAN"}, {"warm_ocean", "BIOME_WARM_OCEAN"},
            {"cold_ocean", "BIOME_COLD_OCEAN"}, {"lukewarm_ocean", "BIOME_LUKEWARM_OCEAN"},
            {"ocean", "BIOME_OCEAN"}, {"frozen_river", "BIOME_FROZEN_RIVER"},
            {"river", "BIOME_RIVER"}, {"stone_beach", "BIOME_STONY_SHORE"},
            {"stony_shore", "BIOME_STONY_SHORE"}, {"cold_beach", "BIOME_SNOWY_BEACH"},
            {"snowy_beach", "BIOME_SNOWY_BEACH"}, {"beach", "BIOME_BEACH"},
            {"ice_plains_spikes", "BIOME_ICE_SPIKES"}, {"ice_spikes", "BIOME_ICE_SPIKES"},
            {"ice_mountains", "BIOME_SNOWY_SLOPES"}, {"snowy_slopes", "BIOME_SNOWY_SLOPES"},
            {"ice_plains", "BIOME_SNOWY_PLAINS"}, {"snowy_plains", "BIOME_SNOWY_PLAINS"},
            {"snowy_tundra", "BIOME_SNOWY_PLAINS"}, {"jagged_peaks", "BIOME_JAGGED_PEAKS"},
            {"frozen_peaks", "BIOME_FROZEN_PEAKS"}, {"stony_peaks", "BIOME_STONY_PEAKS"},
            {"grove", "BIOME_GROVE"}, {"extreme_hills_plus_trees", "BIOME_WINDSWEPT_FOREST"},
            {"windswept_forest", "BIOME_WINDSWEPT_FOREST"}, {"extreme_hills_mutated", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
            {"windswept_gravelly_hills", "BIOME_WINDSWEPT_GRAVELLY_HILLS"}, {"windswept_hills", "BIOME_EXTREME_HILLS"},
            {"windswept_savanna", "BIOME_WINDSWEPT_SAVANNA"}, {"extreme_hills_edge", "BIOME_EXTREME_HILLS"},
            {"extreme_hills", "BIOME_EXTREME_HILLS"}, {"windswept", "BIOME_EXTREME_HILLS"},
            {"mesa_bryce", "BIOME_ERODED_BADLANDS"}, {"eroded_badlands", "BIOME_ERODED_BADLANDS"},
            {"mesa_plateau_stone", "BIOME_WOODED_BADLANDS"}, {"wooded_badlands", "BIOME_WOODED_BADLANDS"},
            {"mesa_plateau", "BIOME_MESA"}, {"mesa", "BIOME_MESA"},
            {"badlands", "BIOME_MESA"}, {"cherry_grove", "BIOME_CHERRY"},
            {"cherry", "BIOME_CHERRY"}, {"meadow", "BIOME_MEADOW"},
            {"savanna_mutated", "BIOME_WINDSWEPT_SAVANNA"}, {"savanna_plateau", "BIOME_SAVANNA_PLATEAU"},
            {"savanna", "BIOME_SAVANNA"}, {"bamboo_jungle", "BIOME_BAMBOO_JUNGLE"},
            {"jungle_edge", "BIOME_SPARSE_JUNGLE"}, {"sparse_jungle", "BIOME_SPARSE_JUNGLE"},
            {"jungle", "BIOME_JUNGLE"}, {"mangrove_swamp", "BIOME_MANGROVE_SWAMP"},
            {"swampland", "BIOME_SWAMP"}, {"swamp", "BIOME_SWAMP"},
            {"mushroom_island", "BIOME_MUSHROOM"}, {"mushroom_fields", "BIOME_MUSHROOM"},
            {"mushroom", "BIOME_MUSHROOM"}, {"pale_garden", "BIOME_PALE_GARDEN"},
            {"lush_caves", "BIOME_LUSH_CAVES"}, {"dripstone_caves", "BIOME_DRIPSTONE_CAVES"},
            {"sulfur_caves", "BIOME_SULFUR_CAVES"}, {"sulfur", "BIOME_SULFUR_CAVES"},
            {"cold_taiga", "BIOME_SNOWY_TAIGA"}, {"snowy_taiga", "BIOME_SNOWY_TAIGA"},
            {"mega_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"}, {"old_growth_pine_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
            {"redwood_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"}, {"old_growth_spruce_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
            {"taiga", "BIOME_TAIGA"}, {"dark_oak_forest", "BIOME_DARK_FOREST"},
            {"roofed_forest", "BIOME_DARK_FOREST"}, {"dark_forest", "BIOME_DARK_FOREST"},
            {"birch_forest_mutated", "BIOME_OLD_GROWTH_BIRCH_FOREST"}, {"old_growth_birch_forest", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
            {"birch_forest_hills", "BIOME_BIRCH_FOREST"}, {"birch_forest", "BIOME_BIRCH_FOREST"},
            {"flower_forest", "BIOME_FLOWER_FOREST"}, {"forest", "BIOME_FOREST"},
            {"sunflower_plains", "BIOME_SUNFLOWER_PLAINS"}, {"plains", "BIOME_PLAINS"},
            {"deep_dark", "BIOME_DEEP_DARK"}, {"desert", "BIOME_DESERT"},
        };
        for (const auto& rule : kBiomeRules) {
            if (lower.find(rule.first) != std::string::npos) {
                biomeKey = rule.second;
                break;
            }
        }
    }

        std::string result = LanguageManager::GetText(biomeKey);
        
        // 若找不到对应语言，回退将英文名首字母大写返回
        if (result == biomeKey) {
            std::string formattedName = cleanName;
            for (size_t i = 0; i < formattedName.length(); ++i) {
                if (formattedName[i] == '_') formattedName[i] = ' ';
                if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
            }
            return formattedName;
        }
        return result;
    }

// ==========================================
// 真彩自然光色彩引擎
// ==========================================
struct BiomeTintTriple {
        mce::Color grass;
        mce::Color foliage;
        mce::Color water;
    };

// [步骤1排查打点] 记录未命中规则直接落入默认/哈希的群系和方块（每种仅记录一次）
inline void LogColorMiss(std::string const& tag, std::string const& name) {
    static std::unordered_set<std::string> s_loggedMisses;
    static std::mutex s_missLogMutex;
    std::lock_guard<std::mutex> lock(s_missLogMutex);
    std::string key = tag + ":" + name;
    if (s_loggedMisses.size() < 1000 && s_loggedMisses.insert(key).second) {
        try {
            auto dataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
            std::ofstream log((dataDir / "color_miss_log.txt").string(), std::ios::app);
            if (log) {
                log << "[" << tag << "] " << name << std::endl;
            }
        } catch (...) {}
    }
}

inline void getBiomeTints(std::string const& biomeName, mce::Color& grass, mce::Color& foliage, mce::Color& water) {
    // [性能] 生物群系染色查找缓存：直接 key 原始 biomeName（含 namespace），
    // 避免每次查找 2~3 次 heap alloc（lower 转换 + 线性 find 子串）。
    // 典型世界内生物群系数目 <= 60，缓存几乎 100% 命中。
    static std::unordered_map<uint64_t, BiomeTintTriple> s_tintCache;
    if (!biomeName.empty()) {
        uint64_t h = Fnv1aHash(biomeName);
        auto it = s_tintCache.find(h);
        if (it != s_tintCache.end()) {
            grass   = it->second.grass;
            foliage = it->second.foliage;
            water   = it->second.water;
            return;
        }
    }

    // 默认基础色调（温带平原/森林）：草地匹配实际颜色 RGB(98,132,60)——基于截图采样，明亮翠绿
    grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
    foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    water   = mce::Color(0.18f, 0.38f, 0.85f, 1.0f);

    if (biomeName.empty()) return;

    std::string lower = biomeName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    // [性能] 包装剩余查找，结果写回缓存
    auto computeTints = [&]() {

    // === 平原：草地匹配实际颜色 RGB(98,132,60)——基于截图采样，明亮翠绿 ===
    if (lower.find("plains") != std::string::npos && lower.find("snow") == std::string::npos && lower.find("ice") == std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    else if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.44f, 0.56f, 0.26f, 1.0f);
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
    }
    else if (lower.find("eroded_badlands") != std::string::npos) {
        // eroded_badlands: grass tint -> dark taiga-green (user request)
        grass   = mce::Color(0.26f, 0.38f, 0.30f, 1.0f);
        foliage = mce::Color(0.15f, 0.28f, 0.25f, 1.0f);
    }
    else if (lower.find("desert") != std::string::npos || lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) {
        grass   = mce::Color(0.45f, 0.42f, 0.20f, 1.0f);
        foliage = mce::Color(0.35f, 0.38f, 0.18f, 1.0f);
        water   = mce::Color(0.15f, 0.45f, 0.45f, 1.0f);
    } 
    else if (lower.find("savanna") != std::string::npos) {
        grass   = mce::Color(0.42f, 0.42f, 0.18f, 1.0f);
        foliage = mce::Color(0.375f, 0.345f, 0.105f, 1.0f); // 热带草原/风袭热带草原/热带高原：温润金橄榄黄褐树叶（实机采样RGB(94,89,28)精准校准，彻底消除冷暗灰绿失真）
    } 
    // === 丛林与竹林：草地匹配实际颜色 RGB(53,124,37) 鲜活翡翠碧绿（红分量由 0.32 降至 0.21，消除偏黄偏暗失真） ===
    else if (lower.find("jungle") != std::string::npos || lower.find("bamboo") != std::string::npos) {
        grass   = mce::Color(0.210f, 0.488f, 0.144f, 1.0f);
        foliage = mce::Color(0.22f, 0.45f, 0.15f, 1.0f);
    } 
    else if (lower.find("mangrove") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.38f, 0.18f, 1.0f);
        foliage = mce::Color(0.32f, 0.40f, 0.09f, 1.0f); // 原版专属色表 mangrove_swamp_foliage (#8db127) 鲜润暖青绿
        water   = mce::Color(0.23f, 0.48f, 0.42f, 1.0f); // 原版 #3a7a6a 红树林沼泽青碧水
    }
    else if (lower.find("swamp") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.30f, 0.13f, 1.0f); // 沼泽暖调橄榄草地色（实机采样调校）
        foliage = mce::Color(0.24f, 0.26f, 0.11f, 1.0f); // 沼泽树叶深暗暖橄榄黄褐色（消除冷暗灰绿感）
        water   = mce::Color(0.24f, 0.28f, 0.20f, 1.0f); // 沼泽特征深暗泥浊水体（消除浅黄绿高光）
    }
    // === 蘑菇岛系列：极鲜亮翠绿草地 ===
    else if (lower.find("mushroom") != std::string::npos) {
        grass   = mce::Color(0.33f, 0.79f, 0.25f, 1.0f);
        foliage = mce::Color(0.30f, 0.70f, 0.22f, 1.0f);
        water   = mce::Color(0.25f, 0.46f, 0.89f, 1.0f);
    }
    // === 海洋与河流细分（必须在通用寒带/温带前）：还原基岩版专属水体色调 ===
    // [温水海洋 / 深温水海洋] 必须在 warm_ocean 之前（因含 "warm_ocean" 子串）；水色与其它主流水域保持完全一致
    else if (lower.find("lukewarm_ocean") != std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
        water   = mce::Color(0.25f, 0.46f, 0.89f, 1.0f); // 与其它主流水域完全一致的群青水色
    }
    // [暖水海洋 (珊瑚海)]
    else if (lower.find("warm_ocean") != std::string::npos) {
        grass   = mce::Color(0.56f, 0.73f, 0.44f, 1.0f);
        foliage = mce::Color(0.40f, 0.65f, 0.30f, 1.0f);
        water   = mce::Color(0.20f, 0.52f, 0.88f, 1.0f); // 暖水海洋柔和热带水色，避免高光青白
    }
    else if (lower.find("cold_ocean") != std::string::npos) {
        grass   = mce::Color(0.35f, 0.48f, 0.33f, 1.0f);
        foliage = mce::Color(0.20f, 0.32f, 0.18f, 1.0f);
        water   = mce::Color(0.24f, 0.34f, 0.84f, 1.0f); // 冷水海洋深海蓝
    }
    else if (lower.find("frozen_river") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
        water   = mce::Color(0.094f, 0.325f, 0.565f, 1.0f); // 原版 #185390 冻河深冽冰水
    }
    else if (lower.find("frozen_ocean") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
        water   = mce::Color(0.145f, 0.439f, 0.710f, 1.0f); // 原版 #2570B5 冻洋水色
    }
    else if (lower.find("ocean") != std::string::npos || lower.find("river") != std::string::npos) {
        grass   = mce::Color(0.385f, 0.518f, 0.235f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
        water   = mce::Color(0.25f, 0.46f, 0.89f, 1.0f); // 普通海洋与河流经典群青蓝
    }
    // === 下界生物群系（必须在通用 forest 规则之前，避免 crimson/warped forest 误判为普通森林） ===
    else if (lower.find("crimson_forest") != std::string::npos) {
        grass   = mce::Color(0.70f, 0.06f, 0.06f, 1.0f);
        foliage = mce::Color(0.75f, 0.08f, 0.08f, 1.0f);
        water   = mce::Color(0.56f, 0.08f, 0.08f, 1.0f);
    }
    else if (lower.find("warped_forest") != std::string::npos) {
        grass   = mce::Color(0.08f, 0.61f, 0.52f, 1.0f);
        foliage = mce::Color(0.08f, 0.61f, 0.52f, 1.0f);
        water   = mce::Color(0.07f, 0.28f, 0.27f, 1.0f);
    }
    else if (lower.find("soul_sand_valley") != std::string::npos || lower.find("soulsand_valley") != std::string::npos) {
        grass   = mce::Color(0.35f, 0.40f, 0.40f, 1.0f);
        foliage = mce::Color(0.30f, 0.38f, 0.38f, 1.0f);
        water   = mce::Color(0.11f, 0.28f, 0.27f, 1.0f);
    }
    else if (lower.find("basalt_deltas") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.26f, 0.28f, 1.0f);
        foliage = mce::Color(0.24f, 0.22f, 0.24f, 1.0f);
        water   = mce::Color(0.27f, 0.23f, 0.23f, 1.0f);
    }
    else if (lower.find("nether") != std::string::npos || lower.find("hell") != std::string::npos) {
        grass   = mce::Color(0.60f, 0.45f, 0.30f, 1.0f);
        foliage = mce::Color(0.60f, 0.45f, 0.30f, 1.0f);
        water   = mce::Color(0.56f, 0.08f, 0.08f, 1.0f);
    }
    // === 末地群系 ===
    else if (lower.find("end") != std::string::npos) {
        grass   = mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
        foliage = mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
        water   = mce::Color(0.38f, 0.32f, 0.62f, 1.0f);
    }
    // === 针叶林与寒带：草地与树叶匹配实际颜色（草地 RGB(86,116,84)，树叶 RGB(45,66,45)） ===
    else if (lower.find("taiga") != std::string::npos || lower.find("snow") != std::string::npos || lower.find("ice") != std::string::npos || lower.find("frozen") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
    } 
    // === 风袭丘陵/山地系列：草地匹配实际颜色 RGB(78,102,76) ===
    else if ((lower.find("extreme_hills") != std::string::npos || lower.find("windswept") != std::string::npos)
             && lower.find("savanna") == std::string::npos) {
        grass   = mce::Color(0.306f, 0.400f, 0.298f, 1.0f);
        foliage = mce::Color(0.215f, 0.337f, 0.215f, 1.0f);
    }
    // === 山峰系列（1.18+ 山地峰顶与山坡） ===
    else if (lower.find("stony_peaks") != std::string::npos) {
        grass   = mce::Color(0.55f, 0.72f, 0.36f, 1.0f); // 暖性石峰，邻接温热带
        foliage = mce::Color(0.38f, 0.62f, 0.28f, 1.0f);
    }
    else if (lower.find("peaks") != std::string::npos || lower.find("slopes") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f); // 寒冷雪峰与雪坡
        foliage = mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
        water   = mce::Color(0.22f, 0.22f, 0.79f, 1.0f);
    }
    // === 海岸与沙滩 ===
    else if (lower.find("stony_shore") != std::string::npos || lower.find("stone_beach") != std::string::npos) {
        grass   = mce::Color(0.39f, 0.48f, 0.35f, 1.0f);
        foliage = mce::Color(0.25f, 0.38f, 0.25f, 1.0f);
        water   = mce::Color(0.24f, 0.34f, 0.84f, 1.0f);
    }
    else if (lower.find("beach") != std::string::npos || lower.find("shore") != std::string::npos) {
        grass   = mce::Color(0.57f, 0.74f, 0.35f, 1.0f);
        foliage = mce::Color(0.40f, 0.65f, 0.30f, 1.0f);
        water   = mce::Color(0.25f, 0.46f, 0.89f, 1.0f);
    }
    // === 白桦森林与原始桦木森林：草地匹配实际颜色 RGB(83,114,63) ===
    else if (lower.find("birch") != std::string::npos) {
        grass   = mce::Color(0.325f, 0.447f, 0.247f, 1.0f);
        foliage = mce::Color(0.263f, 0.341f, 0.173f, 1.0f);
    }
    else if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos || lower.find("dark_oak_forest") != std::string::npos) {
        grass   = mce::Color(0.20f, 0.30f, 0.12f, 1.0f);
        foliage = mce::Color(0.235f, 0.471f, 0.118f, 1.0f);
    }
    // === 森林与繁花森林：草地匹配实际颜色 RGB(75,118,55) ===
    else if (lower.find("forest") != std::string::npos) {
        grass   = mce::Color(0.294f, 0.463f, 0.216f, 1.0f);
        foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    }
    // === 樱花树林：必须在通用 grove 之前（cherry_grove 含 grove 需优先拦截） ===
    else if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.56f, 0.81f, 0.29f, 1.0f); // 鲜亮春意浅翠绿
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f); // 标志性樱花粉红
        water   = mce::Color(0.365f, 0.718f, 0.937f, 1.0f); // 原版 #5db7ef 浅天蓝
    }
    else if (lower.find("meadow") != std::string::npos || lower.find("grove") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.455f, 0.329f, 1.0f);
        foliage = mce::Color(0.18f, 0.32f, 0.20f, 1.0f);
    }
    else if (lower.find("lush_caves") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.45f, 0.20f, 1.0f);
        foliage = mce::Color(0.20f, 0.40f, 0.15f, 1.0f);
        water   = mce::Color(0.15f, 0.35f, 0.55f, 1.0f);
    }
    else if (lower.find("deep_dark") != std::string::npos) {
        grass   = mce::Color(0.18f, 0.22f, 0.20f, 1.0f);
        foliage = mce::Color(0.15f, 0.18f, 0.16f, 1.0f);
        water   = mce::Color(0.10f, 0.15f, 0.20f, 1.0f);
    }
    else if (lower.find("dripstone_caves") != std::string::npos) {
        grass   = mce::Color(0.48f, 0.64f, 0.38f, 1.0f);
        foliage = mce::Color(0.38f, 0.57f, 0.27f, 1.0f);
        water   = mce::Color(0.25f, 0.46f, 0.89f, 1.0f);
    }
    // === 苍白之园：草地匹配实际颜色 RGB(86,97,79) ===
    else if (lower.find("pale_garden") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
        foliage = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
    }
    // === 硫黄洞穴：原版 client_biome 定义水色 #34BF89, 草色 #ABA64F ===
    else if (lower.find("sulfur_caves") != std::string::npos || lower.find("sulfur") != std::string::npos) {
        grass   = mce::Color(0.671f, 0.651f, 0.310f, 1.0f);
        foliage = mce::Color(0.671f, 0.651f, 0.310f, 1.0f);
        water   = mce::Color(0.204f, 0.749f, 0.537f, 1.0f);
    } else {
        LogColorMiss("BiomeMiss", biomeName);
    }
    }; // [性能] lambda 闭合
    computeTints();
    // 写回缓存（上层已对 biomeName 为空做 return）
    BiomeTintTriple tri{grass, foliage, water};
    s_tintCache.emplace(Fnv1aHash(biomeName), tri);
}

// [辅助] 判断方块是否为隐形/技术性覆盖方块（地图扫描应穿透保留底层方块，不渲染为黑洞或杂色）
inline bool IsInvisibleOrTechnicalOverlay(std::string const& rawName) noexcept {
    if (rawName.empty()) return true;
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (name == "minecraft:air" || name == "air") return true;
    if (name.find("barrier") != std::string::npos) return true;
    if (name.find("light_block") != std::string::npos) return true;
    if (name.find("structure_void") != std::string::npos) return true;
    if (name.find("placeholder") != std::string::npos) return true;
    if (name.find("unknown") != std::string::npos) return true;
    if (name.find("info_update") != std::string::npos) return true;
    if (name.find("invisiblebedrock") != std::string::npos || name.find("invisible_bedrock") != std::string::npos) return true;
    if (name.find("movingblock") != std::string::npos || name.find("moving_block") != std::string::npos) return true;
    if (name.find("pistonarmcollision") != std::string::npos || name.find("piston_arm_collision") != std::string::npos) return true;
    if (name.find("border_block") != std::string::npos) return true;
    if (name == "minecraft:allow" || name == "allow" || name == "minecraft:deny" || name == "deny") return true;
    if (name == "minecraft:camera" || name == "camera") return true;
    if (name.find("jigsaw") != std::string::npos) return true;
    if (name.find("structure_block") != std::string::npos) return true;
    if (name.find("tripwire") != std::string::npos || name.find("trip_wire") != std::string::npos) return true;
    if (name.find("frame") != std::string::npos && name.find("end_portal_frame") == std::string::npos) return true;
    if (name.find("reserved") != std::string::npos) return true;
    return false;
}

inline mce::Color getBlockColor(std::string const& rawName, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol) {
    // 统一转为全小写，杜绝基岩版 camelCase 命名（如 invisibleBedrock, seaLantern, tripWire, concretePowder）匹配问题
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (IsInvisibleOrTechnicalOverlay(name)) {
        return mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (name.find("glass") != std::string::npos) return mce::Color(0.8f, 0.9f, 0.9f, 0.3f);
    if (name.find("path") != std::string::npos || name.find("farmland") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    // [竹板/竹马赛克/竹制品 vs 生竹子]
    if (name.find("bamboo") != std::string::npos) {
        if (name.find("planks") != std::string::npos || name.find("mosaic") != std::string::npos ||
            name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos ||
            name.find("fence") != std::string::npos || name.find("door") != std::string::npos ||
            name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos ||
            name.find("button") != std::string::npos || name.find("pressure_plate") != std::string::npos) {
            return mce::Color(0.85f, 0.78f, 0.55f, 1.0f);
        }
        return mce::Color(0.40f, 0.70f, 0.20f, 1.0f);
    }
    // [干海带块] 必须在通用 kelp 规则之前
    if (name.find("dried_kelp_block") != std::string::npos) return mce::Color(0.25f, 0.35f, 0.15f, 1.0f);
    // [枯萎的灌木] 原版基岩版名称为 minecraft:deadbush（无下划线，Java版为 dead_bush）
    // 原版贴图采样真实均值 RGB(108, 79, 41) #6b4e28，必须在通用 bush 规则之前，彻底消除被按生物群系草色误染为绿色的严重失真缺陷
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return mce::Color(0.421f, 0.309f, 0.159f, 1.0f);
    // [矮枯草丛 / 高枯草丛 (Dry Grass)] 原版不受生物群系草色影响，为固定金黄麦草/枯草色
    // 材质采样：short_dry_grass RGB(187, 159, 108) #bb9e6c，tall_dry_grass RGB(197, 172, 123) #c4ab7a
    // 必须在通用 grass 规则之前，彻底杜绝恶地与沙漠枯草被误染为绿草
    if (name.find("tall_dry_grass") != std::string::npos) return mce::Color(0.771f, 0.674f, 0.482f, 1.0f);
    if (name.find("dry_grass") != std::string::npos) return mce::Color(0.733f, 0.622f, 0.424f, 1.0f);
    // [仙人掌花] 恶地/沙漠新植物，亮粉红色花瓣 RGB(210, 121, 135) #d17887，杜绝被通用 flower 泛化为黄色
    if (name.find("cactus_flower") != std::string::npos) return mce::Color(0.822f, 0.473f, 0.531f, 1.0f);
    // [萤火虫灌木丛] 原版基岩版名称为 minecraft:firefly_bush
    // 由深暗橄榄褐枯枝与温暖金黄发光萤火虫组成，实机采样暖金琥珀色 RGB(148, 128, 56) #948038
    // 必须在通用 bush 规则之前，彻底消除被按生物群系草色误染为绿草导致完全隐形的问题
    if (name.find("firefly_bush") != std::string::npos || name.find("firefly") != std::string::npos) return mce::Color(0.58f, 0.50f, 0.22f, 1.0f);
    // [枯叶堆 / 掉落枯叶 (Leaf Litter)] 原版森林地表落叶覆盖层，基岩版标识为 minecraft:leaf_litter
    // 实机纹理与物品采样暖秋枯叶红褐色 RGB(127, 88, 60) #7f583c
    // 必须在通用 leaf/leaves 树叶规则之前，彻底杜绝被误按树叶染为绿色而在草地上隐形
    if (name.find("leaf_litter") != std::string::npos || name.find("litter") != std::string::npos || name.find("fallen_leaf") != std::string::npos || name.find("fallen_leaves") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.24f, 1.0f);
    // [红灌木 (实验版)]
    if (name.find("red_shrub") != std::string::npos) return mce::Color(0.48f, 0.14f, 0.09f, 1.0f);

    // [荷叶] 必须在通用 water 规则之前（Bedrock 原版名称为 minecraft:waterlily，含子串 "water"）
    // 原版荷叶不受群系染色影响，取实机真实采样深森林绿 RGB(19, 72, 27)
    if (name.find("lily_pad") != std::string::npos || name.find("waterlily") != std::string::npos) {
        return mce::Color(0.08f, 0.35f, 0.12f, 1.0f);
    }
    // [气泡柱]
    if (name.find("bubble_column") != std::string::npos) return waterCol;

    if (name.find("water") != std::string::npos) return waterCol;
    if (name.find("pink_petals") != std::string::npos) return mce::Color(0.95f, 0.68f, 0.78f, 1.0f);

    if (name.find("peony") != std::string::npos || name.find("pink_tulip") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (name.find("dandelion") != std::string::npos || name.find("sunflower") != std::string::npos || name.find("yellow_flower") != std::string::npos) return mce::Color(0.95f, 0.85f, 0.20f, 1.0f);
    if (name.find("rose") != std::string::npos || name.find("poppy") != std::string::npos || name.find("red_flower") != std::string::npos || name.find("red_tulip") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("orchid") != std::string::npos || name.find("cornflower") != std::string::npos) return mce::Color(0.20f, 0.40f, 0.85f, 1.0f);
    if (name.find("allium") != std::string::npos || name.find("lilac") != std::string::npos) return mce::Color(0.70f, 0.30f, 0.70f, 1.0f);
    if (name.find("daisy") != std::string::npos || name.find("bluet") != std::string::npos || name.find("valley") != std::string::npos || name.find("white_tulip") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (name.find("flower") != std::string::npos || name.find("bloom") != std::string::npos || name.find("blossom") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.25f, 1.0f);
    // [眼眸花] 开眼状态为橙色，闭眼为灰褐；地图统一取开眼橙色作为代表色
    if (name.find("eyeblossom") != std::string::npos) return mce::Color(0.71f, 0.35f, 0.12f, 1.0f);

    // [陶瓦与硬化粘土] 恶地/平顶山核心构成方块，原版专属温润暗沉大地色表（采样真实纹理均值）
    // 必须在通用高饱和染色规则前处理，防止恶地各色彩层被纯鲜艳羊毛色劫持
    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white") != std::string::npos) return mce::Color(0.822f, 0.698f, 0.633f, 1.0f);
        if (name.find("orange") != std::string::npos) return mce::Color(0.634f, 0.329f, 0.148f, 1.0f);
        if (name.find("magenta") != std::string::npos) return mce::Color(0.587f, 0.345f, 0.426f, 1.0f);
        if (name.find("light_blue") != std::string::npos) return mce::Color(0.445f, 0.426f, 0.541f, 1.0f);
        if (name.find("yellow") != std::string::npos) return mce::Color(0.730f, 0.522f, 0.139f, 1.0f);
        if (name.find("lime") != std::string::npos) return mce::Color(0.406f, 0.461f, 0.207f, 1.0f);
        if (name.find("pink") != std::string::npos) return mce::Color(0.635f, 0.307f, 0.309f, 1.0f);
        if (name.find("light_gray") != std::string::npos || name.find("silver") != std::string::npos) return mce::Color(0.530f, 0.420f, 0.382f, 1.0f);
        if (name.find("gray") != std::string::npos) return mce::Color(0.227f, 0.166f, 0.139f, 1.0f);
        if (name.find("cyan") != std::string::npos) return mce::Color(0.340f, 0.357f, 0.357f, 1.0f);
        if (name.find("purple") != std::string::npos) return mce::Color(0.464f, 0.276f, 0.338f, 1.0f);
        if (name.find("blue") != std::string::npos) return mce::Color(0.291f, 0.234f, 0.357f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.303f, 0.201f, 0.140f, 1.0f);
        if (name.find("green") != std::string::npos) return mce::Color(0.298f, 0.327f, 0.166f, 1.0f);
        if (name.find("red") != std::string::npos) return mce::Color(0.561f, 0.239f, 0.184f, 1.0f);
        if (name.find("black") != std::string::npos) return mce::Color(0.146f, 0.090f, 0.064f, 1.0f);
        return mce::Color(0.597f, 0.369f, 0.266f, 1.0f);
    }

    // [红沙与红砂岩系列] 恶地/风蚀恶地地表与沙柱，原版真实贴图采样：红沙 RGB(191,103,33 #be6621)、红砂岩 RGB(181,98,32 #b5611f)
    // 必须在通用染色规则之前，杜绝恶地在地图上沦为鲜红血泊
    if (name.find("red_sandstone") != std::string::npos) return mce::Color(0.711f, 0.384f, 0.123f, 1.0f);
    if (name.find("red_sand") != std::string::npos) return mce::Color(0.748f, 0.404f, 0.130f, 1.0f);

    // [冰系列] blue_ice 必须在通用 blue_ 规则之前，避免误判为深皇家蓝
    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.70f, 0.92f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.44f, 0.57f, 0.80f, 1.0f);

    // [红色下界砖] 必须在通用 red_ 规则之前
    if (name.find("red_nether_brick") != std::string::npos) return mce::Color(0.38f, 0.08f, 0.10f, 1.0f);

    // [蘑菇与下界真菌] 必须在通用染色规则之前，避免 red_mushroom / brown_mushroom 被劫持
    if (name.find("mushroom") != std::string::npos || name.find("fungus") != std::string::npos || name.find("fungi") != std::string::npos) {
        if (name.find("red") != std::string::npos || name.find("crimson") != std::string::npos) return mce::Color(0.85f, 0.20f, 0.20f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
        if (name.find("warped") != std::string::npos) return mce::Color(0.15f, 0.55f, 0.50f, 1.0f);
        return mce::Color(0.80f, 0.70f, 0.60f, 1.0f);
    }

    // [树叶系列] 必须在通用染色规则之前，避免 red_poplar_leaves / yellow_poplar_leaves 等树叶被误染
    if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) {
        if (name.find("cherry") != std::string::npos) return mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
        if (name.find("mangrove") != std::string::npos) return mce::Color(0.32f, 0.40f, 0.09f, 1.0f);
        if (name.find("pale") != std::string::npos) return mce::Color(0.431f, 0.451f, 0.420f, 1.0f);
        if (name.find("birch") != std::string::npos) return mce::Color(0.263f, 0.341f, 0.173f, 1.0f);
        if (name.find("spruce") != std::string::npos || name.find("pine") != std::string::npos) return mce::Color(0.176f, 0.261f, 0.175f, 1.0f);
        return foliageCol;
    }

    // [通用 16 染色规则 (羊毛/地毯/混凝土/潜影盒/蜡烛/彩色玻璃等)]
    auto hasColor = [&](const char* col) -> bool {
        std::string p1 = std::string(col) + "_";
        if (name.rfind(p1, 0) == 0) return true;
        std::string p2 = std::string(":") + col + "_";
        if (name.find(p2) != std::string::npos) return true;
        std::string p3 = std::string("_") + col + "_";
        if (name.find(p3) != std::string::npos) return true;
        std::string p4 = std::string("_") + col;
        if (name.size() >= p4.size() && name.compare(name.size() - p4.size(), p4.size(), p4) == 0) return true;
        return false;
    };

    if (hasColor("white") || name.find("white_") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (hasColor("orange") || name.find("orange_") != std::string::npos) return mce::Color(0.85f, 0.50f, 0.20f, 1.0f);
    if (hasColor("magenta") || name.find("magenta_") != std::string::npos) return mce::Color(0.75f, 0.35f, 0.75f, 1.0f);
    if (hasColor("light_blue") || name.find("light_blue_") != std::string::npos) return mce::Color(0.40f, 0.65f, 0.90f, 1.0f);
    if (hasColor("yellow") || name.find("yellow_") != std::string::npos) return mce::Color(0.90f, 0.85f, 0.20f, 1.0f);
    if (hasColor("lime") || name.find("lime_") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.20f, 1.0f);
    if (hasColor("pink") || name.find("pink_") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (hasColor("light_gray") || hasColor("silver") || name.find("light_gray_") != std::string::npos || name.find("silver_") != std::string::npos) return mce::Color(0.65f, 0.65f, 0.65f, 1.0f);
    if (hasColor("gray") || name.find("gray_") != std::string::npos) return mce::Color(0.40f, 0.40f, 0.40f, 1.0f);
    if (hasColor("cyan") || name.find("cyan_") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.60f, 1.0f);
    if (hasColor("purple") || name.find("purple_") != std::string::npos) return mce::Color(0.50f, 0.25f, 0.60f, 1.0f);
    if (hasColor("blue") || name.find("blue_") != std::string::npos) return mce::Color(0.20f, 0.30f, 0.70f, 1.0f);
    if (hasColor("brown") || name.find("brown_") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);
    if (hasColor("green") || name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.50f, 0.20f, 1.0f);
    if (hasColor("red") || (name.find("red_") != std::string::npos && name.find("weathered_") == std::string::npos && name.find("powered_") == std::string::npos)) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    if (hasColor("black") || name.find("black_") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.15f, 1.0f);

    if (name.find("double_plant") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);

    if (name.find("warped_wart") != std::string::npos) return mce::Color(0.20f, 0.50f, 0.42f, 1.0f);
    if (name.find("wart") != std::string::npos) return mce::Color(0.65f, 0.10f, 0.10f, 1.0f);
    if (name.find("chorus") != std::string::npos) return mce::Color(0.60f, 0.40f, 0.60f, 1.0f);
    // [下界根/下界苗]
    if (name.find("crimson_roots") != std::string::npos) return mce::Color(0.55f, 0.10f, 0.10f, 1.0f);
    if (name.find("warped_roots") != std::string::npos) return mce::Color(0.15f, 0.40f, 0.35f, 1.0f);
    if (name.find("nether_sprouts") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);

    // [下界木] 必须在 grass 检查之前（"stem" 同时匹配作物茎和下界木，需优先处理下界木）
    if (name.find("crimson_stem") != std::string::npos || name.find("crimson_hyphae") != std::string::npos) return mce::Color(0.45f, 0.18f, 0.18f, 1.0f);
    if (name.find("warped_stem") != std::string::npos || name.find("warped_hyphae") != std::string::npos) return mce::Color(0.15f, 0.40f, 0.38f, 1.0f);
    // [垂泪藤/缠怨藤] 必须在通用 vine 规则之前
    if (name.find("weeping_vines") != std::string::npos) return mce::Color(0.45f, 0.08f, 0.08f, 1.0f);
    if (name.find("twisting_vines") != std::string::npos) return mce::Color(0.15f, 0.50f, 0.45f, 1.0f);
    // [苍白苔藓/苍白垂须] 必须在通用 moss 规则之前
    if (name.find("pale_moss") != std::string::npos || name.find("pale_hanging_moss") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.41f, 1.0f);

    // [仙人掌] 必须在植物通用规则前（仙人掌为固定特征墨绿色，不随生物群系染色）
    if (name.find("cactus") != std::string::npos) return mce::Color(0.33f, 0.52f, 0.18f, 1.0f);

    // [洞穴藤蔓/发光浆果] 必须在通用 vine 规则之前
    if (name.find("cave_vines") != std::string::npos) {
        if (name.find("berries") != std::string::npos || name.find("berry") != std::string::npos) {
            return mce::Color(0.78f, 0.55f, 0.18f, 1.0f);
        }
        return mce::Color(0.35f, 0.40f, 0.15f, 1.0f);
    }

    // [苔藓块与苔藓地毯] 原版不受生物群系染色影响，固定为浓郁温润的苔藓绿 RGB(89, 110, 45)
    if (name.find("moss") != std::string::npos) return mce::Color(0.35f, 0.43f, 0.18f, 1.0f);

    // [甘蔗/芦苇] 必须在植物通用规则前（Bedrock 名称为 reeds，不受群系染色影响，取清爽嫩绿）
    if (name.find("sugar_cane") != std::string::npos || name.find("reeds") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);

    if (name.find("grass") != std::string::npos || name.find("fern") != std::string::npos ||
        name.find("shrub") != std::string::npos || name.find("plant") != std::string::npos || name.find("vine") != std::string::npos ||
        name.find("sapling") != std::string::npos || name.find("propagule") != std::string::npos ||
        name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos || name.find("potato") != std::string::npos ||
        name.find("beetroot") != std::string::npos || name.find("crop") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("bush") != std::string::npos || name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos ||
        name.find("lichen") != std::string::npos || name.find("seed") != std::string::npos) {
        return grassCol;
    }
    // [灰化土] 表层枯叶深棕偏暗
    if (name.find("podzol") != std::string::npos) return mce::Color(0.38f, 0.27f, 0.14f, 1.0f);
    // [粗制泥土/砂土] 稍偏灰暗深棕
    if (name.find("coarse_dirt") != std::string::npos) return mce::Color(0.47f, 0.34f, 0.23f, 1.0f);
    // [缠根泥土]
    if (name.find("dirt_with_roots") != std::string::npos || (name.find("rooted") != std::string::npos && name.find("dirt") != std::string::npos)) {
        return mce::Color(0.56f, 0.41f, 0.30f, 1.0f);
    }
    // [泥土] 原版贴图均值 RGB(134, 96, 67)，温暖明亮自然大地色，消除原本偏深偏暗问题
    if (name.find("dirt") != std::string::npos) return mce::Color(0.53f, 0.38f, 0.26f, 1.0f);

    if (name.find("pumpkin") != std::string::npos || name.find("melon") != std::string::npos) {
        if (name.find("melon") != std::string::npos) return mce::Color(0.50f, 0.65f, 0.15f, 1.0f);
        return mce::Color(0.90f, 0.45f, 0.05f, 1.0f);
    }

    if (name.find("hay_block") != std::string::npos || name.find("hay_bale") != std::string::npos) return mce::Color(0.78f, 0.68f, 0.18f, 1.0f);
    if (name.find("bone_block") != std::string::npos) return mce::Color(0.88f, 0.86f, 0.78f, 1.0f);
    if (name.find("beehive") != std::string::npos || name.find("bee_nest") != std::string::npos) return mce::Color(0.85f, 0.65f, 0.28f, 1.0f);
    if (name.find("cake") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.78f, 1.0f);

    if (name.find("end_stone") != std::string::npos || name.find("end_brick") != std::string::npos) return mce::Color(0.539f, 0.633f, 0.422f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.890f, 0.840f, 0.680f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("glowstone") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.30f, 1.0f);
    if (name.find("lodestone") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
    if (name.find("end_portal") != std::string::npos) return mce::Color(0.10f, 0.25f, 0.25f, 1.0f);
    // [紫珀方块] 末地城标志性紫色方块（purpur_block/purpur_pillar/purpur_stairs/purpur_slab）
    // 必须在下方通用 stairs/slab 规则（约 L339）之前处理，否则紫珀楼梯/台阶会被误判为木色棕色
    // 实际贴图平均色约 RGB(153,108,172)，与紫水晶(0.58,0.48,0.68) 区分：紫珀更纯净、绿分量更低
    if (name.find("purpur") != std::string::npos) return mce::Color(0.60f, 0.42f, 0.68f, 1.0f);

    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("nether_wart") != std::string::npos) return mce::Color(0.60f, 0.14f, 0.14f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("lava") != std::string::npos) return mce::Color(0.95f, 0.30f, 0.0f, 1.0f);
    // [红色下界砖] 必须在 nether_brick 之前
    if (name.find("red_nether_brick") != std::string::npos) return mce::Color(0.38f, 0.08f, 0.10f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("gilded_blackstone") != std::string::npos) return mce::Color(0.22f, 0.18f, 0.12f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.18f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    // [哭泣黑曜石 / 重生锚] 必须在 obsidian 之前
    if (name.find("crying_obsidian") != std::string::npos) return mce::Color(0.20f, 0.08f, 0.28f, 1.0f);
    if (name.find("respawn_anchor") != std::string::npos) return mce::Color(0.25f, 0.12f, 0.32f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.06f, 0.04f, 0.09f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("quartz_ore") != std::string::npos) return mce::Color(0.65f, 0.55f, 0.55f, 1.0f);
    if (name.find("quartz") != std::string::npos) return mce::Color(0.92f, 0.90f, 0.86f, 1.0f);
    if (name.find("nether_gold_ore") != std::string::npos) return mce::Color(0.65f, 0.35f, 0.15f, 1.0f);

    // [杜鹃花丛与杜鹃树叶] 原版不受生物群系着色影响，为固定特征青润绿色，盛开型带花蕾粉紫调
    if (name.find("flowering_azalea") != std::string::npos) return mce::Color(0.48f, 0.46f, 0.32f, 1.0f);
    if (name.find("azalea") != std::string::npos) return mce::Color(0.40f, 0.49f, 0.19f, 1.0f);
    // [孢子花] 繁茂洞穴天花板花卉，标志性洋红色花瓣
    if (name.find("spore_blossom") != std::string::npos || name.find("spore") != std::string::npos) return mce::Color(0.81f, 0.38f, 0.62f, 1.0f);

    if (name.find("snow") != std::string::npos) return mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.330f, 0.250f, 0.200f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.265f, 0.200f, 0.160f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.880f, 0.830f, 0.660f, 1.0f);

    // [羊毛与地毯兜底（处理 Bedrock 无颜色前缀的 minecraft:wool / minecraft:carpet）]
    if (name.find("wool") != std::string::npos || name.find("carpet") != std::string::npos) return mce::Color(0.88f, 0.88f, 0.88f, 1.0f);
    // [混凝土与混凝土粉末兜底（处理 Bedrock 无颜色前缀的 minecraft:concrete / concretePowder）]
    if (name.find("concrete") != std::string::npos) return mce::Color(0.82f, 0.82f, 0.82f, 1.0f);
    // [潜影盒兜底（处理 Bedrock 无前缀 minecraft:shulker_box / undyed_shulker_box）]
    if (name.find("shulker_box") != std::string::npos) return mce::Color(0.58f, 0.40f, 0.60f, 1.0f);
    // [床（处理 Bedrock 无前缀 minecraft:bed）]
    if (name.find("bed") != std::string::npos && name.find("bedrock") == std::string::npos) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    // [蜡烛（暖黄蜡色）]
    if (name.find("candle") != std::string::npos) return mce::Color(0.85f, 0.75f, 0.55f, 1.0f);
    // [青蛙灯与青蛙卵]
    if (name.find("froglight") != std::string::npos) return mce::Color(0.92f, 0.90f, 0.80f, 1.0f);
    if (name.find("frog_spawn") != std::string::npos || name.find("frogspawn") != std::string::npos) return mce::Color(0.45f, 0.40f, 0.35f, 0.7f);

    // [苍白橡木] 灰白色木材，区别于普通橡木的暖棕色
    if (name.find("pale_oak") != std::string::npos) return mce::Color(0.68f, 0.66f, 0.60f, 1.0f);
    // [嘎枝之心] 苍白橡木质地，需在通用 wood 规则前处理（名称含 "wood"）
    if (name.find("creaking_heart") != std::string::npos) return mce::Color(0.55f, 0.52f, 0.48f, 1.0f);
    // [深板岩] 冷调深暗蓝灰岩石（原生贴图均值 RGB(80,80,83)），必须在 wood/stairs/slab 检查之前
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.33f, 1.0f);
    // [海晶石系列]
    if (name.find("prismarine") != std::string::npos) {
        if (name.find("dark") != std::string::npos) return mce::Color(0.20f, 0.35f, 0.30f, 1.0f);
        if (name.find("brick") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.55f, 1.0f);
        return mce::Color(0.45f, 0.65f, 0.50f, 1.0f);
    }
    // [珊瑚系列] Tube=蓝 Brain=粉 Bubble=紫 Fire=红 Horn=黄 Dead=灰
    if (name.find("coral") != std::string::npos) {
        if (name.find("tube") != std::string::npos) return mce::Color(0.20f, 0.35f, 0.75f, 1.0f);
        if (name.find("brain") != std::string::npos) return mce::Color(0.85f, 0.40f, 0.55f, 1.0f);
        if (name.find("bubble") != std::string::npos) return mce::Color(0.65f, 0.30f, 0.70f, 1.0f);
        if (name.find("fire") != std::string::npos) return mce::Color(0.75f, 0.25f, 0.25f, 1.0f);
        if (name.find("horn") != std::string::npos) return mce::Color(0.75f, 0.70f, 0.20f, 1.0f);
        if (name.find("dead") != std::string::npos) return mce::Color(0.70f, 0.70f, 0.70f, 1.0f);
        return mce::Color(0.50f, 0.80f, 0.80f, 1.0f);
    }
    // [工作台与功能容器类方块]
    if (name.find("anvil") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
    if (name.find("barrel") != std::string::npos) return mce::Color(0.53f, 0.40f, 0.24f, 1.0f);
    if (name.find("blast_furnace") != std::string::npos) return mce::Color(0.32f, 0.32f, 0.32f, 1.0f);
    if (name.find("smoker") != std::string::npos) return mce::Color(0.28f, 0.24f, 0.20f, 1.0f);
    if (name.find("hopper") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.30f, 1.0f);
    if (name.find("lectern") != std::string::npos) return mce::Color(0.65f, 0.48f, 0.25f, 1.0f);
    if (name.find("smithing_table") != std::string::npos) return mce::Color(0.22f, 0.24f, 0.28f, 1.0f);
    if (name.find("loom") != std::string::npos) return mce::Color(0.62f, 0.50f, 0.35f, 1.0f);
    if (name.find("cartography_table") != std::string::npos) return mce::Color(0.58f, 0.46f, 0.32f, 1.0f);
    if (name.find("fletching_table") != std::string::npos) return mce::Color(0.75f, 0.68f, 0.48f, 1.0f);
    if (name.find("crafting_table") != std::string::npos) return mce::Color(0.68f, 0.52f, 0.32f, 1.0f);
    if (name.find("grindstone") != std::string::npos) return mce::Color(0.50f, 0.50f, 0.50f, 1.0f);
    if (name.find("cauldron") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
    if (name.find("bell") != std::string::npos) return mce::Color(0.92f, 0.78f, 0.22f, 1.0f);
    if (name.find("beacon") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.85f, 1.0f);
    if (name.find("conduit") != std::string::npos) return mce::Color(0.40f, 0.75f, 0.75f, 1.0f);
    if (name.find("enchanting_table") != std::string::npos) return mce::Color(0.35f, 0.15f, 0.18f, 1.0f);
    if (name.find("brewing_stand") != std::string::npos) return mce::Color(0.50f, 0.45f, 0.40f, 1.0f);
    if (name.find("dispenser") != std::string::npos || name.find("dropper") != std::string::npos) return mce::Color(0.48f, 0.48f, 0.48f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("target") != std::string::npos) return mce::Color(0.85f, 0.72f, 0.55f, 1.0f);
    if (name.find("ender_chest") != std::string::npos) return mce::Color(0.12f, 0.18f, 0.18f, 1.0f);
    if (name.find("bookshelf") != std::string::npos) return mce::Color(0.58f, 0.42f, 0.24f, 1.0f);
    if (name.find("composter") != std::string::npos) return mce::Color(0.42f, 0.30f, 0.18f, 1.0f);
    // [熔炉/活塞/侦测器/TNT/音符盒/刷怪笼/传送门/铁轨等功能方块]
    if (name.find("furnace") != std::string::npos) return mce::Color(0.45f, 0.45f, 0.45f, 1.0f);
    if (name.find("piston") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("observer") != std::string::npos) return mce::Color(0.42f, 0.42f, 0.42f, 1.0f);
    if (name.find("tnt") != std::string::npos) return mce::Color(0.82f, 0.25f, 0.20f, 1.0f);
    if (name.find("jukebox") != std::string::npos || name.find("noteblock") != std::string::npos) return mce::Color(0.58f, 0.40f, 0.25f, 1.0f);
    if (name.find("spawner") != std::string::npos) return mce::Color(0.22f, 0.26f, 0.30f, 1.0f);
    if (name.find("portal") != std::string::npos) return mce::Color(0.48f, 0.18f, 0.72f, 0.85f);
    if (name.find("end_gateway") != std::string::npos) return mce::Color(0.05f, 0.05f, 0.10f, 1.0f);
    if (name.find("rail") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("command_block") != std::string::npos) return mce::Color(0.68f, 0.45f, 0.32f, 1.0f);
    if (name.find("banner") != std::string::npos) return mce::Color(0.70f, 0.60f, 0.40f, 1.0f);
    if (name.find("monster_egg") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
    if (name.find("netherreactor") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.80f, 1.0f);

    // [照明、营火与装饰]
    if (name.find("soul_campfire") != std::string::npos) return mce::Color(0.25f, 0.55f, 0.60f, 1.0f);
    if (name.find("campfire") != std::string::npos) return mce::Color(0.65f, 0.35f, 0.15f, 1.0f);
    if (name.find("soul_fire") != std::string::npos) return mce::Color(0.20f, 0.65f, 0.75f, 1.0f);
    if (name.find("fire") != std::string::npos) return mce::Color(0.95f, 0.55f, 0.10f, 1.0f);
    if (name.find("soul_lantern") != std::string::npos || name.find("soul_torch") != std::string::npos) return mce::Color(0.30f, 0.75f, 0.80f, 1.0f);
    if (name.find("lantern") != std::string::npos || name.find("torch") != std::string::npos) return mce::Color(0.95f, 0.80f, 0.40f, 1.0f);
    if (name.find("chain") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("scaffolding") != std::string::npos) return mce::Color(0.75f, 0.65f, 0.40f, 1.0f);
    if (name.find("web") != std::string::npos) return mce::Color(0.85f, 0.85f, 0.85f, 0.7f);
    if (name.find("ladder") != std::string::npos) return mce::Color(0.55f, 0.42f, 0.25f, 1.0f);
    if (name.find("lever") != std::string::npos || name.find("repeater") != std::string::npos || name.find("comparator") != std::string::npos) return mce::Color(0.55f, 0.50f, 0.50f, 1.0f);
    if (name.find("daylight_detector") != std::string::npos) return mce::Color(0.55f, 0.50f, 0.45f, 1.0f);
    if (name.find("lightning_rod") != std::string::npos) return mce::Color(0.72f, 0.45f, 0.28f, 1.0f);
    if (name.find("end_rod") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.90f, 1.0f);
    if (name.find("dragon_egg") != std::string::npos) return mce::Color(0.08f, 0.06f, 0.12f, 1.0f);
    if (name.find("decorated_pot") != std::string::npos) return mce::Color(0.62f, 0.38f, 0.25f, 1.0f);
    if (name.find("flower_pot") != std::string::npos) return mce::Color(0.55f, 0.30f, 0.20f, 1.0f);
    // [自然装饰与特殊生物衍生方块]
    if (name.find("sea_pickle") != std::string::npos) return mce::Color(0.42f, 0.60f, 0.22f, 1.0f);
    if (name.find("turtle_egg") != std::string::npos) return mce::Color(0.85f, 0.88f, 0.80f, 1.0f);
    if (name.find("cocoa") != std::string::npos) return mce::Color(0.68f, 0.38f, 0.18f, 1.0f);
    if (name.find("skull") != std::string::npos || name.find("head") != std::string::npos) return mce::Color(0.68f, 0.65f, 0.60f, 1.0f);
    if (name.find("hanging_roots") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.20f, 1.0f);

    // [矿石] 统一处理所有矿石方块，每种带特征矿物色调（必须在金属纯块/粗矿/金属规则前）
    if (name.find("ore") != std::string::npos) {
        if (name.find("gold") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.20f, 1.0f);
        if (name.find("iron") != std::string::npos) return mce::Color(0.50f, 0.42f, 0.32f, 1.0f);
        if (name.find("copper") != std::string::npos) return mce::Color(0.55f, 0.38f, 0.25f, 1.0f);
        if (name.find("diamond") != std::string::npos) return mce::Color(0.42f, 0.52f, 0.55f, 1.0f);
        if (name.find("emerald") != std::string::npos) return mce::Color(0.30f, 0.52f, 0.35f, 1.0f);
        if (name.find("lapis") != std::string::npos) return mce::Color(0.22f, 0.35f, 0.55f, 1.0f);
        if (name.find("coal") != std::string::npos) return mce::Color(0.25f, 0.25f, 0.25f, 1.0f);
        return mce::Color(0.45f, 0.45f, 0.45f, 1.0f);
    }

    // [粗矿块] 棕色调（必须在 iron/copper 规则前）
    if (name.find("raw_") != std::string::npos) return mce::Color(0.50f, 0.38f, 0.25f, 1.0f);

    // [金属与宝石纯块]
    if (name.find("gold_block") != std::string::npos || name.find("block_of_gold") != std::string::npos) return mce::Color(0.98f, 0.85f, 0.25f, 1.0f);
    if (name.find("diamond_block") != std::string::npos || name.find("block_of_diamond") != std::string::npos) return mce::Color(0.38f, 0.85f, 0.82f, 1.0f);
    if (name.find("emerald_block") != std::string::npos || name.find("block_of_emerald") != std::string::npos) return mce::Color(0.20f, 0.78f, 0.35f, 1.0f);
    if (name.find("lapis_block") != std::string::npos || name.find("block_of_lapis") != std::string::npos) return mce::Color(0.12f, 0.28f, 0.65f, 1.0f);
    if (name.find("netherite_block") != std::string::npos || name.find("block_of_netherite") != std::string::npos) return mce::Color(0.22f, 0.20f, 0.22f, 1.0f);
    if (name.find("coal_block") != std::string::npos || name.find("block_of_coal") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.12f, 1.0f);
    if (name.find("ancient_debris") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.24f, 1.0f);

    // [铜与氧化阶段] 必须涵盖所有变体（cut, chiseled, grate, bulb, door, stairs, slab 等）
    if (name.find("copper") != std::string::npos) {
        if (name.find("oxidized") != std::string::npos) return mce::Color(0.30f, 0.55f, 0.50f, 1.0f);
        if (name.find("weathered") != std::string::npos) return mce::Color(0.35f, 0.50f, 0.40f, 1.0f);
        if (name.find("exposed") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.35f, 1.0f);
        return mce::Color(0.72f, 0.45f, 0.28f, 1.0f);
    }

    // [铁质方块与铁门/铁栏杆]
    if (name.find("iron") != std::string::npos) return mce::Color(0.72f, 0.72f, 0.72f, 1.0f);

    // [木种精准色彩区分] 必须在 generic planks/stairs/slab 前处理
    // 1. 云杉木：完整保留已调校精准参数（村庄屋顶原木 0.212, 0.149, 0.075）
    if (name.find("spruce") != std::string::npos) {
        if (name.find("stripped") != std::string::npos) {
            return mce::Color(0.45f, 0.34f, 0.20f, 1.0f); // 去皮云杉木
        }
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            return mce::Color(0.212f, 0.149f, 0.075f, 1.0f); // 云杉原木/云杉木
        }
        return mce::Color(0.45f, 0.33f, 0.19f, 1.0f); // 云杉木板/楼梯/台阶等
    }

    // 2. 樱花木 (木质浅粉，树皮暗褐)
    if (name.find("cherry") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.89f, 0.70f, 0.68f, 1.0f);
            }
            return mce::Color(0.35f, 0.25f, 0.25f, 1.0f);
        }
        return mce::Color(0.89f, 0.70f, 0.68f, 1.0f);
    }

    // 3. 红树木 (深红褐泥红色)
    if (name.find("mangrove") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.48f, 0.22f, 0.18f, 1.0f);
            }
            return mce::Color(0.35f, 0.20f, 0.15f, 1.0f);
        }
        return mce::Color(0.48f, 0.22f, 0.18f, 1.0f);
    }

    // 4. 金合欢木 (标志性鲜橙色)
    if (name.find("acacia") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.68f, 0.38f, 0.20f, 1.0f);
            }
            return mce::Color(0.40f, 0.38f, 0.35f, 1.0f);
        }
        return mce::Color(0.68f, 0.38f, 0.20f, 1.0f);
    }

    // 5. 白桦木 (白底黑斑树皮，淡米黄木质)
    if (name.find("birch") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.82f, 0.74f, 0.55f, 1.0f);
            }
            return mce::Color(0.82f, 0.82f, 0.80f, 1.0f);
        }
        return mce::Color(0.82f, 0.74f, 0.55f, 1.0f);
    }

    // 6. 深色橡木 (浓郁黑巧深褐色)
    if (name.find("dark_oak") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.28f, 0.18f, 0.10f, 1.0f);
            }
            return mce::Color(0.22f, 0.14f, 0.08f, 1.0f);
        }
        return mce::Color(0.28f, 0.18f, 0.10f, 1.0f);
    }

    // 7. 丛林木 (微红暖木色)
    if (name.find("jungle") != std::string::npos) {
        if (name.find("log") != std::string::npos || name.find("wood") != std::string::npos) {
            if (name.find("stripped") != std::string::npos) {
                return mce::Color(0.60f, 0.42f, 0.28f, 1.0f);
            }
            return mce::Color(0.35f, 0.30f, 0.18f, 1.0f);
        }
        return mce::Color(0.60f, 0.42f, 0.28f, 1.0f);
    }

    // 8. 绯红与诡异下界木制品 (门/楼梯/台阶/栅栏等)
    if (name.find("crimson") != std::string::npos) return mce::Color(0.48f, 0.18f, 0.24f, 1.0f);
    if (name.find("warped") != std::string::npos) return mce::Color(0.18f, 0.42f, 0.40f, 1.0f);

    // [泥方块/泥砖/带根红树根] 泥土深褐色（必须在 stone/brick 规则前）
    if (name.find("mud") != std::string::npos) {
        if (name.find("brick") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.20f, 1.0f);
        return mce::Color(0.34f, 0.28f, 0.24f, 1.0f); // 泥巴方块与泥化红树根：深灰泥褐色
    }

    // [滴水石与垂滴叶]（含 "stone" 但应偏棕色，需在 stone 检查前处理）
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    if (name.find("dripleaf") != std::string::npos) return mce::Color(0.44f, 0.56f, 0.20f, 1.0f);

    // [石材与砖石变体] 必须在 generic stairs/slab 之前，确保 stone_stairs/brick_stairs 不会被误判为木头
    if (name.find("granite") != std::string::npos) return mce::Color(0.60f, 0.45f, 0.38f, 1.0f);
    if (name.find("diorite") != std::string::npos) return mce::Color(0.80f, 0.80f, 0.80f, 1.0f);
    if (name.find("andesite") != std::string::npos) return mce::Color(0.52f, 0.52f, 0.52f, 1.0f);
    if (name.find("tuff") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.40f, 1.0f);
    if (name.find("brick") != std::string::npos) return mce::Color(0.62f, 0.35f, 0.28f, 1.0f);
    if (name.find("stone") != std::string::npos || name.find("cobble") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    // [通用木质与结构兜底]
    if (name.find("planks") != std::string::npos || name.find("oak") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos || name.find("fence") != std::string::npos ||
        name.find("door") != std::string::npos || name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos ||
        name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);

    // [基岩] 极深灰色
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);

    // [海绵] 淡黄色
    if (name.find("sponge") != std::string::npos) return mce::Color(0.76f, 0.71f, 0.31f, 1.0f);
    // [黏液块] 浅绿色半透明
    if (name.find("slime") != std::string::npos) return mce::Color(0.49f, 0.74f, 0.35f, 1.0f);
    // [蜂蜜系列] 金黄/橙黄色
    if (name.find("honeycomb") != std::string::npos) return mce::Color(0.81f, 0.53f, 0.15f, 1.0f);
    if (name.find("honey_block") != std::string::npos) return mce::Color(0.89f, 0.58f, 0.12f, 1.0f);
    // [幽匿系列] 标志性深暗青翠/荧光青蓝色，告别纯黑死色
    if (name.find("sculk_catalyst") != std::string::npos) return mce::Color(0.20f, 0.32f, 0.35f, 1.0f);
    if (name.find("sculk_shrieker") != std::string::npos) return mce::Color(0.48f, 0.45f, 0.38f, 1.0f);
    if (name.find("calibrated_sculk_sensor") != std::string::npos) return mce::Color(0.35f, 0.30f, 0.50f, 1.0f);
    if (name.find("sculk_sensor") != std::string::npos) return mce::Color(0.07f, 0.38f, 0.42f, 1.0f);
    if (name.find("sculk_vein") != std::string::npos) return mce::Color(0.08f, 0.32f, 0.36f, 1.0f);
    if (name.find("sculk") != std::string::npos) return mce::Color(0.07f, 0.28f, 0.32f, 1.0f);
    // [树脂系列] 琥珀橙黄色
    if (name.find("resin") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.12f, 1.0f);
    // [沉重核心] 深灰蓝色（1.21 重锤相关）
    if (name.find("heavy_core") != std::string::npos) return mce::Color(0.24f, 0.25f, 0.29f, 1.0f);
    // [试炼大厅 1.21] 刷怪笼、自动合成器、宝库
    if (name.find("trial_spawner") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.24f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("vault") != std::string::npos) return mce::Color(0.48f, 0.40f, 0.35f, 1.0f);
    // [嗅探兽蛋]
    if (name.find("sniffer_egg") != std::string::npos) return mce::Color(0.78f, 0.38f, 0.22f, 1.0f);
    // [火把与灯笼]
    if (name.find("soul_torch") != std::string::npos || name.find("soul_lantern") != std::string::npos) return mce::Color(0.35f, 0.80f, 0.85f, 1.0f);
    if (name.find("torch") != std::string::npos || name.find("lantern") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.35f, 1.0f);
    // [教育版与实验性方块]
    if (name.find("cinnabar") != std::string::npos) return mce::Color(0.68f, 0.22f, 0.20f, 1.0f);
    if (name.find("sulfur") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.22f, 1.0f);
    if (name.find("dried_ghast") != std::string::npos) return mce::Color(0.90f, 0.90f, 0.90f, 1.0f);
    if (name.find("shelf") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("chemistry_table") != std::string::npos || name.find("lab_table") != std::string::npos ||
        name.find("compound_creator") != std::string::npos || name.find("chemical_heat") != std::string::npos) {
        return mce::Color(0.70f, 0.70f, 0.72f, 1.0f);
    }
    // [红树根] 深棕色
    if (name.find("mangrove_roots") != std::string::npos) return mce::Color(0.43f, 0.27f, 0.16f, 1.0f);
    // [菌丝体] 灰紫色（蘑菇岛地表）
    if (name.find("mycelium") != std::string::npos) return mce::Color(0.48f, 0.42f, 0.42f, 1.0f);
    // [紫水晶] 淡紫色
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.58f, 0.48f, 0.68f, 1.0f);
    // [方解石] 原版为平滑的灰白/米白色方块
    if (name.find("calcite") != std::string::npos) return mce::Color(0.86f, 0.86f, 0.82f, 1.0f);

    // [黏土] 标志性浅灰蓝黏土，自然柔和不刺眼
    if (name.find("clay") != std::string::npos) return mce::Color(0.58f, 0.61f, 0.66f, 1.0f);

    // [墙、沙砾兜底]
    if (name.find("wall") != std::string::npos || name.find("gravel") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    LogColorMiss("BlockMiss", name);

    // [步骤2安全兜底] 彻底终结伪随机哈希杂色，未知方块以自然中性岩石灰融入地貌
    return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);
}

// ==========================================
// [传送日志器] 文件级日志，便于调试与问题追踪
// 日志路径：<mod_data_dir>/teleport_log.txt，按行追加
// ==========================================
inline void LogTeleport(const std::string& message) {
    try {
        static std::string logPath;
        if (logPath.empty()) {
            auto dataDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir();
            logPath = (dataDir / "teleport_log.txt").string();
        }
        std::ofstream log(logPath, std::ios::app);
        if (log) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
            localtime_s(&tm_buf, &time);
            char timeBuf[64];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tm_buf);
            log << "[" << timeBuf << "] " << message << std::endl;
        }
    } catch (...) {
        // 日志失败绝不影响传送主流程
    }
}

// ==========================================
// [服务器指令发送器] 通过 CommandRequestPacket 直发指令
// 等价于聊天栏输入指令，服务器端执行，避免客户端回弹
// ==========================================
inline void SendServerCommand(Player& player, std::string const& cmd) {
    auto origin = std::make_unique<PlayerCommandOrigin>(player);
    CommandContext ctx(cmd, std::move(origin), (int)CurrentCmdVersion::Latest);
    CommandRequestPacketPayload payload(ctx, false);
    CommandRequestPacket packet(std::move(payload));
    packet.sendToServer();
}

// ==========================================
// [安全地表高度查询] SEH 包装，防止部分加载区块的 NULL subchunk 解引用崩溃
// 崩溃根因：Phase 0 传送到未访问区域后，区块处于部分加载状态（部分 subchunk 指针为 NULL）
//   getAboveTopSolidBlock 遍历 subchunk 时执行 mov rax,[rax+0x08] (RAX=0) → 0xC0000005
// 解决方案：__try/__except 捕获 AV，返回 -32000 哨兵表示"区块未就绪"，调用方重试
// 注意：__try/__except 不能与 C++ 对象析构共存，故本函数仅使用 POD 类型
// ==========================================
inline short SafeGetSurfaceY(BlockSource& region, int x, int z) noexcept {
    short result = -32000;
    __try {
        // includeUnloaded=true 保留原行为（对已加载区块返回真实 Y，对完全未加载返回 ≤-64）
        // 部分加载区块的 NULL subchunk AV 由 __except 兜底
        result = region.getAboveTopSolidBlock(x, z, true, true);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // 部分加载区块触发 AV → 视为"未就绪"，调用方下次重试
        result = -32000;
    }
    return result;
}

// ==========================================
// [安全生物群系名查询] try/catch 包装，防止部分加载区块 AV
// 与 SafeGetSurfaceY 的区别：getBiome 返回 Biome const& 并需 std::string 赋值（含析构），
//   故不能用 __try/__except（不能与 C++ 对象析构共存），改用 try/catch(...)
//   在 /EHa 模式下 catch(...) 可捕获 SEH AV，与 PlayerHook.h:616,773 现有调用风格一致
// ==========================================
inline bool SafeGetBiomeName(BlockSource& region, int x, int y, int z,
                             std::string& outRawName) noexcept {
    try {
        auto const& biome = region.getBiome(BlockPos(x, y, z));
        outRawName = biome.mHash->getString();
        return !outRawName.empty();
    } catch (...) {
        // 部分加载区块触发 AV/其他异常 → 视为"未就绪"，调用方下次重试
        return false;
    }
}

// [辅助] 判断方块名是否为液体（水/熔岩）— 传送安全共用
inline bool IsLiquidBlockName(std::string const& name) noexcept {
    return name.find("water") != std::string::npos ||
           name.find("lava") != std::string::npos;
}

// [辅助] 判断方块名是否为空气/可穿透（玩家可站立其中）— 传送安全共用
inline bool IsAirLikeName(std::string const& name) noexcept {
    if (name.empty()) return false;
    if (name.find("air") != std::string::npos) return true;
    if (name.find("barrier") != std::string::npos) return true;
    if (name.find("light_block") != std::string::npos) return true;
    if (name.find("structure_void") != std::string::npos) return true;
    if (name.find("placeholder") != std::string::npos) return true;
    if (name.find("info_update") != std::string::npos) return true;
    if (name.find("invisible_bedrock") != std::string::npos || name.find("invisiblebedrock") != std::string::npos) return true;
    if (name.find("moving_block") != std::string::npos || name.find("movingblock") != std::string::npos) return true;
    if (name.find("piston_arm_collision") != std::string::npos || name.find("pistonarmcollision") != std::string::npos) return true;
    if (name.find("border_block") != std::string::npos) return true;
    return false;
}

// [辅助] 判断方块名是否为"石头类"灰色污染候选
// 地表扫描中, 部分加载区块 SafeGetSurfaceY 可能返回洞穴天花板Y, 读取到石头/深板岩等,
// 写入缓存造成地表大地图灰色块状污染。检测到此类方块时回退查缓存地表Y重新读取。
inline bool IsStoneLikeBlock(std::string const& name) noexcept {
    // stone 必须先于 sandstone/redstone 等含 "stone" 子串的非石头方块排除
    if (name.find("sandstone") != std::string::npos) return false;
    if (name.find("redstone") != std::string::npos) return false;
    if (name.find("glowstone") != std::string::npos) return false;
    if (name.find("lodestone") != std::string::npos) return false;
    if (name.find("end_stone") != std::string::npos) return false;
    if (name.find("blackstone") != std::string::npos) return false;
    if (name.find("dripstone") != std::string::npos) return false;
    if (name.find("moss_block") != std::string::npos) return false;
    // 真正的石头类: stone/cobblestone/andesite/diorite/granite/deepslate/tuff/bedrock/gravel
    return name.find("stone") != std::string::npos ||
           name.find("cobble") != std::string::npos ||
           name.find("andesite") != std::string::npos ||
           name.find("diorite") != std::string::npos ||
           name.find("granite") != std::string::npos ||
           name.find("deepslate") != std::string::npos ||
           name.find("tuff") != std::string::npos ||
           name.find("bedrock") != std::string::npos ||
           name.find("gravel") != std::string::npos;
}

// ==========================================
// [安全方块查询] SEH 包装的 getBlock 查询
// 与 SafeGetSurfaceY 同模式：__try/__except 仅用 POD + out 参数
// 避免 std::string 析构与 SEH 冲突（v1-v6 教训：部分加载 chunk 的 NULL subchunk 指针引发 0xC0000005）
// ==========================================
inline bool SafeGetBlockName(BlockSource& region, int x, int y, int z, std::string& outName) noexcept {
    __try {
        auto const& block = region.getBlock(BlockPos(x, y, z));
        outName = block.getTypeName();
        return !outName.empty();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outName.clear();
        return false;
    }
}

// ==================== 洞穴材质判定 (汲取 0.3.4 优势: Material枚举精准识别通道/水体) ====================
// Xaero 风格洞穴分层投影：所有列从同一个 Top Y 向下解析，避免相邻列跳到不同高度层。
inline constexpr int kCaveLayerTopOffset = 3;
inline constexpr int kCaveLayerAirSearchDepth = 64;
inline constexpr int kCaveLayerFloorSearchDepth = 64;

inline ChiyanMapMaterialType GetCaveMaterialType(Block const& block) {
    return block.getBlockType().mMaterial.mType;
}

inline bool IsCaveWaterBlock(Block const& block) {
    if (block.isAir()) return false;
    ChiyanMapMaterialType material = GetCaveMaterialType(block);
    return material == ChiyanMapMaterialType::Water || material == ChiyanMapMaterialType::Bubble;
}

inline constexpr float kWaterOverlayAlpha = 0.65f;
inline constexpr mce::Color kDefaultWaterTint(0.20f, 0.52f, 0.88f, 1.0f);

// 水色叠加在洞底颜色之上；避免液体单独饱和色显得突兀。
inline mce::Color BlendWaterOverFloor(mce::Color floorColor, mce::Color waterTint) {
    if (waterTint.a <= 0.01f) waterTint = kDefaultWaterTint;
    return mce::Color(
        floorColor.r + (waterTint.r - floorColor.r) * kWaterOverlayAlpha,
        floorColor.g + (waterTint.g - floorColor.g) * kWaterOverlayAlpha,
        floorColor.b + (waterTint.b - floorColor.b) * kWaterOverlayAlpha,
        floorColor.a
    );
}

// 洞穴投影和自动判定共享此规则：流体、植被与透明方块都不形成洞穴墙体。
// 相比字符串匹配，Material 枚举更精准，避免漏判新方块导致黑灰/红块。
inline bool IsCavePassableBlock(Block const& block) {
    if (block.isAir()) return true;
    ChiyanMapMaterialType material = GetCaveMaterialType(block);
    switch (material) {
    case ChiyanMapMaterialType::Air:
    case ChiyanMapMaterialType::Water:
    case ChiyanMapMaterialType::Bubble:
    case ChiyanMapMaterialType::Plant:
    case ChiyanMapMaterialType::SolidPlant:
    case ChiyanMapMaterialType::Leaves:
    case ChiyanMapMaterialType::Glass:
    case ChiyanMapMaterialType::Ice:
    case ChiyanMapMaterialType::PowderSnow:
    case ChiyanMapMaterialType::Cactus:
    case ChiyanMapMaterialType::Fire:
    case ChiyanMapMaterialType::Portal:
    case ChiyanMapMaterialType::Grate:
    case ChiyanMapMaterialType::StoneDecoration:
    case ChiyanMapMaterialType::DecorationSolid:
    case ChiyanMapMaterialType::NonSolid:
    case ChiyanMapMaterialType::StructureVoid:
        return true;
    default:
        break;
    }
    if (material == ChiyanMapMaterialType::Wood) {
        std::string const& name = block.getTypeName();
        return name.find("log") != std::string::npos || name.find("stem") != std::string::npos;
    }
    return false;
}

// ==================== 洞穴地图系统 (Xaero's Cave Map 1:1 复刻) ====================

// [洞穴辅助] 判断方块是否为"覆盖层"（透明/非实心），在洞穴列扫描中跳过
// 对应 Xaero's MapWriter.isInvisible: air, liquid, glass, torch, grass, flowers, leaves
inline bool IsCaveOverlayBlockName(std::string const& rawName) noexcept {
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    if (IsAirLikeName(name)) return true;
    if (IsLiquidBlockName(name)) return true;  // 液体在洞穴扫描中视为可穿透层
    if (name.find("glass") != std::string::npos) return true;
    if (name.find("torch") != std::string::npos || name.find("lantern") != std::string::npos) return true;
    if (name.find("chain") != std::string::npos) return true;
    if (name.find("pale_hanging_moss") != std::string::npos) return true;
    if (name.find("pointed_dripstone") != std::string::npos) return true;
    if (name.find("amethyst_cluster") != std::string::npos || name.find("amethyst_bud") != std::string::npos) return true;
    if (name.find("grass") != std::string::npos || name.find("fern") != std::string::npos) return true;
    if (name.find("flower") != std::string::npos || name.find("bush") != std::string::npos) return true;
    if (name.find("leaves") != std::string::npos) return true;
    if (name.find("vine") != std::string::npos || name.find("lichen") != std::string::npos) return true;
    if (name.find("roots") != std::string::npos || name.find("dripleaf") != std::string::npos) return true;
    if (name.find("sculk_vein") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("mushroom") != std::string::npos || name.find("spore") != std::string::npos) return true;
    if (name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos) return true;
    if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) return true;
    if (name.find("rail") != std::string::npos) return true;
    if (name.find("ladder") != std::string::npos) return true;
    if (name.find("sign") != std::string::npos) return true;
    if (name.find("banner") != std::string::npos) return true;
    if (name.find("button") != std::string::npos) return true;
    if (name.find("pressure_plate") != std::string::npos) return true;
    if (name.find("lever") != std::string::npos) return true;
    if (name.find("door") != std::string::npos) return true;
    if (name.find("trapdoor") != std::string::npos) return true;
    if (name.find("fence") != std::string::npos) return true;
    if (name.find("wall") != std::string::npos && name.find("cobblestone_wall") == std::string::npos) return true;
    if (name.find("carpet") != std::string::npos) return true;
    if (name.find("web") != std::string::npos) return true;
    if (name.find("lily") != std::string::npos) return true;
    if (name.find("candle") != std::string::npos) return true;
    if (name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos ||
        name.find("potato") != std::string::npos || name.find("beetroot") != std::string::npos ||
        name.find("crop") != std::string::npos) return true;
    return false;
}

// [辅助] 安全判断指定坐标是否为可穿透/非实心天花板阻挡方块
inline bool IsPassableCeilingBlock(BlockSource& region, int x, int y, int z) noexcept {
    std::string name;
    if (!SafeGetBlockName(region, x, y, z, name)) return true;
    if (name.empty()) return true;
    if (IsAirLikeName(name) || IsCaveOverlayBlockName(name) || IsLiquidBlockName(name)) return true;
    if (name.find("log") != std::string::npos || name.find("stem") != std::string::npos) return true;
    return false;
}

// [洞穴检测] 智能检测玩家是否身处真正的地下洞穴中
// 彻底解决巨型洞穴误显地表、深层洞穴变黑、以及浮空岛/大树冠/屋檐误判为洞穴的问题
// 返回值: true=在洞穴中, caveStartY=洞穴起始Y
inline bool DetectCaveStart(BlockSource& region, int playerX, int playerY, int playerZ, int& outCaveStartY) noexcept {
    if (playerY <= -64 || playerY >= 315) return false;

    // 1. 获取玩家所在列的最高地表高度
    short surfaceY = SafeGetSurfaceY(region, playerX, playerZ);

    // 如果玩家自身高度已经等于或超过最高地表高度 (例如站在开阔地表、山顶、高台或悬崖顶)，直接为地表
    if (surfaceY > -64 && surfaceY != -32000 && surfaceY <= playerY + 2) return false;

    // 2. 深层地下快速通道 (playerY < 55):
    // 主世界海平面为 62，地下 Y < 55 绝无自然浮空岛、树冠或人工遮阳棚。
    // 在负高度 (如 Y = -9, Y = -46) 及海平面以下，只要处于封闭岩体或深裂谷中，100% 为地下洞穴。
    if (playerY < 55) {
        // 向上寻找首个实心岩石/深板岩/泥土天花板 (搜索上限放宽至 80 格或 surfaceY，覆盖 1.18+ 巨型穹顶洞穴)
        int ceilingY = -1;
        int maxScan = (surfaceY > playerY + 2 && surfaceY != -32000) ? std::min((int)surfaceY, playerY + 80) : std::min(319, playerY + 80);
        for (int y = playerY + 2; y <= maxScan; y++) {
            if (!IsPassableCeilingBlock(region, playerX, y, playerZ)) {
                ceilingY = y;
                break;
            }
        }
        if (ceilingY != -1) {
            // 命中实心天花板：无需做多层空腔 airAbove 检查 (真实洞穴常有上下多层空腔/废弃矿井/裂隙)
            outCaveStartY = ceilingY;
            return true;
        }

        // 头顶未在 80 格内探测到实心天花板 (超巨型开敞裂谷/天坑深渊):
        // 若地表高度远高于玩家 (surfaceY > playerY + 8)，玩家身处深渊裂谷底部，依然应显示裂谷而非高空地表
        if (surfaceY > playerY + 8 && surfaceY != -32000) {
            // 排除开阔海洋游泳 (若从脚下到地表全为水体且无天花板，则为海洋地表)
            std::string currentBlock;
            if (SafeGetBlockName(region, playerX, playerY, playerZ, currentBlock) &&
                IsLiquidBlockName(currentBlock)) {
                return false;
            }
            outCaveStartY = std::min((int)surfaceY - 1, playerY + 40);
            return true;
        }

        // 负高度极其深层 (如 Y < 0 或 Y < 30): 在主世界绝不可能为露天地表，即使区块加载延迟也视为地下
        if (playerY < 30) {
            outCaveStartY = playerY + 24;
            return true;
        }

        return false;
    }

    // 3. 高空与近地表环境检测 (playerY >= 55):
    // 此时玩家可能处于山峰、森林、房屋遮阳棚、悬崖突出岩壁或半山腰洞穴中。
    // 需严格过滤浮空岛、树冠、建筑屋檐，避免误判。
    if (surfaceY <= -64 || surfaceY == -32000) return false;  // 未加载区块

    // 向上探测玩家头顶的第一个实心天花板 (非空气、非植被、非树叶/原木、非透明方块)
    int ceilingY = -1;
    int maxScanCeilingY = std::min((int)surfaceY, playerY + 60);
    for (int y = playerY + 2; y <= maxScanCeilingY; y++) {
        if (!IsPassableCeilingBlock(region, playerX, y, playerZ)) {
            ceilingY = y;
            break;
        }
    }
    if (ceilingY == -1) return false;

    // 检查天花板厚度: 若天花板极薄 (<= 2 格实心方块，如木板挑檐/遮阳板/薄石拱桥)
    int solidThickness = 0;
    for (int y = ceilingY; y <= (int)surfaceY && solidThickness <= 4; y++) {
        if (IsPassableCeilingBlock(region, playerX, y, playerZ)) {
            break;
        }
        solidThickness++;
    }
    if (solidThickness <= 2) return false;

    // 头顶净空高度检测 (针对高空浮空岛/高空突出悬崖):
    int clearance = ceilingY - (playerY + 2);
    if (clearance > 8) {
        std::string floorName;
        if (SafeGetBlockName(region, playerX, playerY - 1, playerZ, floorName)) {
            if (floorName.find("grass_block") != std::string::npos ||
                floorName.find("dirt_with_roots") != std::string::npos ||
                floorName.find("podzol") != std::string::npos ||
                floorName.find("mycelium") != std::string::npos) {
                return false;
            }
        }
    }

    // 四周地表采样判决:
    static const struct { int dx, dz; } kSampleOffsets[] = {
        { -8,   0 }, {  8,   0 }, {  0,  -8 }, {  0,   8 },
        { -14,  0 }, { 14,   0 }, {  0, -14 }, {  0,  14 },
        { -10, -10 }, { 10, -10 }, { -10, 10 }, { 10, 10 }
    };

    int openSurfaceCount = 0;
    int validSampleCount = 0;
    for (const auto& offset : kSampleOffsets) {
        short sY = SafeGetSurfaceY(region, playerX + offset.dx, playerZ + offset.dz);
        if (sY <= -64 || sY == -32000) continue;
        validSampleCount++;
        if (sY <= playerY + 5) {
            openSurfaceCount++;
        }
    }

    if (validSampleCount >= 4 && openSurfaceCount >= 2) {
        return false;
    }
    if (validSampleCount > 0 && openSurfaceCount >= 1 && clearance > 6) {
        return false;
    }

    // 周边有露天采样点且脚下为草方块，直接排除洞穴
    if (openSurfaceCount > 0) {
        std::string floorName;
        if (SafeGetBlockName(region, playerX, playerY - 1, playerZ, floorName)) {
            if (floorName.find("grass_block") != std::string::npos) {
                return false;
            }
        }
    }

    // 针对特大浮空岛 (半径超过 14 格): 扩展至 24 格采样
    if (clearance > 8 || playerY >= 62) {
        static const struct { int dx, dz; } kFarOffsets[] = {
            { -24, 0 }, { 24, 0 }, { 0, -24 }, { 0, 24 },
            { -18, -18 }, { 18, -18 }, { -18, 18 }, { 18, 18 }
        };
        int farOpenCount = 0;
        int farValidCount = 0;
        for (const auto& offset : kFarOffsets) {
            short sY = SafeGetSurfaceY(region, playerX + offset.dx, playerZ + offset.dz);
            if (sY <= -64 || sY == -32000) continue;
            farValidCount++;
            if (sY <= playerY + 6) {
                farOpenCount++;
            }
        }
        if (farValidCount >= 4 && farOpenCount >= 2) {
            return false;
        }
        if (farValidCount > 0 && farOpenCount >= 1 && clearance > 10) {
            return false;
        }
    }

    outCaveStartY = ceilingY;
    return true;
}

// [洞穴亮度计算] 深度衰减亮度公式
// 对应 Xaero's MapPixel.getPixelColours 中的深度衰减:
//   legible=false (非清晰): finalBrightness = 0.375 + 0.625 * (1 - depth/caveDepth)
//   legible=true (清晰): finalBrightness = 1.0 (全亮, 由深度因子直接乘RGB)
// 参数: depth = 从 caveStart 向下的层数, caveDepth = 最大扫描深度
inline float ComputeCaveBrightness(int depth, int caveDepth) noexcept {
    if (caveDepth <= 0) return 1.0f;
    float t = 1.0f - std::clamp((float)depth / (float)caveDepth, 0.0f, 1.0f);
    if (MapRenderState::g_legibleCaveMaps) {
        return 1.0f;  // 清晰模式: 全亮, 深度通过 RGB 乘子控制
    }
    return 0.375f + 0.625f * t;  // 非清晰模式: 0.375(底) ~ 1.0(顶)
}

// [洞穴列扫描] 扫描单个 x,z 列, 从 startY 向下查找第一个紧邻空气的实心方块
// 对应 Xaero's MapWriter.loadPixel: 从 caveStart 向下扫描到 caveStart - caveDepth
// 核心逻辑: 只渲染洞穴空腔下方的方块 (洞穴地板/墙壁), 纯石头区域返回 false (透明)
// 液体方块 (熔岩/水) 在空气下方时返回, 由 GetCaveLiquidColor 应用饱和色 (不受深度衰减)
// 参数: region, x, z, startY(扫描起点Y), caveDepth(扫描深度)
// 输出: outBlockName(方块名), outBlockY(方块Y), outDepth(深度), outHasWater(是否含水)
// 返回值: true=找到洞穴方块, false=列内无洞穴 (纯实心石头, 渲染为透明)
inline bool ScanColumnCave(BlockSource& region, int x, int z, int startY, int caveDepth,
                           std::string& outBlockName, int& outBlockY, int& outDepth, bool& outHasWater) noexcept {
    outHasWater = false;
    int topY = startY;
    if (topY < -64) return false;

    // 阶段一: 从 topY 向下找第一个"通道"方块(空气/水/植被等可穿透)
    // 寻找玩家所处高度附近的洞穴通道空腔 (最多向下搜索 64 格)
    int channelY = -99999;
    int airSearchBottom = std::max(-64, topY - 64);
    for (int y = topY; y >= airSearchBottom; y--) {
        try {
            Block const& block = region.getBlock(BlockPos(x, y, z));
            if (IsCavePassableBlock(block)) {
                channelY = y;
                break;
            }
        } catch (...) {
            continue;
        }
    }
    if (channelY == -99999) return false;  // 纯实体岩石列 = 透明(黑色背景)

    // 阶段二: 从通道Y向下找第一个非穿透方块(地板)
    // 深度至少允许向下探测 64 格，确保巨型洞穴与深渊裂谷的地板能够被完整渲染
    int maxFloorDepth = std::max(64, caveDepth);
    int floorSearchBottom = std::max(-64, channelY - maxFloorDepth);
    int depth = 0;
    bool hasWater = false;
    for (int y = channelY; y >= floorSearchBottom; y--, depth++) {
        try {
            Block const& block = region.getBlock(BlockPos(x, y, z));
            if (IsCaveWaterBlock(block)) {
                hasWater = true;
                continue;  // 记录含水并继续向下寻找水下海床/基岩
            }
            if (IsCavePassableBlock(block)) continue;  // 通道内的空气或可穿透方块
            // 命中地板/墙壁方块
            outBlockName = block.getTypeName();
            outBlockY = y;
            outDepth = depth;
            outHasWater = hasWater;
            return true;
        } catch (...) {
            continue;
        }
    }

    if (hasWater) {
        // 纯水体直到底部仍未找到实心方块: 以水方块本身作为表面
        outBlockName = "minecraft:water";
        outBlockY = floorSearchBottom;
        outDepth = depth;
        outHasWater = true;
        return true;
    }

    // 有通道但 maxFloorDepth 内无地板 = 深洞，渲染深灰而非纯黑背景
    outBlockName = "__CAVE_DEEPHOLE__";
    outBlockY = std::max(channelY - maxFloorDepth, -64);
    outDepth = maxFloorDepth;
    outHasWater = false;
    return true;
}

// [洞穴特殊方块颜色] 液体方块在洞穴中使用饱和颜色 (不受深度衰减)
// 对应 Xaero's MapPixel: 液体通过 fluidToBlock 转换, 颜色保持饱和
inline mce::Color GetCaveLiquidColor(std::string const& name) noexcept {
    if (name.find("lava") != std::string::npos) return mce::Color(1.0f, 0.40f, 0.05f, 1.0f);
    if (name.find("water") != std::string::npos) return mce::Color(0.20f, 0.52f, 0.88f, 1.0f);
    return mce::Color(0, 0, 0, 0);  // 非液体
}

// [洞穴方块专用颜色] 比通用 getBlockColor 更精细, 识别洞穴常见方块并赋予准确颜色
// 使洞穴地图色彩丰富: 矿石有对应颜色, 石头变种有区分, 深板岩为冷蓝灰
inline mce::Color GetCaveBlockColor(std::string const& rawName) noexcept {
    std::string name = rawName;
    for (char& c : name) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');

    // [矿石] 必须在 deepslate 检查之前，确保 deepslate_diamond_ore / deepslate_gold_ore 等深层矿石不会被误判为深板岩石材
    if (name.find("diamond") != std::string::npos) return mce::Color(0.40f, 0.85f, 0.85f, 1.0f);
    if (name.find("gold") != std::string::npos) return mce::Color(0.95f, 0.82f, 0.25f, 1.0f);
    if (name.find("iron") != std::string::npos) return mce::Color(0.78f, 0.58f, 0.42f, 1.0f);
    if (name.find("coal") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.78f, 0.15f, 0.15f, 1.0f);
    if (name.find("lapis") != std::string::npos) return mce::Color(0.20f, 0.42f, 0.88f, 1.0f);
    if (name.find("emerald") != std::string::npos) return mce::Color(0.20f, 0.80f, 0.38f, 1.0f);
    if (name.find("debris") != std::string::npos) return mce::Color(0.48f, 0.36f, 0.30f, 1.0f);
    if (name.find("netherite") != std::string::npos) return mce::Color(0.22f, 0.20f, 0.22f, 1.0f);
    if (name.find("quartz") != std::string::npos) return mce::Color(0.88f, 0.85f, 0.78f, 1.0f);

    // [铜与氧化阶段] 必须涵盖所有变体
    if (name.find("copper") != std::string::npos) {
        if (name.find("oxidized") != std::string::npos) return mce::Color(0.30f, 0.55f, 0.50f, 1.0f);
        if (name.find("weathered") != std::string::npos) return mce::Color(0.35f, 0.50f, 0.40f, 1.0f);
        if (name.find("exposed") != std::string::npos) return mce::Color(0.55f, 0.45f, 0.35f, 1.0f);
        return mce::Color(0.78f, 0.52f, 0.35f, 1.0f);
    }

    // [基岩与深板岩]
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.14f, 1.0f);
    if (name.find("reinforced_deepslate") != std::string::npos) return mce::Color(0.28f, 0.28f, 0.30f, 1.0f);
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.28f, 0.28f, 0.31f, 1.0f);

    // [试炼大厅方块 (1.21)]
    if (name.find("trial_spawner") != std::string::npos) return mce::Color(0.63f, 0.39f, 0.24f, 1.0f);
    if (name.find("crafter") != std::string::npos) return mce::Color(0.42f, 0.40f, 0.40f, 1.0f);
    if (name.find("vault") != std::string::npos) return mce::Color(0.38f, 0.36f, 0.38f, 1.0f);
    if (name.find("heavy_core") != std::string::npos) return mce::Color(0.24f, 0.25f, 0.29f, 1.0f);

    // 石头变种 (暖→冷渐变区分)
    if (name.find("granite") != std::string::npos) return mce::Color(0.58f, 0.42f, 0.35f, 1.0f);
    if (name.find("diorite") != std::string::npos) return mce::Color(0.72f, 0.70f, 0.68f, 1.0f);
    if (name.find("andesite") != std::string::npos) return mce::Color(0.50f, 0.48f, 0.46f, 1.0f);
    if (name.find("tuff") != std::string::npos) return mce::Color(0.45f, 0.43f, 0.41f, 1.0f);
    if (name.find("calcite") != std::string::npos) return mce::Color(0.82f, 0.80f, 0.78f, 1.0f);
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.62f, 0.42f, 0.82f, 1.0f);
    // [化石与骨块]
    if (name.find("bone_block") != std::string::npos) return mce::Color(0.88f, 0.86f, 0.78f, 1.0f);
    // [嗅探兽蛋]
    if (name.find("sniffer_egg") != std::string::npos) return mce::Color(0.78f, 0.38f, 0.22f, 1.0f);
    // [末地石与末石砖]
    if (name.find("end_stone") != std::string::npos || name.find("end_brick") != std::string::npos) return mce::Color(0.539f, 0.633f, 0.422f, 1.0f);
    // [紫珀块系列]
    if (name.find("purpur") != std::string::npos) return mce::Color(0.60f, 0.42f, 0.68f, 1.0f);

    // 泥土/沙石 (soul_sand/soul_soil 必须在 sand 之前, 因 "soul_sand" 含子串 "sand")
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.330f, 0.250f, 0.200f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.265f, 0.200f, 0.160f, 1.0f);
    if (name.find("dirt") != std::string::npos) return mce::Color(0.53f, 0.38f, 0.26f, 1.0f);
    // [红沙与红砂岩] 必须在通用 sand 之前
    if (name.find("red_sandstone") != std::string::npos) return mce::Color(0.711f, 0.384f, 0.123f, 1.0f);
    if (name.find("red_sand") != std::string::npos) return mce::Color(0.748f, 0.404f, 0.130f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.890f, 0.840f, 0.680f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.880f, 0.830f, 0.660f, 1.0f);
    if (name.find("gravel") != std::string::npos) return mce::Color(0.52f, 0.47f, 0.44f, 1.0f);
    if (name.find("clay") != std::string::npos && name.find("hardened_clay") == std::string::npos) return mce::Color(0.48f, 0.51f, 0.55f, 1.0f);

    // [陶瓦与硬化粘土] (恶地地下层及洞穴出露陶瓦层)
    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white") != std::string::npos) return mce::Color(0.822f, 0.698f, 0.633f, 1.0f);
        if (name.find("orange") != std::string::npos) return mce::Color(0.634f, 0.329f, 0.148f, 1.0f);
        if (name.find("magenta") != std::string::npos) return mce::Color(0.587f, 0.345f, 0.426f, 1.0f);
        if (name.find("light_blue") != std::string::npos) return mce::Color(0.445f, 0.426f, 0.541f, 1.0f);
        if (name.find("yellow") != std::string::npos) return mce::Color(0.730f, 0.522f, 0.139f, 1.0f);
        if (name.find("lime") != std::string::npos) return mce::Color(0.406f, 0.461f, 0.207f, 1.0f);
        if (name.find("pink") != std::string::npos) return mce::Color(0.635f, 0.307f, 0.309f, 1.0f);
        if (name.find("light_gray") != std::string::npos || name.find("silver") != std::string::npos) return mce::Color(0.530f, 0.420f, 0.382f, 1.0f);
        if (name.find("gray") != std::string::npos) return mce::Color(0.227f, 0.166f, 0.139f, 1.0f);
        if (name.find("cyan") != std::string::npos) return mce::Color(0.340f, 0.357f, 0.357f, 1.0f);
        if (name.find("purple") != std::string::npos) return mce::Color(0.464f, 0.276f, 0.338f, 1.0f);
        if (name.find("blue") != std::string::npos) return mce::Color(0.291f, 0.234f, 0.357f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.303f, 0.201f, 0.140f, 1.0f);
        if (name.find("green") != std::string::npos) return mce::Color(0.298f, 0.327f, 0.166f, 1.0f);
        if (name.find("red") != std::string::npos) return mce::Color(0.561f, 0.239f, 0.184f, 1.0f);
        if (name.find("black") != std::string::npos) return mce::Color(0.146f, 0.090f, 0.064f, 1.0f);
        return mce::Color(0.597f, 0.369f, 0.266f, 1.0f);
    }

    // [植被与繁茂洞穴]
    if (name.find("pale_moss") != std::string::npos || name.find("pale_hanging_moss") != std::string::npos) return mce::Color(0.42f, 0.44f, 0.41f, 1.0f);
    if (name.find("moss") != std::string::npos) return mce::Color(0.35f, 0.43f, 0.18f, 1.0f);
    if (name.find("flowering_azalea") != std::string::npos) return mce::Color(0.48f, 0.46f, 0.32f, 1.0f);
    if (name.find("azalea") != std::string::npos) return mce::Color(0.40f, 0.49f, 0.19f, 1.0f);
    if (name.find("dripleaf") != std::string::npos) return mce::Color(0.44f, 0.56f, 0.20f, 1.0f);
    if (name.find("cave_vines") != std::string::npos || (name.find("glow") != std::string::npos && name.find("berry") != std::string::npos)) {
        if (name.find("berries") != std::string::npos || name.find("berry") != std::string::npos) {
            return mce::Color(0.78f, 0.55f, 0.18f, 1.0f);
        }
        return mce::Color(0.35f, 0.40f, 0.15f, 1.0f);
    }
    if (name.find("spore") != std::string::npos) return mce::Color(0.81f, 0.38f, 0.62f, 1.0f);
    if (name.find("glow_lichen") != std::string::npos || name.find("lichen") != std::string::npos) return mce::Color(0.44f, 0.51f, 0.48f, 1.0f);
    if (name.find("dirt_with_roots") != std::string::npos || (name.find("rooted") != std::string::npos && name.find("dirt") != std::string::npos)) {
        return mce::Color(0.56f, 0.41f, 0.30f, 1.0f);
    }
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos) return mce::Color(0.421f, 0.309f, 0.159f, 1.0f);
    if (name.find("tall_dry_grass") != std::string::npos) return mce::Color(0.771f, 0.674f, 0.482f, 1.0f);
    if (name.find("dry_grass") != std::string::npos) return mce::Color(0.733f, 0.622f, 0.424f, 1.0f);
    if (name.find("cactus_flower") != std::string::npos) return mce::Color(0.822f, 0.473f, 0.531f, 1.0f);
    if (name.find("firefly_bush") != std::string::npos || name.find("firefly") != std::string::npos) return mce::Color(0.58f, 0.50f, 0.22f, 1.0f);
    if (name.find("leaf_litter") != std::string::npos || name.find("litter") != std::string::npos || name.find("fallen_leaf") != std::string::npos || name.find("fallen_leaves") != std::string::npos) return mce::Color(0.50f, 0.35f, 0.24f, 1.0f);

    // [硫黄洞穴系列方块 (1.26 硫黄与辰砂)]
    if (name.find("cinnabar") != std::string::npos) return mce::Color(0.68f, 0.22f, 0.20f, 1.0f);
    if (name.find("sulfur") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.22f, 1.0f);

    // [幽匿系列 (深暗之域)] 标志性深暗青翠/荧光青蓝，彻底消除黑色空洞误解
    if (name.find("sculk_catalyst") != std::string::npos) return mce::Color(0.20f, 0.32f, 0.35f, 1.0f);
    if (name.find("sculk_shrieker") != std::string::npos) return mce::Color(0.48f, 0.45f, 0.38f, 1.0f);
    if (name.find("calibrated_sculk_sensor") != std::string::npos) return mce::Color(0.35f, 0.30f, 0.50f, 1.0f);
    if (name.find("sculk_sensor") != std::string::npos) return mce::Color(0.07f, 0.38f, 0.42f, 1.0f);
    if (name.find("sculk_vein") != std::string::npos) return mce::Color(0.08f, 0.32f, 0.36f, 1.0f);
    if (name.find("sculk") != std::string::npos) return mce::Color(0.07f, 0.28f, 0.32f, 1.0f);

    // [冰系列]
    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.70f, 0.92f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.44f, 0.57f, 0.80f, 1.0f);

    // 普通石头
    if (name.find("stone") != std::string::npos) return mce::Color(0.42f, 0.42f, 0.44f, 1.0f);

    // 下界方块 (Nether blocks — 用于下界地图渲染)
    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("nether_wart") != std::string::npos) return mce::Color(0.60f, 0.14f, 0.14f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("glowstone") != std::string::npos) return mce::Color(1.0f, 0.85f, 0.30f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("warped_wart") != std::string::npos) return mce::Color(0.20f, 0.50f, 0.42f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.25f, 0.22f, 0.22f, 1.0f);
    if (name.find("gilded") != std::string::npos) return mce::Color(0.45f, 0.38f, 0.22f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.20f, 0.18f, 0.20f, 1.0f);
    if (name.find("crying_obsidian") != std::string::npos) return mce::Color(0.25f, 0.12f, 0.35f, 1.0f);
    if (name.find("respawn_anchor") != std::string::npos) return mce::Color(0.32f, 0.18f, 0.38f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.12f, 0.08f, 0.18f, 1.0f);

    // 废弃矿井/要塞/遗迹结构
    if (name.find("spawner") != std::string::npos) return mce::Color(0.22f, 0.26f, 0.30f, 1.0f);
    if (name.find("rail") != std::string::npos) return mce::Color(0.55f, 0.48f, 0.38f, 1.0f);
    if (name.find("web") != std::string::npos) return mce::Color(0.85f, 0.85f, 0.85f, 0.7f);
    if (name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("chain") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("planks") != std::string::npos || name.find("fence") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);

    // 默认: 中性灰
    return mce::Color(0.35f, 0.35f, 0.37f, 1.0f);
}

// ==========================================
// [安全传送增强·核心函数群] 解决"传送到地下/水中"问题
// 设计目标：99.9%+ 传送准确落在地表表面，无地下/水中传送
// 五道防线：①区块就绪检查 ②水面/液体检测 ③落脚点验证 ④稳定性确认 ⑤附近点回退
// ==========================================

// [防线①] 区块就绪检查：通过 hasChunksAt 验证目标区块已完全加载
// ignoreClientChunk = true: 忽略客户端空区块占位，仅当真实数据存在时返回 true
inline bool IsChunkReady(BlockSource& region, int x, int y, int z) noexcept {
    try {
        return region.hasChunksAt(BlockPos(x, y, z), 0, true);
    } catch (...) {
        return false;
    }
}

// [辅助] 判断方块名是否为岩浆（熔岩流体或岩浆方块）
inline bool IsLavaBlockName(std::string const& name) noexcept {
    return name.find("lava") != std::string::npos;
}

// [安全落脚点判定] 判断方块是否可作为脚下的有效支撑
// 规则1 (主世界/下界): 除了岩浆，其它任何方块均可作为安全落脚点（包括水、固体、植物等）
// 规则2 (末地): 除了虚空(空气)，任何位置/方块均作为安全落脚点
inline bool IsValidGroundName(std::string const& name, int dimId = 0) noexcept {
    if (name.empty()) return false;
    if (IsAirLikeName(name)) return false;
    if (dimId == 2) {
        return true; // 末地：非空气/非虚空即可站立
    }
    if (IsLavaBlockName(name)) return false;
    if (name.find("fire") != std::string::npos) return false;
    if (name.find("campfire") != std::string::npos) return false;
    if (name.find("magma") != std::string::npos) return false;
    if (name.find("powder_snow") != std::string::npos) return false;
    if (name.find("cactus") != std::string::npos) return false;
    return true; // 主世界与下界：非危险方块即可站立
}

// [安全站立空间判定] 判断玩家脚部/头部所处空间是否通畅且安全（非窒息、非岩浆）
// 严禁将草方块(grass_block)、巨型蘑菇方块(mushroom_block)等实体方块判定为可站立空间
inline bool IsBreathableSpaceName(std::string const& name, int dimId = 0) noexcept {
    if (name.empty()) return false;
    // 空间内绝不能是岩浆、火焰、营火或有害植物
    if (IsLavaBlockName(name)) return false;
    if (name.find("fire") != std::string::npos) return false;
    if (name.find("campfire") != std::string::npos) return false;
    if (name.find("wither_rose") != std::string::npos) return false;
    if (name.find("sweet_berry") != std::string::npos) return false;
    if (name.find("powder_snow") != std::string::npos) return false;
    if (name.find("cactus") != std::string::npos) return false;
    // 空气类方块
    if (IsAirLikeName(name)) return true;
    // 水体（允许在水中/水面站立）
    if (name.find("water") != std::string::npos) return true;
    // 各种无窒息碰撞/可穿透的覆盖物（植物、火把、告示牌、红石、梯子等）
    if (name.find("torch") != std::string::npos) return true;
    if (name.find("short_grass") != std::string::npos || 
        name.find("tallgrass") != std::string::npos || 
        name.find("tall_grass") != std::string::npos ||
        (name.find("grass") != std::string::npos && name.find("grass_block") == std::string::npos && name.find("path") == std::string::npos)) {
        return true;
    }
    if (name.find("fern") != std::string::npos) return true;
    if (name.find("flower") != std::string::npos && name.find("chorus_flower") == std::string::npos) return true;
    if (name.find("tulip") != std::string::npos || name.find("rose") != std::string::npos || name.find("dandelion") != std::string::npos ||
        name.find("orchid") != std::string::npos || name.find("allium") != std::string::npos || name.find("bluet") != std::string::npos ||
        name.find("poppy") != std::string::npos || name.find("daisy") != std::string::npos || name.find("cornflower") != std::string::npos ||
        name.find("sunflower") != std::string::npos || name.find("lilac") != std::string::npos || name.find("peony") != std::string::npos) {
        return true;
    }
    if (name.find("deadbush") != std::string::npos || name.find("dead_bush") != std::string::npos || name.find("firefly") != std::string::npos || name.find("litter") != std::string::npos) return true;
    if (name.find("vine") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("mushroom") != std::string::npos && name.find("mushroom_block") == std::string::npos && name.find("mushroom_stem") == std::string::npos) return true;
    if (name.find("fungus") != std::string::npos) return true;
    if (name.find("seagrass") != std::string::npos) return true;
    if (name.find("kelp") != std::string::npos) return true;
    if (name.find("rail") != std::string::npos) return true;
    if (name.find("ladder") != std::string::npos) return true;
    if (name.find("sign") != std::string::npos) return true;
    if (name.find("banner") != std::string::npos) return true;
    if (name.find("button") != std::string::npos) return true;
    if (name.find("lever") != std::string::npos) return true;
    if (name.find("pressure_plate") != std::string::npos) return true;
    if (name.find("tripwire") != std::string::npos) return true;
    if (name.find("carpet") != std::string::npos) return true;
    if (name.find("web") != std::string::npos) return true;
    if (name.find("lily") != std::string::npos) return true;
    if (name.find("crop") != std::string::npos || name.find("wheat") != std::string::npos || name.find("carrots") != std::string::npos ||
        name.find("potatoes") != std::string::npos || name.find("beetroot") != std::string::npos) return true;
    if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) return true;
    if (name.find("sugar_cane") != std::string::npos || name.find("reeds") != std::string::npos) return true;
    if (name.find("spore_blossom") != std::string::npos || 
        (name.find("roots") != std::string::npos && name.find("mangrove") == std::string::npos) || 
        name.find("sprouts") != std::string::npos) return true;
    return false;
}

// [最终防线] 检查目标点是否 100% 安全（脚部与头部两格通畅无窒息，下方为合法支撑）
inline bool IsTeleportSpotSafe(BlockSource& region, int blockX, int blockY, int blockZ, int dimId = 0) noexcept {
    try {
        std::string feetName, headName, groundName;
        if (!SafeGetBlockName(region, blockX, blockY, blockZ, feetName) || feetName.empty()) return false;
        if (!SafeGetBlockName(region, blockX, blockY + 1, blockZ, headName) || headName.empty()) return false;
        if (!SafeGetBlockName(region, blockX, blockY - 1, blockZ, groundName) || groundName.empty()) return false;

        if (!IsBreathableSpaceName(feetName, dimId)) return false;
        if (!IsBreathableSpaceName(headName, dimId)) return false;
        if (!IsValidGroundName(groundName, dimId)) return false;

        // [水域安全强化] 传送到水域时必须落在水面，头部不能浸没在水中（水底），确保呼吸与安全
        if (dimId == 0) {
            if (feetName.find("water") != std::string::npos || groundName.find("water") != std::string::npos) {
                if (headName.find("water") != std::string::npos) {
                    return false; // 头部在水里 = 水底浸没，拒绝！
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

// [邻域危险检测] 主世界/下界检测周围是否有岩浆；末地无危险方块检测
inline bool HasAdjacentHazard(BlockSource& region, int x, int y, int z, int radius = 1, int dimId = 0) noexcept {
    if (dimId == 2) return false;
    try {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue;
                std::string name;
                if (SafeGetBlockName(region, x + dx, y, z + dz, name)) {
                    if (IsLavaBlockName(name)) return true;
                }
            }
        }
        return false;
    } catch (...) {
        return true;
    }
}

// [地表露天验证] 验证目标落脚点之上无实心岩石/深板岩/地底天花板阻挡
// 杜绝两阶段探测传送到地下洞穴天花板下方或未加载完整的下层岩石中窒息
inline bool HasOpenSkyAbove(BlockSource& region, int x, short standY, int z, int dimId = 0) noexcept {
    if (dimId == 1) return true; // 下界为封闭洞穴结构，不适用露天检查
    try {
        // 主世界全量向上巡检至建筑高度上限 319；末地巡检至 256
        short scanLimit = (dimId == 2) ? 256 : 319;
        for (short y = standY + 2; y <= scanLimit; ++y) {
            std::string name;
            if (!SafeGetBlockName(region, x, y, z, name)) {
                // 上方有 subchunk 尚未加载到达客户端 (AV/空指针) → 视为未就绪
                return false;
            }
            if (name.empty()) return false;
            if (IsAirLikeName(name)) continue;
            // 允许树木枝叶、树冠、雪层、藤蔓等自然地表覆盖物
            if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) continue;
            if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos) continue;
            if (name.find("vine") != std::string::npos || name.find("lichen") != std::string::npos) continue;
            if (name.find("snow") != std::string::npos && name.find("snow_block") == std::string::npos) continue;
            if (IsBreathableSpaceName(name, dimId)) continue;

            // 遇到任何其他非透气实体方块（岩石/深板岩/泥土/矿石/砂岩等天花板）→ 绝对不是露天！
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

// [下界高品质安全落脚点评分查找] 在单个列内寻找最佳下界落脚点
// 规则：
// 1. 严格限制脚部Y在 [33, 100] 区间（避开Y<=31岩浆海与Y>=105天花板基岩缝隙）
// 2. 支撑方块为非岩浆、非空气、非火焰、非岩浆块的实心支撑
// 3. 脚部(y)、头部(y+1)以及头部上方(y+2)全部通畅，至少保证 3 格垂直净空，杜绝起跳卡头窒息
// 4. 水平四周检查：脚部与头部四周至少有 2 个方向通畅，杜绝 1x1 嵌岩缝隙导致窒息
// 5. 按开阔度与净空评分，优先选择最开阔、最安全的洞穴平坦地面
inline short FindBestNetherSpawnInColumn(BlockSource& region, int x, int z, int preferredY, int& outScore) noexcept {
    outScore = -1;
    short bestY = -32000;

    constexpr int kNetherMinY = 33;  // 脚部Y=33, 支撑地面Y=32 (高于岩浆海31)
    constexpr int kNetherMaxY = 100; // 脚部Y<=100 (避开天花板裂隙)

    for (int y = kNetherMaxY; y >= kNetherMinY; --y) {
        // 1. 支撑方块 (y - 1) 校验
        std::string groundName;
        if (!SafeGetBlockName(region, x, y - 1, z, groundName) || groundName.empty()) continue;
        if (!IsValidGroundName(groundName, 1)) continue;
        if (IsBreathableSpaceName(groundName, 1)) continue; // 支撑方块不能是空气/植物/透光虚体
        if (groundName.find("magma") != std::string::npos) continue;

        // 2. 脚部 (y) 与 头部 (y + 1) 校验
        std::string feetName, headName;
        if (!SafeGetBlockName(region, x, y, z, feetName) || !IsBreathableSpaceName(feetName, 1)) continue;
        if (!SafeGetBlockName(region, x, y + 1, z, headName) || !IsBreathableSpaceName(headName, 1)) continue;

        // 3. 头部上方空间 (y + 2) 校验：至少 3 格连续垂直净空
        std::string aboveName;
        if (!SafeGetBlockName(region, x, y + 2, z, aboveName) || !IsBreathableSpaceName(aboveName, 1)) continue;

        // 4. 水平四周开阔度校验 (杜绝 1x1 夹缝穿模窒息)
        static const struct { int dx, dz; } kAdj[] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        int openSides = 0;
        for (const auto& adj : kAdj) {
            std::string adjFeet, adjHead;
            if (SafeGetBlockName(region, x + adj.dx, y, z + adj.dz, adjFeet) && IsBreathableSpaceName(adjFeet, 1) &&
                SafeGetBlockName(region, x + adj.dx, y + 1, z + adj.dz, adjHead) && IsBreathableSpaceName(adjHead, 1)) {
                openSides++;
            }
        }
        if (openSides < 2) continue;

        // 5. 测量实际净空高度
        int headroom = 3;
        for (int h = y + 3; h <= std::min(y + 16, 120); ++h) {
            std::string hName;
            if (SafeGetBlockName(region, x, h, z, hName) && IsBreathableSpaceName(hName, 1)) {
                headroom++;
            } else {
                break;
            }
        }

        // 6. 综合评分
        int score = headroom * 12 + openSides * 15;
        int targetRef = (preferredY >= 33 && preferredY <= 100) ? preferredY : 64;
        score -= std::abs(y - targetRef) * 2;

        if (score > outScore) {
            outScore = score;
            bestY = (short)y;
        }
    }

    return bestY;
}

// [下界附近最佳安全点搜索] 螺旋搜索附近列，寻找最开阔安全的洞穴地面
inline bool FindBestSafeSpawnNether(BlockSource& region, int& x, int& z, short& outY, int preferredY = 64, int maxRadius = 16) noexcept {
    int bestScore = -1;
    short bestY = -32000;
    int bestX = x, bestZ = z;

    int selfScore = -1;
    short selfY = FindBestNetherSpawnInColumn(region, x, z, preferredY, selfScore);
    if (selfY > -64 && selfScore >= 45) {
        outY = selfY;
        return true;
    }
    if (selfY > -64) {
        bestScore = selfScore;
        bestY = selfY;
    }

    for (int r = 1; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                int colScore = -1;
                short colY = FindBestNetherSpawnInColumn(region, testX, testZ, preferredY, colScore);
                if (colY > -64) {
                    colScore -= r * 3;  // 距离惩罚
                    if (colScore > bestScore) {
                        bestScore = colScore;
                        bestY = colY;
                        bestX = testX;
                        bestZ = testZ;
                    }
                }
            }
        }
        if (bestScore >= 60) {
            x = bestX;
            z = bestZ;
            outY = bestY;
            return true;
        }
    }

    if (bestY > -64) {
        x = bestX;
        z = bestZ;
        outY = bestY;
        return true;
    }
    return false;
}

// [防线②③·地表安全落脚点查找] 给定 (x,z)，返回玩家可安全站立的实际 Y 坐标
// [洞穴/下界传送·安全落脚点查找] 给定 (x,z) 和参考 Y, 在 [minY, maxY] 范围内 refY 附近搜索可安全站立的 Y
// 用于洞穴/下界传送: 从参考高度向上下扫描, 找到通畅且下方为有效落脚点的位置
// 主世界/下界：除岩浆外任何方块均可落脚；末地：除虚空外任何位置均可落脚
inline short SafeFindSafeSpawnYNearY(BlockSource& region, int x, int z, int refY, int minY = -64, int maxY = 319, int dimId = 0) noexcept {
    if (refY < minY) refY = minY;
    if (refY > maxY) refY = maxY;
    try {
        if (dimId == 1) {
            int score = -1;
            int prefY = (refY >= 33 && refY <= 100) ? refY : 64;
            short bestY = FindBestNetherSpawnInColumn(region, x, z, prefY, score);
            if (bestY > -64) return bestY;
            return -32000;
        }

        // 常规/已保存坐标区域: 优先在 refY 附近查找
        // 先从 refY 向上扫描 (优先传送到同高度或略高), 上限 maxY
        for (int y = refY; y <= maxY; ++y) {
            std::string feetName, headName, belowName;
            if (!SafeGetBlockName(region, x, y, z, feetName) || feetName.empty()) continue;
            if (IsBreathableSpaceName(feetName, dimId)) {
                if (y + 1 > maxY) break;
                if (!SafeGetBlockName(region, x, y + 1, z, headName) || headName.empty()) continue;
                if (IsBreathableSpaceName(headName, dimId)) {
                    if (y - 1 < minY) continue;
                    if (!SafeGetBlockName(region, x, y - 1, z, belowName) || belowName.empty()) continue;
                    if (IsValidGroundName(belowName, dimId)) {
                        // [水域安全强化] 若下方或脚部是水，头部必须不能浸在水中（水面判定）
                        if (dimId == 0 && (feetName.find("water") != std::string::npos || belowName.find("water") != std::string::npos)) {
                            if (headName.find("water") != std::string::npos) continue;
                        }
                        return (short)y;
                    }
                }
            }
        }
        // 再从 refY-1 向下扫描, 下限 minY
        for (int y = refY - 1; y >= minY; --y) {
            std::string feetName, headName, belowName;
            if (!SafeGetBlockName(region, x, y, z, feetName) || feetName.empty()) continue;
            if (IsBreathableSpaceName(feetName, dimId)) {
                if (y + 1 > maxY) continue;
                if (!SafeGetBlockName(region, x, y + 1, z, headName) || headName.empty()) continue;
                if (IsBreathableSpaceName(headName, dimId)) {
                    if (y - 1 < minY) continue;
                    if (!SafeGetBlockName(region, x, y - 1, z, belowName) || belowName.empty()) continue;
                    if (IsValidGroundName(belowName, dimId)) {
                        // [水域安全强化] 若下方或脚部是水，头部必须不能浸在水中（水面判定）
                        if (dimId == 0 && (feetName.find("water") != std::string::npos || belowName.find("water") != std::string::npos)) {
                            if (headName.find("water") != std::string::npos) continue;
                        }
                        return (short)y;
                    }
                }
            }
        }
        return -32000;
    } catch (...) {
        return -32000;
    }
}

// [洞穴/下界传送·附近安全点搜索] 螺旋搜索附近列, 在 refY 附近 [minY,maxY] 范围找安全落脚点
inline bool FindNearestSafeSpawnNearY(BlockSource& region, int& x, int& z, short& outY, int refY, int maxRadius = 16, int minY = -64, int maxY = 319, int dimId = 0) noexcept {
    if (dimId == 1) {
        int prefY = (refY >= 33 && refY <= 100) ? refY : 64;
        return FindBestSafeSpawnNether(region, x, z, outY, prefY, maxRadius);
    }
    // 第一轮: 严格 (邻居无岩浆)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1, dimId)) {
                        x = testX;
                        z = testZ;
                        outY = testY;
                        return true;
                    }
                }
            }
        }
    }
    // 第二轮: 宽松 (仅本列安全)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    // 第三轮: 超大范围宽松 (±17~±32)
    for (int r = maxRadius + 1; r <= 32; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    return false;
}

// [防线②③·地表安全落脚点查找] 给定 (x,z)，返回玩家可安全站立的实际 Y 坐标
// 规则：
// 1. 主世界与下界：除岩浆外任何方块均为安全落脚点（包括水体、草地、冰面等）
// 2. 末地：除虚空外任何位置均为安全落脚点
// 3. 自顶向下扫描：从天空 Y=319 (主世界/末地) 或 Y=128 (下界) 向下查找第一个真实地表/洞底支撑方块，绝不卡入地底或石头内部
// 返回值：安全 Y（玩家脚部位置），或 -32000 表示该列无安全落脚点
inline short SafeFindSafeSpawnY(BlockSource& region, int x, int z, int dimId = 0) noexcept {
    try {
        if (dimId == 1) {
            // 下界传送:
            // 1. 若大地图已保存具体坐标点的高度 (已探索区域), 优先在保存高度附近查找
            int16_t cachedNetherY = MapCacheManager::GetCachedSurfaceHeight(x, z, true);
            if (cachedNetherY != MapCacheManager::HEIGHT_UNKNOWN && cachedNetherY > 0) {
                int outScore = -1;
                short bestY = FindBestNetherSpawnInColumn(region, x, z, (int)cachedNetherY + 1, outScore);
                if (bestY > -64) return bestY;
                return SafeFindSafeSpawnYNearY(region, x, z, cachedNetherY + 1, 2, 125, 1);
            }
            // 2. 未保存具体坐标点的位置或未去过的区域: 寻找纵向最佳开阔下界洞穴地面 (避开天花板夹层)
            int outScore = -1;
            short bestY = FindBestNetherSpawnInColumn(region, x, z, 64, outScore);
            if (bestY > -64) return bestY;
            return -32000;
        }

        // 主世界 (dimId == 0) 与 末地 (dimId == 2):
        // 从最高空 (319) 向下扫描真实地表
        int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(x, z);

        // [主世界底部基岩与地表就绪校验] 主世界 Y=-64 必须为 bedrock 且 SafeGetSurfaceY 就绪
        if (dimId == 0) {
            std::string bottomBlock;
            if (!SafeGetBlockName(region, x, -64, z, bottomBlock) || bottomBlock.find("bedrock") == std::string::npos) {
                return -32000;
            }
            short surfY = SafeGetSurfaceY(region, x, z);
            if (surfY <= -64 || surfY >= 320 || surfY == -32000) {
                return -32000;
            }
        }

        for (short y = 319; y >= -60; --y) {
            std::string blockName;
            if (!SafeGetBlockName(region, x, y, z, blockName)) {
                // 上层 subchunk 尚未加载到达 (AV/异常) → 严禁当作空气继续向下扫描，直接返回 -32000 等待！
                return -32000;
            }
            if (blockName.empty()) return -32000;

            // 跳过空气，寻找从天空向下的第一个实体支撑方块 (y 为支撑方块，y+1 为脚部)
            if (IsAirLikeName(blockName)) {
                continue;
            }

            // 找到了最高实体方块 y
            if (IsValidGroundName(blockName, dimId)) {
                short standY = y + 1;
                if (standY > 318) return -32000;

                // 与引擎最高实体高度 SafeGetSurfaceY 校验，落差不可超过 8 格 (杜绝洞穴地面)
                if (dimId == 0) {
                    short surfY = SafeGetSurfaceY(region, x, z);
                    if (surfY <= -64 || surfY >= 320 || surfY == -32000 || standY < surfY - 8) {
                        return -32000;
                    }
                }

                std::string feetName, headName, aboveHeadName;
                if (!SafeGetBlockName(region, x, standY, z, feetName) || feetName.empty()) return -32000;
                if (!SafeGetBlockName(region, x, standY + 1, z, headName) || headName.empty()) return -32000;
                if (!SafeGetBlockName(region, x, standY + 2, z, aboveHeadName) || aboveHeadName.empty()) return -32000;

                // 保证脚部、头部及头部上方至少 3 格垂直通畅
                if (IsBreathableSpaceName(feetName, dimId) && 
                    IsBreathableSpaceName(headName, dimId) && 
                    IsBreathableSpaceName(aboveHeadName, dimId)) {
                    
                    // 如果存在有效历史缓存且当前扫描高度比缓存地表低 15 格以上，
                    // 说明上层 subchunk 尚未加载到达，返回 -32000 继续等待，防止提前掉入未加载的地底石头！
                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -60) {
                        if (standY < cachedY - 15) {
                            return -32000;
                        }
                    }

                    // [露天与无实心天花板阻挡校验]
                    if (!HasOpenSkyAbove(region, x, standY, z, dimId)) {
                        return -32000;
                    }

                    return standY;
                }
            } else {
                // 最高方块为岩浆等危险方块 → 该列不安全
                return -32000;
            }
        }

        return -32000;
    } catch (...) {
        return -32000;
    }
}

// [防线⑤·附近安全点搜索] 目标列无安全落脚点时，螺旋搜索附近列
// 主世界/下界：避开岩浆；末地：寻找最近的非虚空空岛
inline bool FindNearestSafeSpawn(BlockSource& region, int& x, int& z, short& outY, int maxRadius = 16, int dimId = 0) noexcept {
    if (dimId == 1) {
        return FindBestSafeSpawnNether(region, x, z, outY, 64, maxRadius);
    }
    // 第一轮：严格搜索（主世界/下界 3x3 邻居无岩浆）
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1, dimId)) {
                        x = testX;
                        z = testZ;
                        outY = testY;
                        return true;
                    }
                }
            }
        }
    }

    // 第二轮：宽松搜索（仅本列安全）
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }

    // 第三轮：超大范围宽松搜索（半径 ±(maxRadius+1)~±32）
    for (int r = maxRadius + 1; r <= 32; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ, dimId);
                if (testY > -64 && testY < 319 && IsTeleportSpotSafe(region, testX, testY, testZ, dimId)) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }
    return false;
}

LL_TYPE_INSTANCE_HOOK(
    ClientInstanceUpdateHook,
    ll::memory::HookPriority::Normal,
    ClientInstance,
    &ClientInstance::$update,
    bool,
    bool a1
) {
    MapRenderState::lastFrameTotalCalls = MapRenderState::frameCallCount.load();
    if (MapRenderState::lastFrameTotalCalls < 1) MapRenderState::lastFrameTotalCalls = 1;
    MapRenderState::frameCallCount.store(0);

    bool result = origin(a1);

    // [关闭期安全] disable() 已启动资源释放流程（D3D/ImGui/缓存均可能已失效）。
    // 此时进入复杂玩家状态/区块/缓存逻辑会访问已释放对象导致 0xC0000005 退出崩溃。
    // 直接放行原函数结果，让 update 链路继续往下走（游戏自身的退出流程）。
    if (MapRenderState::g_isShuttingDown.load()) return result;

    g_clientInstance = this;

    auto* player = this->getLocalPlayer();
    if (player && this->isWorldActive()) {
        Vec3 pos;
        try {
            pos = player->getFeetPos();

            if (g_prevPhysicsPos.x == 0.0f && g_prevPhysicsPos.z == 0.0f) {
                g_prevPhysicsPos = pos;
            } else {
                g_prevPhysicsPos = g_currPhysicsPos;
            }
            g_currPhysicsPos  = pos;
            g_lastPhysicsTime = std::chrono::steady_clock::now();

            g_playerBlockX = (int)std::floor(pos.x);
            g_playerBlockZ = (int)std::floor(pos.z);
            g_playerX = pos.x; g_playerY = pos.y; g_playerZ = pos.z;
            g_playerYaw = player->getRotation().y;
            g_hasPlayer   = true;
            g_localPlayer = player;
        } catch (...) {
            // player 指针可能失效(维度切换/区块卸载/退出过程), 跳过本帧
            return result;
        }

        // ==========================================
        // [地表直达传送·增强版] 三级地表Y识别 + 两阶段探测 + 五道安全防线
        // ==========================================
        if (MapRenderState::triggerTeleport.load()) {
            float targetX = MapRenderState::tpTargetX;
            float targetY = MapRenderState::tpTargetY;
            float targetZ = MapRenderState::tpTargetZ;
            int targetDim = MapRenderState::tpTargetDim;
            int currentDim = MapRenderState::currentDimensionId;
            if (targetDim < 0) {
                targetDim = currentDim;
            }

            // [跨维度传送] 若目标维度与当前维度不同，使用 Bedrock 规范指令 /execute in <dim> run tp @s 执行跨界传送
            if (targetDim != currentDim) {
                std::string dimName;
                if (targetDim == 1) dimName = "nether";
                else if (targetDim == 2) dimName = "the_end";
                else dimName = "overworld";

                float finalY = targetY;
                // 若 Y 为未指定哨兵 (<-500 或 == 320)，赋予目标维度的安全默认高度
                if (finalY < -500.0f || std::abs(finalY - 320.0f) < 0.1f) {
                    if (targetDim == 1) finalY = 64.0f;       // 下界安全中层
                    else if (targetDim == 2) finalY = 65.0f;  // 末地岛屿表面
                    else finalY = 70.0f;                      // 主世界海平面附近
                }

                char coordBuf[160];
                std::snprintf(coordBuf, sizeof(coordBuf), "/execute in %s run tp @s %.2f %.2f %.2f",
                              dimName.c_str(), targetX, finalY, targetZ);
                SendServerCommand(*player, coordBuf);

                LogTeleport("tp cross-dimension from dim=" + std::to_string(currentDim) +
                            " to dim=" + std::to_string(targetDim) + " (" + dimName +
                            ") coords=(" + std::to_string((int)targetX) + "," +
                            std::to_string((int)finalY) + "," + std::to_string((int)targetZ) + ")");

                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                MapRenderState::teleportStatusMsg.clear();
                MapRenderState::tpTargetDim = -1;
                MapRenderState::triggerTeleport.store(false);
            } else {
                MapRenderState::tpTargetDim = -1;
                std::string detectMethod = "user-specified";

                // targetY < -500（哨兵）或 == 320（未加载默认值）→ 系统决定地表
                bool needSurfaceDetect = (targetY < -500.0f) || (std::abs(targetY - 320.0f) < 0.1f);

            if (needSurfaceDetect) {
                int blockX = (int)std::floor(targetX);
                int blockZ = (int)std::floor(targetZ);
                BlockSource* region = this->getRegion();

                // [维度感知] 确定传送模式
                // 0=主世界地表, 1=下界/主世界洞穴, 2=末地开阔空岛
                int teleportMode = 0;
                int dimId = MapRenderState::currentDimensionId;
                if (dimId == 1) {
                    teleportMode = 1;  // 下界: 洞穴模式传送
                } else if (dimId == 2) {
                    teleportMode = 2;  // 末地: 开阔空岛地表传送 (避免虚空)
                } else if (MapRenderState::g_caveModeActive) {
                    teleportMode = 1;  // 主世界洞穴: 洞穴模式传送
                }
                int refY = (int)g_playerY;  // 洞穴传送的参考Y (玩家当前高度)
                if (dimId == 1) {
                    int16_t cachedNetherY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, true);
                    if (cachedNetherY != MapCacheManager::HEIGHT_UNKNOWN && cachedNetherY > 0) {
                        refY = (int)cachedNetherY + 1; // 已保存具体坐标点的位置: 优先在保存高度附近查找
                    } else {
                        refY = (g_playerY >= 33.0f && g_playerY <= 100.0f) ? (int)g_playerY : 64; // 未保存具体坐标点或未去过的区域: 默认参考高度64(或当前玩家有效洞穴高度)
                    }
                } else if (MapRenderState::g_caveModeActive) {
                    int16_t cachedCaveY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, true);
                    if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64) {
                        refY = (int)cachedCaveY + 1;
                    }
                }

                // [Y范围限制] 按维度计算扫描范围
                // 下界 [2,125] 避开基岩层; 洞穴 [refY-48, refY+48] 避免穿过天花板到地表; 末地不限
                int tpMinY = -64, tpMaxY = 319;
                if (dimId == 1) {
                    tpMinY = 2; tpMaxY = 125;  // 下界: 避开底部/顶部基岩层
                } else if (teleportMode == 1) {
                    tpMinY = std::max(-64, refY - 48);  // 洞穴: 限制在玩家Y±48
                    tpMaxY = std::min(319, refY + 48);
                }

                // [防线①] 区块就绪检查：仅当区块已完全加载时才信任实时/缓存 Y
                bool chunkReady = false;
                if (region) {
                    int checkY = (teleportMode == 1) ? ((dimId == 1 && refY >= 120) ? 64 : refY) : 64;
                    chunkReady = IsChunkReady(*region, blockX, checkY, blockZ);
                }

                bool needProbe = false;

                if (teleportMode == 0 || teleportMode == 2) {
                    // === 地表/末地传送: 缓存/实时高度 → SafeFindSafeSpawnY → FindNearestSafeSpawn ===
                    int16_t surfaceY = MapCacheManager::HEIGHT_UNKNOWN;

                    // [优先级1] 缓存高度图：可识别已扫描但已卸载的区域
                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ);
                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64 && cachedY < 319) {
                        surfaceY = cachedY;
                        detectMethod = "cache";
                    }

                    // [优先级2] 实时 BlockSource：仅对已加载区块有效
                    if (surfaceY == MapCacheManager::HEIGHT_UNKNOWN && region && chunkReady) {
                        short liveY = SafeGetSurfaceY(*region, blockX, blockZ);
                        if (liveY > -64 && liveY < 319) {
                            surfaceY = liveY;
                            detectMethod = "live";
                        }
                    }

                    if (surfaceY != MapCacheManager::HEIGHT_UNKNOWN && surfaceY > -64 && surfaceY < 319) {
                        if (region && chunkReady) {
                            // [防线②③] 命中缓存/实时且区块就绪 → 用 SafeFindSafeSpawnY 验证落脚点
                            short safeY = SafeFindSafeSpawnY(*region, blockX, blockZ, dimId);
                            if (safeY > -64 && safeY < 319 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, dimId)) {
                                // 落脚点验证通过 → 直接传送
                                float finalY = (float)safeY;
                                LogTeleport("tp instant (" + std::to_string(blockX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                            ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/safe-spawn");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else {
                                // [防线⑤] 目标列为岩浆/虚空/不安全 → 螺旋搜索周围安全落脚点 (±16~±32)
                                int searchX = blockX, searchZ = blockZ;
                                short nearbyY = -32000;
                                if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16, dimId) &&
                                    IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId)) {
                                    LogTeleport("tp nearby-fallback (" + std::to_string(searchX) + "," +
                                                std::to_string((int)nearbyY) + "," + std::to_string(searchZ) +
                                                ") [original (" + std::to_string(blockX) + "," +
                                                std::to_string(blockZ) + ") lava/void, landed on safe ground, dim=" + std::to_string(dimId) + "]");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)searchX + 0.5f, (float)nearbyY, (float)searchZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else if (surfaceY != MapCacheManager::HEIGHT_UNKNOWN && surfaceY > -64 && surfaceY < 319) {
                                    // [解除误判驳回] 实时落脚点未就绪（虽然 hasChunksAt 返回 true，但远端区块 subchunk 尚未全部解压到达客户端或基岩未下发）
                                    // 绝不可误判为"周围全是岩浆/虚空"而立即驳回！优先使用已探索地图缓存中的真实地表Y高度进行精准传送
                                    float finalY = (float)surfaceY;
                                    if (dimId == 0 && MapCacheManager::IsCachedWater(blockX, blockZ)) {
                                        if (finalY < 63.0f) finalY = 63.0f;
                                    }
                                    LogTeleport("tp instant cached-fallback (" + std::to_string(blockX) + "," +
                                                std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                                ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/direct-cache-fallback");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else {
                                    // 缓存无记录且实时落脚点未就绪 → 转入两阶段探测加载区块
                                    needProbe = true;
                                }
                            }
                        } else {
                            // [解除强行区块就绪校验] 区块未就绪（如不在视野/未加载），但已探索缓存（MapCacheManager）中有地表Y记录 → 直接精准传送到地表
                            float finalY = (float)surfaceY;
                            if (dimId == 0 && MapCacheManager::IsCachedWater(blockX, blockZ)) {
                                if (finalY < 63.0f) finalY = 63.0f;
                            }
                            LogTeleport("tp instant cached (" + std::to_string(blockX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                        ") dim=" + std::to_string(dimId) + " method=" + detectMethod + "/direct-cache");
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                            MapRenderState::teleportStatusMsg.clear();
                        }
                    } else {
                        // 缓存无记录且区块未就绪 → 两阶段探测
                        needProbe = true;
                    }
                } else {
                    // === 洞穴/下界传送: 在参考Y附近搜索安全落脚点 ===
                    bool isCave = (dimId == 1) || MapRenderState::g_caveModeActive;
                    int16_t cachedCaveY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ, isCave);

                    if (dimId == 1) {
                        // [下界传送] 若区块已就绪且已加载
                        if (region && chunkReady) {
                            short safeY = SafeFindSafeSpawnY(*region, blockX, blockZ, 1);
                            if (safeY > -64 && safeY < 319 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, 1)) {
                                LogTeleport("tp nether-instant (" + std::to_string(blockX) + "," +
                                            std::to_string((int)safeY) + "," + std::to_string(blockZ) +
                                            ") [refY=" + std::to_string(refY) + "]");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, (float)safeY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else {
                                // 目标点为岩浆或不安全 → 搜索周围安全落脚点 (±16~±32)
                                int searchX = blockX, searchZ = blockZ;
                                short nearbyY = -32000;
                                if ((FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 16, tpMinY, tpMaxY, 1) ||
                                     FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 32, tpMinY, tpMaxY, 1)) &&
                                    IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, 1)) {
                                    LogTeleport("tp nether-nearby (" + std::to_string(searchX) + "," +
                                                std::to_string((int)nearbyY) + "," + std::to_string(searchZ) +
                                                ") [refY=" + std::to_string(refY) + "]");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)searchX + 0.5f, (float)nearbyY, (float)searchZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > 0 && cachedCaveY < 127) {
                                    // [解除误判驳回] 下界实时落脚点未就绪，但缓存中有下界Y记录 → 回退使用缓存高度直接传送
                                    float finalY = (float)cachedCaveY + 1.0f;
                                    LogTeleport("tp nether cached-fallback (" + std::to_string(blockX) + "," +
                                                std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                                ") dim=1 method=cache/direct-nether-fallback");
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                  (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                } else {
                                    // 缓存无记录或未找到 → 转入两阶段探测
                                    needProbe = true;
                                }
                            }
                        } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > 0 && cachedCaveY < 127) {
                            // [解除强行区块就绪校验] 下界区块未就绪，但缓存中有下界Y高度记录 → 直接精准传送
                            float finalY = (float)cachedCaveY + 1.0f;
                            LogTeleport("tp nether cached (" + std::to_string(blockX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                        ") dim=1 method=cache/direct-nether");
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                            MapRenderState::teleportStatusMsg.clear();
                        } else {
                            needProbe = true;
                        }
                    } else {
                        // [主世界洞穴传送]
                        if (region && chunkReady) {
                            short safeY = SafeFindSafeSpawnYNearY(*region, blockX, blockZ, refY, tpMinY, tpMaxY, 0);
                            if (safeY > -64 && safeY < 319 && IsTeleportSpotSafe(*region, blockX, safeY, blockZ, 0)) {
                                LogTeleport("tp cave-instant (" + std::to_string(blockX) + "," +
                                            std::to_string((int)safeY) + "," + std::to_string(blockZ) +
                                            ") [refY=" + std::to_string(refY) + "]");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, (float)safeY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64 && cachedCaveY < 319) {
                                float finalY = (float)cachedCaveY + 1.0f;
                                LogTeleport("tp cave cached-fallback (" + std::to_string(blockX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                            ") dim=0 method=cache/direct-cave-fallback");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                            } else {
                                needProbe = true;
                            }
                        } else if (cachedCaveY != MapCacheManager::HEIGHT_UNKNOWN && cachedCaveY > -64 && cachedCaveY < 319) {
                            // [解除强行区块就绪校验] 洞穴区块未就绪，但缓存中有洞穴Y高度记录 → 直接精准传送
                            float finalY = (float)cachedCaveY + 1.0f;
                            LogTeleport("tp cave cached (" + std::to_string(blockX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                        ") dim=0 method=cache/direct-cave");
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                            MapRenderState::teleportStatusMsg.clear();
                        } else {
                            needProbe = true;
                        }
                    }
                }

                if (needProbe) {
                    // [两阶段探测] Phase 0 tp 触发区块加载, Phase 1 轮询找安全Y
                    MapRenderState::probeMode = teleportMode;
                    MapRenderState::probeMinY = tpMinY;
                    MapRenderState::probeMaxY = tpMaxY;
                    MapRenderState::probeRefY = refY;
                    MapRenderState::probeIsNether = (dimId == 1);
                    MapRenderState::probeOriginalX = g_playerX;
                    MapRenderState::probeOriginalY = g_playerY;
                    MapRenderState::probeOriginalZ = g_playerZ;
                    MapRenderState::probeTargetX.store(blockX);
                    MapRenderState::probeTargetZ.store(blockZ);
                    MapRenderState::probeStartTime = std::chrono::steady_clock::now();
                    MapRenderState::probeLastY = -32000;
                    MapRenderState::probeLastX = 0;
                    MapRenderState::probeLastZ = 0;
                    MapRenderState::probeStableCount = 0;
                    MapRenderState::pendingSurfaceProbe.store(true);
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Loading);
                    MapRenderState::teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
                    MapRenderState::teleportFailReason.clear();

                    // Phase 0 探测传送：先传送到安全高度触发区块加载
                    // 下界: Y=128 (基岩顶层之上, 玩家安全等待区块加载)
                    // 其他维度: Y=320 (地表/空岛之上)
                    float probeY = (dimId == 1) ? 128.0f : 320.0f;
                    char probeBuf[128];
                    std::snprintf(probeBuf, sizeof(probeBuf), "/tp @s %.2f %.2f %.2f", targetX, probeY, targetZ);
                    SendServerCommand(*player, probeBuf);

                    LogTeleport("probe START (" + std::to_string(blockX) + "," +
                                std::to_string((int)probeY) + "," + std::to_string(blockZ) +
                                ") [mode=" + std::to_string(teleportMode) + " dim=" + std::to_string(dimId) +
                                " waiting chunk load, chunkReady=" + (chunkReady ? "yes" : "no") + "]");
                }
            } else {
                // 用户/路径点显式指定 Y（非 -500/非 320）→ 直接传送，尊重原意
                LogTeleport("tp user-y (" + std::to_string((int)targetX) + "," +
                            std::to_string((int)targetY) + "," + std::to_string((int)targetZ) +
                            ") method=" + detectMethod);
                char coordBuf[128];
                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f", targetX, targetY, targetZ);
                SendServerCommand(*player, coordBuf);
                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                MapRenderState::teleportStatusMsg.clear();
            }
                MapRenderState::triggerTeleport.store(false);
            }
        }

        // ==========================================
        // [两阶段探测·Phase 1 轮询] 等待目标区块加载后传送到地表/落脚点
        // ==========================================
        if (MapRenderState::pendingSurfaceProbe.load()) {
            int probeX = MapRenderState::probeTargetX.load();
            int probeZ = MapRenderState::probeTargetZ.load();
            bool probeDone = false;

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - MapRenderState::probeStartTime).count();

            // [防线①] Phase 0 tp 后 1000ms 宽限期，留出远端区块建立连接与初始包下发时间
            if (elapsedMs >= 1000) {
                BlockSource* region = this->getRegion();
                if (region) {
                    int probeMode = MapRenderState::probeMode;
                    int dimId = MapRenderState::currentDimensionId;
                    int checkY = (probeMode == 1) ? ((dimId == 1 && MapRenderState::probeRefY >= 120) ? 64 : MapRenderState::probeRefY) : 64;
                    if (IsChunkReady(*region, probeX, checkY, probeZ)) {
                        // [主世界基础数据就绪检查] 确认基岩层与地表就绪，杜绝在空指针/未下发区块上误判
                        bool baseDataReady = true;
                        if (dimId == 0) {
                            std::string testBedrock;
                            if (!SafeGetBlockName(*region, probeX, -64, probeZ, testBedrock) || testBedrock.find("bedrock") == std::string::npos) {
                                baseDataReady = false;
                            }
                            if (probeMode == 0) {
                                // 地表传送模式：引擎最高地表高度必须就绪且有效
                                short testSurfY = SafeGetSurfaceY(*region, probeX, probeZ);
                                if (testSurfY <= -64 || testSurfY >= 320 || testSurfY == -32000) {
                                    baseDataReady = false;
                                }
                            }
                        }
                        if (!baseDataReady) {
                            MapRenderState::probeStableCount = 0;
                            MapRenderState::probeLastY = -32000;
                        } else if (dimId == 1) {
                            // ==========================================
                            // [下界·Phase 1] 确认下界区块方块数据加载到达客户端后，寻找最佳开阔安全落脚点
                            // ==========================================
                            bool netherDataLoaded = false;
                            std::string testBedrock;
                            if (SafeGetBlockName(*region, probeX, 127, probeZ, testBedrock) && !testBedrock.empty() && !IsAirLikeName(testBedrock)) {
                                netherDataLoaded = true;
                            } else if (SafeGetBlockName(*region, probeX, 0, probeZ, testBedrock) && !testBedrock.empty() && !IsAirLikeName(testBedrock)) {
                                netherDataLoaded = true;
                            }

                            if (netherDataLoaded) {
                                int searchX = probeX;
                                int searchZ = probeZ;
                                short netherY = -32000;
                                int prefY = (MapRenderState::probeRefY >= 33 && MapRenderState::probeRefY <= 100) ? MapRenderState::probeRefY : 64;

                                // 优先搜索目标列及周围 r=16 内的最佳开阔安全点，若周围全是实心地狱岩或岩浆海则扩大至 r=32
                                bool found = FindBestSafeSpawnNether(*region, searchX, searchZ, netherY, prefY, 16);
                                if (!found) {
                                    found = FindBestSafeSpawnNether(*region, searchX, searchZ, netherY, prefY, 32);
                                }

                                if (found && netherY > -64 && netherY < 125) {
                                    // 连续 3 帧确认安全点坐标与高度稳定
                                    if (netherY == MapRenderState::probeLastY && searchX == MapRenderState::probeLastX && searchZ == MapRenderState::probeLastZ) {
                                        MapRenderState::probeStableCount++;
                                    } else {
                                        MapRenderState::probeLastY = netherY;
                                        MapRenderState::probeLastX = searchX;
                                        MapRenderState::probeLastZ = searchZ;
                                        MapRenderState::probeStableCount = 1;
                                    }

                                    if (MapRenderState::probeStableCount >= MapRenderState::kProbeStableThreshold) {
                                        float finalY = (float)netherY;
                                        char coordBuf[128];
                                        std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                      (float)searchX + 0.5f, finalY, (float)searchZ + 0.5f);
                                        SendServerCommand(*player, coordBuf);
                                        LogTeleport("probe SUCCESS-nether (" + std::to_string(searchX) + "," +
                                                    std::to_string((int)finalY) + "," + std::to_string(searchZ) +
                                                    ") [prefY=" + std::to_string(prefY) + ", safe open cavern ground]");
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                        MapRenderState::teleportStatusMsg.clear();
                                        probeDone = true;
                                    }
                                } else {
                                    MapRenderState::probeStableCount = 0;
                                    MapRenderState::probeLastY = -32000;
                                    // 区块已加载，但半径 32 内全是岩浆海或全实心地狱岩无任何安全落脚点
                                    // 超过 4 秒仍未找到时安全驳回
                                    if (elapsedMs >= 4000) {
                                        LogTeleport("probe REJECT-nether (" + std::to_string(probeX) + "," +
                                                    std::to_string(probeZ) + ") [chunk loaded but no safe spawn within r=32, reject]");
                                        char abortBuf[128];
                                        std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                                      MapRenderState::probeOriginalX, MapRenderState::probeOriginalY, MapRenderState::probeOriginalZ);
                                        SendServerCommand(*player, abortBuf);
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                                        MapRenderState::teleportStatusMsg.clear();
                                        MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                        probeDone = true;
                                    }
                                }
                            } else {
                                MapRenderState::probeStableCount = 0;
                                MapRenderState::probeLastY = -32000;
                            }
                        } else if (probeMode == 1) {
                            // [主世界洞穴·Phase 1] 稳定性检查 + 纵向安全落脚点搜索
                            int refY = MapRenderState::probeRefY;
                            short liveY = SafeFindSafeSpawnYNearY(*region, probeX, probeZ, refY, MapRenderState::probeMinY, MapRenderState::probeMaxY, dimId);
                            int targetX = probeX;
                            int targetZ = probeZ;
                            short targetY = liveY;

                            if (targetY <= -64 || targetY >= 319 || !IsTeleportSpotSafe(*region, targetX, targetY, targetZ, dimId)) {
                                // 目标列为岩石/岩浆/不安全 → 搜索周围安全落脚点
                                short nearbyY = -32000;
                                int searchX = probeX, searchZ = probeZ;
                                if (FindNearestSafeSpawnNearY(*region, searchX, searchZ, nearbyY, refY, 16, MapRenderState::probeMinY, MapRenderState::probeMaxY, dimId) &&
                                    IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId)) {
                                    targetX = searchX;
                                    targetZ = searchZ;
                                    targetY = nearbyY;
                                }
                            }

                            if (targetY > -64 && targetY < 319) {
                                if (targetY == MapRenderState::probeLastY && targetX == MapRenderState::probeLastX && targetZ == MapRenderState::probeLastZ) {
                                    MapRenderState::probeStableCount++;
                                } else {
                                    MapRenderState::probeLastY = targetY;
                                    MapRenderState::probeLastX = targetX;
                                    MapRenderState::probeLastZ = targetZ;
                                    MapRenderState::probeStableCount = 1;
                                }
                            } else {
                                MapRenderState::probeStableCount = 0;
                                MapRenderState::probeLastY = -32000;
                            }

                            if (MapRenderState::probeStableCount >= MapRenderState::kProbeStableThreshold) {
                                float finalY = (float)targetY;
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)targetX + 0.5f, finalY, (float)targetZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                LogTeleport("probe SUCCESS-cave (" + std::to_string(targetX) + "," +
                                            std::to_string((int)finalY) + "," + std::to_string(targetZ) +
                                            ") [refY=" + std::to_string(refY) + ", safe cave ground]");
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                MapRenderState::teleportStatusMsg.clear();
                                probeDone = true;
                            } else if (targetY <= -64 && elapsedMs >= 4000) {
                                // 超过 4 秒且周围全无安全落脚点 → 驳回传送，回退原位
                                LogTeleport("probe REJECT-cave (" + std::to_string(probeX) + "," +
                                            std::to_string(probeZ) + ") [chunk stable but target and surroundings are unsafe, reject]");
                                char abortBuf[128];
                                std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                              MapRenderState::probeOriginalX, MapRenderState::probeOriginalY, MapRenderState::probeOriginalZ);
                                SendServerCommand(*player, abortBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                                MapRenderState::teleportStatusMsg.clear();
                                MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                probeDone = true;
                            }
                        } else {
                            // [主世界地表 / 末地·Phase 1] 稳定性检查：直接通过自顶向下的 SafeFindSafeSpawnY 获取 Y
                            short liveY = SafeFindSafeSpawnY(*region, probeX, probeZ, dimId);

                            if (liveY > -64 && liveY < 319) {
                                if (liveY == MapRenderState::probeLastY) {
                                    MapRenderState::probeStableCount++;
                                } else {
                                    MapRenderState::probeLastY = liveY;
                                    MapRenderState::probeStableCount = 1;
                                }
                            } else {
                                MapRenderState::probeStableCount = 0;
                                MapRenderState::probeLastY = -32000;
                            }

                            if (MapRenderState::probeStableCount >= MapRenderState::kProbeStableThreshold) {
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Validating);

                                if (liveY > -64 && liveY < 319 && 
                                    IsTeleportSpotSafe(*region, probeX, liveY, probeZ, dimId) &&
                                    HasOpenSkyAbove(*region, probeX, liveY, probeZ, dimId)) {
                                    // 目标点本身为安全地表/空岛且完全露天无石顶阻挡 → 直接传送
                                    float finalY = (float)liveY;
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf),
                                                  "/tp @s %.2f %.2f %.2f", (float)probeX + 0.5f, finalY, (float)probeZ + 0.5f);
                                    SendServerCommand(*player, coordBuf);
                                    LogTeleport("probe SUCCESS (" + std::to_string(probeX) + "," +
                                                std::to_string((int)finalY) + "," + std::to_string(probeZ) +
                                                ") dim=" + std::to_string(dimId) + " method=stable-safe");
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                    MapRenderState::teleportStatusMsg.clear();
                                    probeDone = true;
                                } else {
                                    // 目标点为岩浆（主世界）或虚空（末地）或不安全 → 搜索周围安全落脚点
                                    int searchX = probeX, searchZ = probeZ;
                                    short nearbyY = -32000;
                                    if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16, dimId) &&
                                        IsTeleportSpotSafe(*region, searchX, nearbyY, searchZ, dimId) &&
                                        HasOpenSkyAbove(*region, searchX, nearbyY, searchZ, dimId)) {
                                        // 周围找到安全陆地/空岛且露天 → 传送到安全落脚点
                                        char coordBuf[128];
                                        std::snprintf(coordBuf, sizeof(coordBuf),
                                                      "/tp @s %.2f %.2f %.2f",
                                                      (float)searchX + 0.5f,
                                                      (float)nearbyY,
                                                      (float)searchZ + 0.5f);
                                        SendServerCommand(*player, coordBuf);
                                        LogTeleport("probe SUCCESS-nearby (" +
                                                    std::to_string(searchX) + "," +
                                                    std::to_string((int)nearbyY) + "," +
                                                    std::to_string(searchZ) +
                                                    ") [original lava/void, landed on safe ground, dim=" + std::to_string(dimId) + "]");
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Idle);
                                        MapRenderState::teleportStatusMsg.clear();
                                        probeDone = true;
                                    } else if (elapsedMs >= 4000) {
                                        // 超过 4 秒仍无任何安全露天落脚点 → 驳回传送，安全回退原位
                                        char abortBuf[128];
                                        std::snprintf(abortBuf, sizeof(abortBuf),
                                                      "/tp @s %.2f %.2f %.2f",
                                                      MapRenderState::probeOriginalX,
                                                      MapRenderState::probeOriginalY,
                                                      MapRenderState::probeOriginalZ);
                                        SendServerCommand(*player, abortBuf);
                                        LogTeleport("probe REJECT (" + std::to_string(probeX) + "," +
                                                    std::to_string(probeZ) + ") [target and surroundings unsafe/unready, reject] → original, dim=" + std::to_string(dimId));
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                                        MapRenderState::teleportStatusMsg.clear();
                                        MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                        probeDone = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (!probeDone) {
                // [超时降级] 下界 10 秒，其他 8 秒：回退原位（给远距离未探索区块生成与下发留出充足时间）
                int timeoutMs = (MapRenderState::probeIsNether || MapRenderState::currentDimensionId == 1) ? 10000 : 8000;
                if (elapsedMs >= timeoutMs) {
                    char abortBuf[128];
                    std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                  MapRenderState::probeOriginalX,
                                  MapRenderState::probeOriginalY,
                                  MapRenderState::probeOriginalZ);
                    SendServerCommand(*player, abortBuf);
                    LogTeleport("probe TIMEOUT abort→original (" +
                                std::to_string((int)MapRenderState::probeOriginalX) + "," +
                                std::to_string((int)MapRenderState::probeOriginalY) + "," +
                                std::to_string((int)MapRenderState::probeOriginalZ) +
                                ") [chunk not ready within " + std::to_string(timeoutMs / 1000) + "s]");
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                    MapRenderState::teleportStatusMsg.clear();
                    MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_TIMEOUT_MSG");
                    probeDone = true;
                }
            }

            if (probeDone) {
                MapRenderState::pendingSurfaceProbe.store(false);
                MapRenderState::probeStableCount = 0;
                MapRenderState::probeLastY = -32000;
                MapRenderState::probeLastX = 0;
                MapRenderState::probeLastZ = 0;
            }
        }

        // ==========================================
        // [分层地图系统] 跨维度与跨地下层级监听器
        // [性能] 原实现每帧调用 getLevelId/getSeed/getDefaultSpawn + 构造 worldId 字符串
        // (约 8-10 次堆分配) + 每帧 getBiome + biome.mHash->getString (2-3 次堆分配)。
        // 改为 30 帧 (0.5s) 节流：世界 ID 在游戏中几乎不变，生物群系名也只需半秒级刷新。
        // ==========================================
        static int s_worldBiomeCheckCounter = 0;
        if (++s_worldBiomeCheckCounter >= 30) {
            s_worldBiomeCheckCounter = 0;
            try {
                int dimId = (int)player->getDimensionId();
                std::string rawLevelId = "UnknownWorld";
                std::string seedStr = "0";
                std::string spawnStr = "0_0";

                try {
                    std::string lid = player->getLevel().getLevelId();
                    if (!lid.empty()) rawLevelId = lid;
                } catch(...) {}

                try {
                    unsigned int seed = player->getLevel().getSeed();
                    seedStr = std::to_string(seed);
                } catch(...) {}

                try {
                    BlockPos spawn = player->getLevel().getDefaultSpawn();
                    spawnStr = std::to_string(spawn.x) + "_" + std::to_string(spawn.z);
                } catch(...) {}

                if (rawLevelId.empty() || rawLevelId == "UnknownWorld") {
                    rawLevelId = "RemoteServer";
                }

                std::string finalWorldId = rawLevelId + "_S" + seedStr + "_P" + spawnStr;

                for (char& c : finalWorldId) {
                    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') c = '_';
                }

                if (MapRenderState::currentWorldId != finalWorldId || MapRenderState::currentDimensionId != dimId) {
                    MapRenderState::currentWorldId = finalWorldId;
                    MapRenderState::currentDimensionId = dimId;

                    MapCacheManager::SwitchWorld(finalWorldId, dimId);
                    WaypointManager::SwitchWorld(finalWorldId, dimId);

                    std::memset(g_mapHeights, 0, sizeof(g_mapHeights));
                    std::memset(g_mapColors, 0, sizeof(g_mapColors));
                    std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
                    std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));

                    MapRenderState::clearGPUCache.store(true);
                    g_mapDataUpdated.store(true);
                }
            } catch(...) {}

            try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    auto const& biome = regionPtr->getBiome(BlockPos(g_playerBlockX, (int)g_playerY, g_playerBlockZ));
                    {
                        std::string rawName = biome.mHash->getString();
                        std::string displayRawName = rawName;

                        // 剥离命名空间前缀（例如将 minecraft:plains 转为 plains）
                        size_t colonPos = displayRawName.find(":");
                        if (colonPos != std::string::npos) {
                            displayRawName = displayRawName.substr(colonPos + 1);
                        }

                        MapRenderState::rawBiomeName = displayRawName;
                        MapRenderState::translatedBiomeName = TranslateBiomeName(rawName);
                    }
                }
            } catch (...) {}
        }

        static bool lastUIState = false;
        bool currentUIState = MapRenderState::IsUIActive();
        if (currentUIState != lastUIState) {
            lastUIState = currentUIState;
            HWND hwnd = FindWindowW(L"Minecraft", NULL);
            if (!hwnd) hwnd = GetForegroundWindow();
            
            if (currentUIState) {
                this->releaseMouse();
                ClipCursor(NULL);
                if (hwnd) {
                    RECT rect; GetWindowRect(hwnd, &rect);
                    SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
                }
            } else {
                this->grabMouse();
                if (hwnd) {
                    RECT rect; GetWindowRect(hwnd, &rect);
                    SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
                }
            }
        }

        // [性能] 游戏 HWND 缓存：FindWindowW 每 tick 调用需要遍历所有顶层窗口，
        // 在多显示器系统上开销可达 ~1-2us；改为静态缓存 + 有效性验证（IsWindow），
        // 失效时才重新查找，整体减少 99% 的 FindWindowW 调用。
        static HWND s_cachedHwnd = nullptr;
        HWND hwnd = s_cachedHwnd;
        if (!hwnd || !IsWindow(hwnd)) {
            hwnd = FindWindowW(L"Minecraft", NULL);
            if (!hwnd) hwnd = GetForegroundWindow();
            s_cachedHwnd = hwnd;
        }

        static int s_clipCursorTickCounter = 0;
        static bool s_lastCursorHidden = false;
        static bool s_lastIsForeground = false;
        // 非 UI 激活时的指针锁定：GetForegroundWindow/GetCursorInfo/GetClientRect/ClientToScreen/ClipCursor
        // 合计 5 个系统调用，每 tick 调用频率 20Hz（50ms）虽然不重，但仍累计。
        // 用 3 tick（150ms）节流 + 状态变化时立即刷新，人眼完全感知不到延迟。
        bool stateChanged = false;
        bool isForeground = (hwnd && GetForegroundWindow() == hwnd);
        if (isForeground != s_lastIsForeground) { s_lastIsForeground = isForeground; stateChanged = true; }
        s_clipCursorTickCounter++;
        if (stateChanged || s_clipCursorTickCounter >= 3) {
            s_clipCursorTickCounter = 0;
            if (hwnd && isForeground) {
                if (!MapRenderState::IsUIActive()) {
                    CURSORINFO ci = {};
                    ci.cbSize = sizeof(CURSORINFO);
                    bool hiddenNow = false;
                    if (GetCursorInfo(&ci)) {
                        hiddenNow = (ci.flags == 0);
                        if (ci.flags == 0) {
                            RECT clientRect;
                            GetClientRect(hwnd, &clientRect);
                            POINT ptCenter = { (clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2 };
                            ClientToScreen(hwnd, &ptCenter);
                            RECT centerRect = { ptCenter.x - 1, ptCenter.y - 1, ptCenter.x + 1, ptCenter.y + 1 };
                            ClipCursor(&centerRect);
                        } else {
                            ClipCursor(NULL);
                        }
                    }
                    if (hiddenNow != s_lastCursorHidden) { s_lastCursorHidden = hiddenNow; }
                }
            } else {
                if (s_lastCursorHidden) {
                    ClipCursor(NULL);
                    s_lastCursorHidden = false;
                }
            }
        }

        static int currentScanX  = -99999;
        static int currentScanZ  = -99999;
        static bool isScanning   = false;
        static int  currentRow   = -MAP_DATA_RADIUS;
        static int  currentCol   = -MAP_DATA_RADIUS;
        static int  ticksSinceScan = 0;
        // [新增] 本次扫描采集的生物群系条目，扫描完成后批量写入 MapCacheManager
        // 声明在扫描状态变量处，以便在扫描启动时 clear()
        static std::vector<MapCacheManager::BiomeEntry> biomeEntries;
        // [洞穴检测冷却] 声明在扫描启动前，以便新扫描开始时重置为 0 (立即检测)
        static int s_caveDetectCooldown = 0;

        int px = g_playerBlockX;
        int pz = g_playerBlockZ;
        ticksSinceScan++;

        if (!isScanning && (std::abs(px - currentScanX) >= 8 || std::abs(pz - currentScanZ) >= 8 || ticksSinceScan > 50)) {
            currentScanX  = px;
            currentScanZ  = pz;
            ticksSinceScan = 0;

            isScanning = true;
            currentRow = -MAP_DATA_RADIUS;
            currentCol = -MAP_DATA_RADIUS;
            biomeEntries.clear();  // [新增] 新扫描周期开始，清空采集缓冲
            // [防石头污染] 新扫描开始时强制立即运行洞穴检测，
            // 避免在洞穴检测冷却期间地表扫描写入石头数据到缓存
            s_caveDetectCooldown = 0;
            // [防残留] 清零后台缓冲区, 确保洞穴→地表切换时不残留旧洞穴数据
            std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
            std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
        }

        // [跑图跟随] 扫描进行中玩家继续移动时, 平移扫描窗口跟随玩家 (详见 ShiftScanGrid 注释)。
        // 使扫描中心始终贴近玩家, 避免小地图纹理中心大幅滞后导致 UV 越界采样条纹。
        if (isScanning && (std::abs(px - currentScanX) >= 16 || std::abs(pz - currentScanZ) >= 16)) {
            int shiftX = px - currentScanX;
            int shiftZ = pz - currentScanZ;
            if (std::abs(shiftX) >= MAP_DATA_SIZE || std::abs(shiftZ) >= MAP_DATA_SIZE) {
                // 异常跳转 (平移量超过缓冲区): 放弃平移, 整幅重扫
                currentScanX = px;
                currentScanZ = pz;
                currentRow = -MAP_DATA_RADIUS;
                currentCol = -MAP_DATA_RADIUS;
                std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
                std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
            } else {
                // 后台缓冲仅本 tick 线程读写 (缓存写入走独立拷贝 g_cacheWriteColors), 无需加锁
                ShiftScanGrid(g_mapColorsBack, shiftX, shiftZ);
                ShiftScanGrid(g_mapHeightsBack, shiftX, shiftZ);
                currentScanX += shiftX;
                currentScanZ += shiftZ;
                // 游标换算到新中心坐标系并夹取到有效范围;
                // 换算后可能落在少量已扫描列上, 重扫无害 (数据以最新扫描为准)
                currentRow = std::clamp(currentRow - shiftX, -MAP_DATA_RADIUS, MAP_DATA_RADIUS);
                currentCol = std::clamp(currentCol - shiftZ, -MAP_DATA_RADIUS, MAP_DATA_RADIUS);
            }
        }

        // ==================== 洞穴地图检测 (Xaero's Cave Map 1:1 复刻) ====================
        // 每帧检测玩家是否在地下洞穴中, 设置 g_caveModeActive 供渲染层使用
        int effectiveCaveType = MapRenderState::g_caveModeType;

        // [下界地图] 下界无天空光照 (hasSkylight=false), 永远视为洞穴模式
        // 对应 Xaero's: ambientLight < 0.25 且非末地 → 启用 cave lighting
        if (MapRenderState::currentDimensionId == 1) {
            MapRenderState::g_caveModeActive = true;
            MapRenderState::g_caveStartY = 120;  // 下界天花板以下, 避开基岩层
        } else if (s_caveDetectCooldown > 0) {
            s_caveDetectCooldown--;
        }

        if (effectiveCaveType != 0 && MapRenderState::currentDimensionId != 1 && s_caveDetectCooldown == 0) {
            try {
                if (MapRenderState::g_caveTopYAuto) {
                    // Auto 模式: 自动检测洞穴
                    BlockSource* regionPtr = this->getRegion();
                    if (regionPtr) {
                        int caveStartY = 0;
                        bool inCave = DetectCaveStart(*regionPtr, px, (int)g_playerY, pz, caveStartY);
                        // [防残留] 洞穴→地表切换时强制触发重扫, 清除缓存中的旧洞穴数据
                        if (MapRenderState::g_caveModeActive && !inCave) {
                            ticksSinceScan = 101;
                        }
                        MapRenderState::g_caveModeActive = inCave;
                        if (inCave) {
                            MapRenderState::g_caveStartY = caveStartY;
                        }
                    }
                } else {
                    // 手动 Top Y 模式: 始终启用洞穴, 使用用户指定的 Top Y
                    MapRenderState::g_caveModeActive = true;
                    MapRenderState::g_caveStartY = MapRenderState::g_caveTopY;
                }
            } catch (...) {
                MapRenderState::g_caveModeActive = false;
            }
            s_caveDetectCooldown = 5;  // 每 5 帧检测一次 (约 0.25 秒)
        } else if (effectiveCaveType == 0 && MapRenderState::currentDimensionId != 1) {
            // Off 模式 (仅主世界): 不启用洞穴渲染
            MapRenderState::g_caveModeActive = false;
            // [防Off模式地表污染] 仍需检测玩家是否在地下洞穴中
            // 原因: SafeGetSurfaceY 在地下时可能返回洞穴天花板 Y (区块部分加载),
            //       导致地表扫描读取石头方块, 扫描完成后 UpdateFromScan 永久写入缓存 → 灰石污染
            // 修复: 检测到地下时中止地表扫描, 保留缓存中的纯净地表数据
            if (s_caveDetectCooldown == 0) {
                try {
                    BlockSource* regionPtr = this->getRegion();
                    if (regionPtr) {
                        int detectY = 0;
                        bool inCave = DetectCaveStart(*regionPtr, px, (int)g_playerY, pz, detectY);
                        if (inCave && isScanning) {
                            // 玩家在地下但洞穴模式关闭: 中止地表扫描, 防止读取洞穴天花板石头
                            isScanning = false;
                            currentRow = -MAP_DATA_RADIUS;
                            currentCol = -MAP_DATA_RADIUS;
                            ticksSinceScan = 60;  // ~3秒后重试, 等玩家可能离开洞穴
                        }
                    }
                } catch (...) {}
                s_caveDetectCooldown = 5;
            }
        }

        // [维度切换] 维度变化时中止当前扫描, 避免跨维度数据混合
        static int s_prevDimension = 0;
        static bool s_prevCaveActive = false;
        if (s_prevDimension != MapRenderState::currentDimensionId) {
            s_prevDimension = MapRenderState::currentDimensionId;
            s_prevCaveActive = MapRenderState::g_caveModeActive;  // 同步, 避免下帧再次触发模式切换中止
            if (isScanning) {
                isScanning = false;
                currentRow = -MAP_DATA_RADIUS;
                currentCol = -MAP_DATA_RADIUS;
                std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
                std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
                ticksSinceScan = 101;  // 强制下帧启动新扫描
            }
        }

        // [防地表污染核心修复] 洞穴模式切换时, 立即中止当前进行中的扫描
        // 原因: 洞穴扫描与地表扫描是 if/else if 互斥分支。若切换发生在扫描中途,
        //   已扫部分是旧模式数据(如洞穴灰石 alpha=1), 后半段是新模式数据,
        //   完成后整份混合数据被 UpdateFromScan 写入缓存 → 永久灰黑块污染。
        // 修复: 检测到 g_caveModeActive 翻转时, 丢弃当前半成品扫描, 下帧重新开始纯模式扫描。
        if (s_prevCaveActive != MapRenderState::g_caveModeActive) {
            s_prevCaveActive = MapRenderState::g_caveModeActive;
            isScanning = false;
            currentRow = -MAP_DATA_RADIUS;
            currentCol = -MAP_DATA_RADIUS;
            std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
            std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
            ticksSinceScan = 101;  // 强制下帧立即启动新模式扫描

            // 注意：不要清空前台缓冲 g_mapColors！
            // 保留当前前台地图画面直至新模式完整扫描完成后由 memcpy 原子替换，彻底杜绝切换瞬间的黑屏/黑块！
        }

        // [洞穴扫描] 玩家在地下且洞穴模式启用时, 执行洞穴列扫描代替地表扫描
        // 对应 Xaero's MapWriter.writeChunk: 从 caveStart 向下扫描 caveDepth 格
        if (isScanning && MapRenderState::g_caveModeActive && (effectiveCaveType != 0 || MapRenderState::currentDimensionId == 1)) {
            try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    BlockSource& region = *regionPtr;
                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    // 洞穴扫描深度
                    int caveDepth = MapRenderState::g_caveDepth;

                    // 统一 Top Y 投影:
                    // 所有列从同一高度向下解析, 避免相邻列跳到不同高度层产生杂色条纹。
                    int currentCaveTopY;
                    int currentCaveScanDepth;
                    if (MapRenderState::currentDimensionId == 1) {
                        currentCaveTopY = std::clamp((int)g_playerY + 16, -64, 120);
                        currentCaveScanDepth = 80;
                    } else if (!MapRenderState::g_caveTopYAuto) {
                        currentCaveTopY = std::clamp(MapRenderState::g_caveTopY, -64, 319);
                        currentCaveScanDepth = caveDepth;
                    } else {
                        // Auto 模式: 紧随玩家所在洞穴层
                        // 上限以 playerY + 16 为基准；若局部通道天花板更低，则以天花板为界避开岩石层
                        int headroom = 16;
                        int targetTop = (int)g_playerY + headroom;
                        if (MapRenderState::g_caveStartY > (int)g_playerY && MapRenderState::g_caveStartY < targetTop) {
                            targetTop = MapRenderState::g_caveStartY;
                        }
                        currentCaveTopY = std::clamp(targetTop, -60, 319);
                        currentCaveScanDepth = caveDepth;
                    }

                    while (currentRow <= MAP_DATA_RADIUS && !timeBudgetExceeded) {
                        int dx = currentRow;
                        int arrX = dx + MAP_DATA_RADIUS;

                        while (currentCol <= MAP_DATA_RADIUS) {
                            int dz = currentCol;
                            int targetX = currentScanX + dx;
                            int targetZ = currentScanZ + dz;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            int startY = currentCaveTopY;
                            int scanDepth = currentCaveScanDepth;

                            std::string blockName;
                            int blockY = 0, depth = 0;
                            bool hasWater = false;

                            if (startY > -64 && ScanColumnCave(region, targetX, targetZ, startY, scanDepth, blockName, blockY, depth, hasWater)) {
                                g_mapHeightsBack[arrX][arrZ] = (float)blockY;

                                // 深洞: 有空气通道但指定深度内无地板，渲染深灰避免泄露地表色
                                if (blockName == "__CAVE_DEEPHOLE__") {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.08f, 0.08f, 0.08f, 1.0f);
                                } else {
                                    // 液体方块: 使用饱和颜色, 不应用深度衰减
                                    mce::Color liquidCol = GetCaveLiquidColor(blockName);
                                    if (liquidCol.a > 0.0f) {
                                        g_mapColorsBack[arrX][arrZ] = liquidCol;
                                    } else {
                                        // 实心方块: 使用洞穴专用颜色表 (含矿石/石头变种/基岩等)
                                        mce::Color baseColor = GetCaveBlockColor(blockName);

                                        // 水色叠加: 若列内含水体，叠加半透明水色
                                        if (hasWater) {
                                            baseColor = BlendWaterOverFloor(baseColor, mce::Color(0.20f, 0.52f, 0.88f, 1.0f));
                                        }

                                        // 应用深度亮度衰减
                                        float brightness = ComputeCaveBrightness(depth, scanDepth);
                                        g_mapColorsBack[arrX][arrZ] = mce::Color(
                                            baseColor.r * brightness,
                                            baseColor.g * brightness,
                                            baseColor.b * brightness,
                                            1.0f  // 洞穴模式 alpha=1 (完全不透明)
                                        );
                                    }
                                }
                            } else {
                                // 列内无方块 = 空气/未探索 = 透明 (显示为黑色背景)
                                g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                                g_mapHeightsBack[arrX][arrZ] = 0.0f;
                            }

                            currentCol++;

                            if ((currentCol & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > 500) {
                                    timeBudgetExceeded = true;
                                    break;
                                }
                            }
                        }

                        if (timeBudgetExceeded) break;

                        if (currentCol > MAP_DATA_RADIUS) {
                            currentCol = -MAP_DATA_RADIUS;
                            currentRow++;
                        }
                    }

                    if (currentRow > MAP_DATA_RADIUS) {
                        isScanning = false;
                        {
                            std::lock_guard<std::mutex> lock(g_mapDataMutex);
                            std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                            std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                            g_lastRenderX = currentScanX;
                            g_lastRenderZ = currentScanZ;
                            g_mapDataUpdated.store(true);
                        }

                        // [持久化] 异步写入缓存 (含下界/洞穴数据)
                        // [性能] 使用持久化 worker 线程，避免每次扫描完成时创建线程 + 5MB 堆分配
                        SubmitCacheWrite(currentScanX, currentScanZ, MapRenderState::currentDimensionId, true, biomeEntries);
                    }
                }
            } catch (...) {}
        } else if (isScanning && !MapRenderState::g_caveModeActive) {
        // ==================== 地表扫描 (原有逻辑) ====================
        // 玩家不在洞穴时执行正常地表扫描
        try {
                BlockSource* regionPtr = this->getRegion();
                if (regionPtr) {
                    BlockSource& region = *regionPtr;
                    int rowsThisFrame = 0;

                    static std::string s_lastBlockName = "";
                    static mce::Color s_lastBlockColor(0, 0, 0, 0);
                    static int s_biomeCellX = -99999;
                    static int s_biomeCellZ = -99999;
                    static std::string s_biomeName = "";
                    // [性能] 记录当前生物群系名称 hash，后续每格计算 cacheKey 时
                    // 直接复用该值，避免重复对 s_biomeName 做 hash（s_biomeName 每 4x4 格才变化一次）
                    static uint64_t s_biomeNameHash = 0;
                    static mce::Color s_cachedGrass(0,0,0,0), s_cachedFoliage(0,0,0,0), s_cachedWater(0,0,0,0);

                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    while (currentRow <= MAP_DATA_RADIUS && !timeBudgetExceeded) {
                        int dx = currentRow;
                        int arrX = dx + MAP_DATA_RADIUS;

                        while (currentCol <= MAP_DATA_RADIUS) {
                            int dz = currentCol;
                            int targetX = currentScanX + dx;
                            int targetZ = currentScanZ + dz;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            // [安全] 使用 SafeGetSurfaceY (SEH 包装)，防止部分加载区块 AV 崩溃
                            short topY = SafeGetSurfaceY(region, targetX, targetZ);
                            g_mapHeightsBack[arrX][arrZ] = (float)topY;

                            // [防洞穴顶石] 跳过 Y 值偏离玩家过远的列
                            if (topY > -64 && std::abs((int)topY - (int)g_playerY) <= 100) {
                                Block const& block = region.getBlock(BlockPos(targetX, topY - 1, targetZ));
                                std::string blockName = block.getTypeName();

                                // [防地表灰石污染] 检测到石头类方块时, 回退查缓存地表Y重新读取
                                // 原因: 部分加载区块 SafeGetSurfaceY 可能返回洞穴天花板Y而非真实地表Y,
                                //       读取到石头/深板岩等写入缓存, 在地表大地图显示为灰色块状污染。
                                // 修复: 若缓存中有更高的地表Y (差值>=15), 用缓存Y重新读取地表方块(如草),
                                //       覆盖灰石数据。无缓存时不干预(可能是合法石头地表)。
                                if (IsStoneLikeBlock(blockName)) {
                                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(targetX, targetZ);
                                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > (int16_t)topY + 15) {
                                        std::string surfName;
                                        if (SafeGetBlockName(region, targetX, cachedY - 1, targetZ, surfName) && !surfName.empty()) {
                                            topY = cachedY;
                                            g_mapHeightsBack[arrX][arrZ] = (float)topY;
                                            blockName = surfName;
                                        }
                                    }
                                }

                                if (blockName.find("snow") != std::string::npos) {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                } else {
                                    Block const& blockAbove = region.getBlock(BlockPos(targetX, topY, targetZ));
                                    std::string aboveName = blockAbove.getTypeName();

                                    if (IsInvisibleOrTechnicalOverlay(aboveName)) {
                                        // 隐形或空气类技术方块，跳过替换，保留底层真实地表方块
                                    } else if (aboveName.find("snow") != std::string::npos) {
                                        g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                        blockName = "";
                                    } else {
                                        blockName = aboveName;
                                    }

                                    if (!blockName.empty()) {
                                        int cellX = targetX >> 2;
                                        int cellZ = targetZ >> 2;
                                        if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
                                            s_biomeCellX = cellX; s_biomeCellZ = cellZ;
                                            try {
                                                auto const& biome = region.getBiome(BlockPos(targetX, topY - 1, targetZ));
                                                std::string newBiomeName = biome.mHash->getString();
                                                if (s_biomeName != newBiomeName) {
                                                    s_biomeName = newBiomeName;
                                                    s_biomeNameHash = Fnv1aHash(s_biomeName);
                                                    getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                }
                                                biomeEntries.push_back({cellX, cellZ, newBiomeName});
                                            } catch (...) {
                                                if (!s_biomeName.empty()) { s_biomeName = ""; s_biomeNameHash = 0; }
                                            }
                                        }

                                        // [性能] 颜色缓存：原使用 std::hash<string> (MSVC 实现慢、需分配 SSO 外字符串)，
                                        // 替换为 FNV-1a 64bit inline hash。
                                        // 同时提高缓存上限到 65536：常见方块+生物群系组合约 80 方块 x 60 生物群系 ≈ 4800，
                                        // 留足余量避免频繁 clear 导致缓存命中率骤降。
                                        static std::unordered_map<uint64_t, mce::Color> s_globalColorCache;
                                        if (s_globalColorCache.size() > 65536) s_globalColorCache.clear();

                                        uint64_t blockHash = Fnv1aHash(blockName);
                                        uint64_t cacheKey = blockHash ^ (s_biomeNameHash + 0x9e3779b97f4a7c15ULL + (blockHash << 6) + (blockHash >> 2));
                                        auto it = s_globalColorCache.find(cacheKey);

                                        if (it != s_globalColorCache.end()) {
                                            g_mapColorsBack[arrX][arrZ] = it->second;
                                        } else {
                                            mce::Color calculatedColor = getBlockColor(blockName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                            s_globalColorCache[cacheKey] = calculatedColor;
                                            g_mapColorsBack[arrX][arrZ] = calculatedColor;
                                        }
                                    }
                                }
                                // 移植0.3.4水域渲染：水面方块向下穿透水层找海床，海床色+水色叠加
                                // 原实现直接返回纯水色(waterCol)，无海床底色，深海区呈纯蓝缺乏层次。
                                // 0.3.4: 检测 topY-1 为水体材质 → 向下64格找首个非穿透方块(海床) →
                                //        海床原色 + 0.65 alpha 水色叠加，保留海床纹理同时覆盖水色。
                                if (IsCaveWaterBlock(block)) {
                                    int seaFloor = (int)topY - 1;
                                    int surfaceY = seaFloor + 1;
                                    while (seaFloor > -64 && (surfaceY - seaFloor) < 64) {
                                        try {
                                            Block const& seaFloorBlock = region.getBlock(BlockPos(targetX, seaFloor, targetZ));
                                            if (!IsCavePassableBlock(seaFloorBlock)) break;
                                        } catch (...) { break; }
                                        seaFloor--;
                                    }
                                    // [修复] 保持 g_mapHeightsBack 为 topY (真实水面高度)，严禁覆盖为海床高度 seaFloor
                                    // 否则写入缓存后全屏大地图传送到水域会传送到水底 (海床) 而非水面
                                    g_mapHeightsBack[arrX][arrZ] = (float)topY;
                                    try {
                                        Block const& seaFloorBlock = region.getBlock(BlockPos(targetX, seaFloor, targetZ));
                                        mce::Color seaFloorColor = getBlockColor(
                                            seaFloorBlock.getTypeName(),
                                            s_cachedGrass, s_cachedFoliage, s_cachedWater
                                        );
                                        g_mapColorsBack[arrX][arrZ] = BlendWaterOverFloor(seaFloorColor, s_cachedWater);
                                    } catch (...) {
                                        g_mapColorsBack[arrX][arrZ] = BlendWaterOverFloor(
                                            mce::Color(0.08f, 0.08f, 0.08f, 1.0f), s_cachedWater
                                        );
                                    }
                                }
                            } else {
                                g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                            }

                            currentCol++;

                            if ((currentCol & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > 500) {
                                    timeBudgetExceeded = true;
                                    break;
                                }
                            }
                        }

                        if (timeBudgetExceeded) break;

                        if (currentCol > MAP_DATA_RADIUS) {
                            currentCol = -MAP_DATA_RADIUS;
                            currentRow++;
                        }
                    }

                    if (currentRow > MAP_DATA_RADIUS) {
                        isScanning = false;

                        // [防污染最终防线] 扫描完成时再次检测玩家是否在地下
                        // 若在地下, 跳过本次扫描数据的提交 (g_mapColors 和缓存), 保留之前的地表数据
                        // 原因: 即使 Off 模式有中途中止检测 (每10帧), 扫描仍可能在检测冷却期内完成,
                        //       将洞穴天花板石头数据写入缓存造成永久污染
                        bool playerUnderground = false;
                        if (MapRenderState::currentDimensionId != 1) {
                            try {
                                int detectY = 0;
                                playerUnderground = DetectCaveStart(region, px, (int)g_playerY, pz, detectY);
                            } catch (...) {}
                        }

                        if (!playerUnderground) {
                            {
                                std::lock_guard<std::mutex> lock(g_mapDataMutex);
                                std::memcpy(g_mapHeights, g_mapHeightsBack, sizeof(g_mapHeights));
                                std::memcpy(g_mapColors, g_mapColorsBack, sizeof(g_mapColors));
                                g_lastRenderX = currentScanX;
                                g_lastRenderZ = currentScanZ;
                                g_mapDataUpdated.store(true);
                            }

                            // [性能] 使用持久化 worker 线程，避免每次扫描完成时创建线程 + 5MB 堆分配
                            SubmitCacheWrite(currentScanX, currentScanZ, MapRenderState::currentDimensionId, false, biomeEntries);
                        } else {
                            // 玩家在地下: 丢弃本次地表扫描数据, 防止洞穴天花板石头污染地表缓存;
                            // 同时若洞穴模式开启，立即激活洞穴模式并强制下帧启动洞穴扫描
                            if (effectiveCaveType != 0) {
                                MapRenderState::g_caveModeActive = true;
                                ticksSinceScan = 101;
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        static int entityDelay = 0;
        if (++entityDelay >= 4) { // 提高刷新率至约 5Hz (原60为3秒/次，移动严重跳变)，近乎零开销实现丝滑雷达
            entityDelay = 0;
            if (MapRenderState::showRadar) {
                std::vector<RadarEntity> tempEntities;
                auto& level = player->getLevel();
                const auto& entities = level.getRuntimeActorList();
                
                for (auto* actor : entities) {
                    if (!actor || actor == player) continue;
                    if (!actor->isAlive()) continue;
                    if (actor->getDimensionId() != player->getDimensionId()) continue;

                    const Vec3& ePos = actor->getPosition();
                    float dx = ePos.x - pos.x;
                    float dz = ePos.z - pos.z;

                    if (dx * dx + dz * dz > MAP_DATA_RADIUS * MAP_DATA_RADIUS) continue;

                    int type = 2; 
                    if (actor->isPlayer()) type = 0;
                    else if (actor->hasCategory(ActorCategory::Item)) type = 3;
                    else if (actor->hasCategory(ActorCategory::Monster)) type = 1;
                    
                    tempEntities.push_back({ePos.x, ePos.y, ePos.z, type});
                }
                g_radarEntities = tempEntities;
                g_radarUpdated.store(true);
            } else if (!g_radarEntities.empty()) {
                g_radarEntities.clear();
                g_radarUpdated.store(true);
            }
        }

    } else {
        g_hasPlayer   = false;
        g_localPlayer = nullptr;
    }

    return result;
}

// ==========================================
// [大地图模式] 视角与交互底层物理锁死模块
// ==========================================

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerApplyTurnDeltaHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::_applyTurnDelta,
    void,
    Vec2 const& rotationDelta
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(rotationDelta); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(rotationDelta);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeStartDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$startDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    bool& isDestroyed
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(pos, face, isDestroyed);
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, isDestroyed);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItem,
    bool,
    ItemStack& item
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerPickBlockHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::pickBlock,
    void,
    HitResult const& hitResult,
    bool withData
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(hitResult, withData); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(hitResult, withData);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySelectSlotHook,
    ll::memory::HookPriority::Normal,
    PlayerInventory,
    &PlayerInventory::selectSlot,
    bool,
    int slot,
    ContainerID containerId
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(slot, containerId);
    if (MapRenderState::IsUIActive()) return false;
    return origin(slot, containerId);
}

LL_TYPE_INSTANCE_HOOK(
    PlayerInventorySetItemHook,
    ll::memory::HookPriority::Normal,
    Inventory,
    &Inventory::$setItem,
    void,
    int slot,
    ItemStack const& item
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(slot, item); return; }
    if (MapRenderState::IsUIActive()) return;
    origin(slot, item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$attack,
    bool,
    Actor& actor,
    Vec3 const& hitPosition
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(actor, hitPosition);
    if (MapRenderState::IsUIActive()) return false;
    return origin(actor, hitPosition);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSwingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$swing,
    bool,
    ActorSwingSource swingSource
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(swingSource);
    if (MapRenderState::IsUIActive()) return false;
    return origin(swingSource);
}

// 【彻底阻断打空气音效】当模组 UI 激活时，拦截打空/未击中逻辑，消除 AttackNoDamage 音效与发包
LL_TYPE_INSTANCE_HOOK(
    LocalPlayerMissedSwingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::missedSwing,
    void
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(); return; }
    if (MapRenderState::IsUIActive()) return;
    origin();
}

LL_TYPE_INSTANCE_HOOK(
    GameModeInteractHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$interact,
    bool,
    Actor& entity,
    Vec3 const& location
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(entity, location);
    if (MapRenderState::IsUIActive()) return false;
    return origin(entity, location);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemAsAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemAsAttack,
    bool,
    ItemStack& item,
    Vec3 const& aimDirection
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, aimDirection);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item, aimDirection);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeUseItemOnHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$useItemOn,
    InteractionResult,
    ItemStack& item,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& hitPos,
    Block const* block,
    bool isFirstEvent
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item, pos, face, hitPos, block, isFirstEvent);
    if (MapRenderState::IsUIActive()) return InteractionResult{false, false};
    return origin(item, pos, face, hitPos, block, isFirstEvent);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeContinueDestroyBlockHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$continueDestroyBlock,
    bool,
    BlockPos const& pos,
    unsigned char face,
    Vec3 const& playerPos,
    bool& hasDestroyedBlock
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(pos, face, playerPos, hasDestroyedBlock);
    if (MapRenderState::IsUIActive()) return false;
    return origin(pos, face, playerPos, hasDestroyedBlock);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerJumpFromGroundHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::canJump,
    bool
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin();
    if (MapRenderState::IsUIActive() && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return false;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerSetSneakingHook,
    ll::memory::HookPriority::Normal,
    LocalPlayer,
    &LocalPlayer::$setSneaking,
    void,
    bool isSneaking
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(isSneaking); return; }
    if (MapRenderState::IsUIActive() && g_localPlayer && this == g_localPlayer) return;
    origin(isSneaking);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerIsImmobileHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$isImmobile,
    bool
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin();
    if (MapRenderState::IsUIActive() && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return true;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(
    MobSetCarriedItemHook,
    ll::memory::HookPriority::Normal,
    Actor,
    &Actor::$setCarriedItem,
    void,
    ItemStack const& item
) {
    if (MapRenderState::g_isShuttingDown.load()) { origin(item); return; }
    if (MapRenderState::IsUIActive() && g_localPlayer && this == (Actor*)g_localPlayer) return;
    origin(item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeBaseUseItemHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::baseUseItem,
    bool,
    ItemStack const& item
) {
    if (MapRenderState::g_isShuttingDown.load()) return origin(item);
    if (MapRenderState::IsUIActive()) return false;
    return origin(item);
}