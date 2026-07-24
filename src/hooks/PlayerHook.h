#pragma once
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
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
#include <mc/world/level/biome/Biome.h>
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

// ==========================================
// 生物群系中文翻译字典引擎
// ==========================================
    inline std::string TranslateBiomeName(const std::string& rawName) {
        std::string cleanName = rawName;
        size_t colonPos = cleanName.find(":");
        if (colonPos != std::string::npos) cleanName = cleanName.substr(colonPos + 1);

        std::string lower = cleanName;
        for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

        // 数据驱动有序子串规则表：首个匹配胜出。
        // 顺序至关重要——具体变体必须在通用之前：
        //   roofed_forest/dark_forest before forest
        //   ice_plains_spikes before ice_plains
        //   savanna_mutated/windswept_savanna before savanna and before bare "windswept"
        //   extreme_hills_plus_trees before extreme_hills
        //   mesa_bryce/mesa_plateau_stone before mesa
        // 同时兼容基岩版旧 ID（roofed_forest/cold_taiga/jungle_edge/stone_beach/mesa_bryce 等）
        // 与 Java 版新 ID（dark_forest/snowy_taiga/sparse_jungle/stony_shore/eroded_badlands 等）
        static const std::vector<std::pair<std::string, std::string>> kBiomeRules = {
            // === 末地生物群系 ===
            {"end_highlands", "BIOME_THE_END"},
            {"end_midlands", "BIOME_THE_END"},
            {"end_barrens", "BIOME_THE_END"},
            {"small_end_islands", "BIOME_THE_END"},
            {"the_end", "BIOME_THE_END"},
            // === 下界生物群系 ===
            {"crimson_forest", "BIOME_CRIMSON_FOREST"},
            {"warped_forest", "BIOME_WARPED_FOREST"},
            {"soulsand_valley", "BIOME_SOUL_SAND_VALLEY"},
            {"soul_sand_valley", "BIOME_SOUL_SAND_VALLEY"},
            {"basalt_deltas", "BIOME_BASALT_DELTAS"},
            {"nether_wastes", "BIOME_HELL"},
            {"hell", "BIOME_HELL"},
            // === 海洋（深海→普通，特定→通用）===
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
            // === 河流 ===
            {"frozen_river", "BIOME_FROZEN_RIVER"},
            {"river", "BIOME_RIVER"},
            // === 沙滩/海岸（兼容 Bedrock stone_beach/cold_beach 与 Java stony_shore/snowy_beach）===
            {"stone_beach", "BIOME_STONY_SHORE"},
            {"stony_shore", "BIOME_STONY_SHORE"},
            {"cold_beach", "BIOME_SNOWY_BEACH"},
            {"snowy_beach", "BIOME_SNOWY_BEACH"},
            {"beach", "BIOME_BEACH"},
            // === 积雪生物群系（ice_plains_spikes 必须在 ice_plains 之前）===
            {"ice_plains_spikes", "BIOME_ICE_SPIKES"},
            {"ice_spikes", "BIOME_ICE_SPIKES"},
            {"ice_mountains", "BIOME_SNOWY_SLOPES"},
            {"snowy_slopes", "BIOME_SNOWY_SLOPES"},
            {"ice_plains", "BIOME_SNOWY_PLAINS"},
            {"snowy_plains", "BIOME_SNOWY_PLAINS"},
            // === 山峰 ===
            {"jagged_peaks", "BIOME_JAGGED_PEAKS"},
            {"frozen_peaks", "BIOME_FROZEN_PEAKS"},
            {"stony_peaks", "BIOME_STONY_PEAKS"},
            {"grove", "BIOME_GROVE"},
            // === 风袭丘陵（旧 Extreme Hills；windswept_savanna 必须在 bare "windswept" 之前）===
            {"extreme_hills_plus_trees", "BIOME_WINDSWEPT_FOREST"},
            {"windswept_forest", "BIOME_WINDSWEPT_FOREST"},
            {"extreme_hills_mutated", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
            {"windswept_gravelly_hills", "BIOME_WINDSWEPT_GRAVELLY_HILLS"},
            {"windswept_hills", "BIOME_EXTREME_HILLS"},
            {"windswept_savanna", "BIOME_WINDSWEPT_SAVANNA"},
            {"extreme_hills_edge", "BIOME_EXTREME_HILLS"},
            {"extreme_hills", "BIOME_EXTREME_HILLS"},
            {"windswept", "BIOME_EXTREME_HILLS"},
            // === 恶地（旧 Mesa；mesa_bryce/mesa_plateau_stone 必须在 mesa 之前）===
            {"mesa_bryce", "BIOME_ERODED_BADLANDS"},
            {"eroded_badlands", "BIOME_ERODED_BADLANDS"},
            {"mesa_plateau_stone", "BIOME_WOODED_BADLANDS"},
            {"wooded_badlands", "BIOME_WOODED_BADLANDS"},
            {"mesa_plateau", "BIOME_MESA"},
            {"mesa", "BIOME_MESA"},
            {"badlands", "BIOME_MESA"},
            // === 草甸/樱花 ===
            {"cherry_grove", "BIOME_CHERRY"},
            {"cherry", "BIOME_CHERRY"},
            {"meadow", "BIOME_MEADOW"},
            // === 热带草原（savanna_mutated 为 Bedrock 版 Windswept Savanna ID）===
            {"savanna_mutated", "BIOME_WINDSWEPT_SAVANNA"},
            {"savanna_plateau", "BIOME_SAVANNA_PLATEAU"},
            {"savanna", "BIOME_SAVANNA"},
            // === 丛林（jungle_edge 为 Bedrock 版 Sparse Jungle ID）===
            {"bamboo_jungle", "BIOME_BAMBOO_JUNGLE"},
            {"jungle_edge", "BIOME_SPARSE_JUNGLE"},
            {"sparse_jungle", "BIOME_SPARSE_JUNGLE"},
            {"jungle", "BIOME_JUNGLE"},
            // === 沼泽（swampland 为 Bedrock 版 ID）===
            {"mangrove_swamp", "BIOME_MANGROVE_SWAMP"},
            {"swampland", "BIOME_SWAMP"},
            {"swamp", "BIOME_SWAMP"},
            // === 蘑菇岛（mushroom_island 为 Bedrock 版 ID）===
            {"mushroom_island", "BIOME_MUSHROOM"},
            {"mushroom_fields", "BIOME_MUSHROOM"},
            {"mushroom", "BIOME_MUSHROOM"},
            // === 苍白花园 ===
            {"pale_garden", "BIOME_PALE_GARDEN"},
            // === 针叶林（cold_taiga/mega_taiga/redwood_taiga 为 Bedrock 版旧 ID）===
            {"cold_taiga", "BIOME_SNOWY_TAIGA"},
            {"snowy_taiga", "BIOME_SNOWY_TAIGA"},
            {"mega_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
            {"old_growth_pine_taiga", "BIOME_OLD_GROWTH_PINE_TAIGA"},
            {"redwood_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
            {"old_growth_spruce_taiga", "BIOME_OLD_GROWTH_SPRUCE_TAIGA"},
            {"taiga", "BIOME_TAIGA"},
            // === 森林（dark/roofed/birch/flower 必须在 bare "forest" 之前——修复黑森林识别 bug 的核心）===
            {"dark_oak_forest", "BIOME_DARK_FOREST"},
            {"roofed_forest", "BIOME_DARK_FOREST"},
            {"dark_forest", "BIOME_DARK_FOREST"},
            {"birch_forest_mutated", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
            {"old_growth_birch_forest", "BIOME_OLD_GROWTH_BIRCH_FOREST"},
            {"birch_forest", "BIOME_BIRCH_FOREST"},
            {"flower_forest", "BIOME_FLOWER_FOREST"},
            {"forest", "BIOME_FOREST"},
            // === 平原 ===
            {"sunflower_plains", "BIOME_SUNFLOWER_PLAINS"},
            {"plains", "BIOME_PLAINS"},
            // === 沙漠 ===
            {"desert", "BIOME_DESERT"},
        };

        std::string biomeKey = "BIOME_UNKNOWN";
        for (const auto& rule : kBiomeRules) {
            if (lower.find(rule.first) != std::string::npos) {
                biomeKey = rule.second;
                break;
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
inline void getBiomeTints(std::string const& biomeName, mce::Color& grass, mce::Color& foliage, mce::Color& water) {
    grass   = mce::Color(0.32f, 0.45f, 0.22f, 1.0f);
    foliage = mce::Color(0.22f, 0.38f, 0.15f, 1.0f);
    water   = mce::Color(0.18f, 0.38f, 0.85f, 1.0f);

    if (biomeName.empty()) return;

    std::string lower = biomeName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;

    if (lower.find("cherry") != std::string::npos) {
        grass   = mce::Color(0.44f, 0.56f, 0.26f, 1.0f);
        foliage = mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
    }
    else if (lower.find("desert") != std::string::npos || lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) {
        grass   = mce::Color(0.45f, 0.42f, 0.20f, 1.0f);
        foliage = mce::Color(0.35f, 0.38f, 0.18f, 1.0f);
        water   = mce::Color(0.15f, 0.45f, 0.45f, 1.0f);
    } 
    else if (lower.find("savanna") != std::string::npos) {
        grass   = mce::Color(0.42f, 0.42f, 0.18f, 1.0f);
        foliage = mce::Color(0.32f, 0.35f, 0.15f, 1.0f);
    } 
    else if (lower.find("jungle") != std::string::npos || lower.find("bamboo") != std::string::npos) {
        grass   = mce::Color(0.32f, 0.50f, 0.18f, 1.0f);
        foliage = mce::Color(0.22f, 0.45f, 0.15f, 1.0f);
    } 
    else if (lower.find("swamp") != std::string::npos || lower.find("mangrove") != std::string::npos) {
        grass   = mce::Color(0.22f, 0.28f, 0.16f, 1.0f);
        foliage = mce::Color(0.18f, 0.25f, 0.15f, 1.0f);
        water   = mce::Color(0.15f, 0.25f, 0.22f, 1.0f);
    } 
    else if (lower.find("taiga") != std::string::npos || lower.find("snow") != std::string::npos || lower.find("ice") != std::string::npos || lower.find("frozen") != std::string::npos) {
        grass   = mce::Color(0.26f, 0.38f, 0.30f, 1.0f);
        foliage = mce::Color(0.15f, 0.28f, 0.25f, 1.0f);
    } 
    else if (lower.find("birch") != std::string::npos) {
        grass   = mce::Color(0.36f, 0.48f, 0.25f, 1.0f);
        foliage = mce::Color(0.25f, 0.42f, 0.20f, 1.0f);
    }
    else if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos || lower.find("dark_oak_forest") != std::string::npos) {
        // foliage 匹配实际黑森林树叶颜色 RGB(60,120,30)——基于截图采样，
        // 最亮树叶主色调（像素数最多），原 (0.15,0.25,0.10) 过暗与实际不符
        grass   = mce::Color(0.20f, 0.30f, 0.12f, 1.0f);
        foliage = mce::Color(0.235f, 0.471f, 0.118f, 1.0f);
    }
    else if (lower.find("meadow") != std::string::npos || lower.find("grove") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.42f, 0.28f, 1.0f);
        foliage = mce::Color(0.18f, 0.32f, 0.20f, 1.0f);
    }
    // === 苍白之园：草地匹配实际颜色 RGB(86,97,79)——基于截图采样，去饱和灰绿色 ===
    else if (lower.find("pale_garden") != std::string::npos) {
        grass   = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
        foliage = mce::Color(0.337f, 0.380f, 0.310f, 1.0f);
    }
}

inline mce::Color getBlockColor(std::string const& name, mce::Color grassCol, mce::Color foliageCol, mce::Color waterCol) {
    if (name == "minecraft:air" || name == "air" || name.find("barrier") != std::string::npos ||
        name.find("light_block") != std::string::npos || name.find("structure_void") != std::string::npos ||
        name.find("placeholder") != std::string::npos || name.find("unknown") != std::string::npos ||
        name.find("info_update") != std::string::npos) {
        return mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (name.find("glass") != std::string::npos) return mce::Color(0.8f, 0.9f, 0.9f, 0.3f);
    if (name.find("path") != std::string::npos || name.find("farmland") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("bamboo") != std::string::npos) return mce::Color(0.40f, 0.70f, 0.20f, 1.0f);

    if (name.find("water") != std::string::npos) return waterCol;
    if (name.find("pink_petals") != std::string::npos) return mce::Color(0.95f, 0.68f, 0.78f, 1.0f);

    if (name.find("peony") != std::string::npos || name.find("pink_tulip") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (name.find("dandelion") != std::string::npos || name.find("sunflower") != std::string::npos || name.find("yellow_flower") != std::string::npos) return mce::Color(0.95f, 0.85f, 0.20f, 1.0f);
    if (name.find("rose") != std::string::npos || name.find("poppy") != std::string::npos || name.find("red_flower") != std::string::npos || name.find("red_tulip") != std::string::npos) return mce::Color(0.85f, 0.15f, 0.15f, 1.0f);
    if (name.find("orchid") != std::string::npos || name.find("cornflower") != std::string::npos) return mce::Color(0.20f, 0.40f, 0.85f, 1.0f);
    if (name.find("allium") != std::string::npos || name.find("lilac") != std::string::npos) return mce::Color(0.70f, 0.30f, 0.70f, 1.0f);
    if (name.find("daisy") != std::string::npos || name.find("bluet") != std::string::npos || name.find("valley") != std::string::npos || name.find("white_tulip") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (name.find("flower") != std::string::npos || name.find("bloom") != std::string::npos || name.find("blossom") != std::string::npos) return mce::Color(0.92f, 0.85f, 0.25f, 1.0f);

    if (name.find("white_") != std::string::npos) return mce::Color(0.95f, 0.95f, 0.95f, 1.0f);
    if (name.find("orange_") != std::string::npos) return mce::Color(0.85f, 0.50f, 0.20f, 1.0f);
    if (name.find("magenta_") != std::string::npos) return mce::Color(0.75f, 0.35f, 0.75f, 1.0f);
    if (name.find("light_blue_") != std::string::npos) return mce::Color(0.40f, 0.65f, 0.90f, 1.0f);
    if (name.find("yellow_") != std::string::npos) return mce::Color(0.90f, 0.85f, 0.20f, 1.0f);
    if (name.find("lime_") != std::string::npos) return mce::Color(0.45f, 0.85f, 0.20f, 1.0f);
    if (name.find("pink_") != std::string::npos) return mce::Color(0.90f, 0.55f, 0.70f, 1.0f);
    if (name.find("gray_") != std::string::npos) return mce::Color(0.40f, 0.40f, 0.40f, 1.0f);
    if (name.find("light_gray_") != std::string::npos || name.find("silver_") != std::string::npos) return mce::Color(0.65f, 0.65f, 0.65f, 1.0f);
    if (name.find("cyan_") != std::string::npos) return mce::Color(0.20f, 0.60f, 0.60f, 1.0f);
    if (name.find("purple_") != std::string::npos) return mce::Color(0.50f, 0.25f, 0.60f, 1.0f);
    if (name.find("blue_") != std::string::npos) return mce::Color(0.20f, 0.30f, 0.70f, 1.0f);
    if (name.find("brown_") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);
    if (name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.50f, 0.20f, 1.0f);
    if (name.find("red_") != std::string::npos) return mce::Color(0.75f, 0.20f, 0.20f, 1.0f);
    if (name.find("black_") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.15f, 1.0f);

    if (name.find("double_plant") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.25f, 1.0f);

    if (name.find("lily_pad") != std::string::npos || name.find("waterlily") != std::string::npos) return mce::Color(0.25f, 0.55f, 0.20f, 1.0f);

    if (name.find("mushroom") != std::string::npos || name.find("fungus") != std::string::npos || name.find("fungi") != std::string::npos) {
        if (name.find("red") != std::string::npos || name.find("crimson") != std::string::npos) return mce::Color(0.85f, 0.20f, 0.20f, 1.0f);
        if (name.find("brown") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
        if (name.find("warped") != std::string::npos) return mce::Color(0.15f, 0.55f, 0.50f, 1.0f);
        return mce::Color(0.80f, 0.70f, 0.60f, 1.0f);
    }

    if (name.find("warped_wart") != std::string::npos) return mce::Color(0.20f, 0.50f, 0.42f, 1.0f);
    if (name.find("wart") != std::string::npos) return mce::Color(0.65f, 0.10f, 0.10f, 1.0f);
    if (name.find("chorus") != std::string::npos) return mce::Color(0.60f, 0.40f, 0.60f, 1.0f);

    // [下界木] 必须在 grass 检查之前（"stem" 同时匹配作物茎和下界木，需优先处理下界木）
    if (name.find("crimson_stem") != std::string::npos || name.find("crimson_hyphae") != std::string::npos) return mce::Color(0.45f, 0.18f, 0.18f, 1.0f);
    if (name.find("warped_stem") != std::string::npos || name.find("warped_hyphae") != std::string::npos) return mce::Color(0.15f, 0.40f, 0.38f, 1.0f);

    if (name.find("grass") != std::string::npos || name.find("fern") != std::string::npos || name.find("moss") != std::string::npos ||
        name.find("shrub") != std::string::npos || name.find("plant") != std::string::npos || name.find("vine") != std::string::npos ||
        name.find("sapling") != std::string::npos || name.find("propagule") != std::string::npos || name.find("sugar_cane") != std::string::npos ||
        name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos || name.find("potato") != std::string::npos ||
        name.find("beetroot") != std::string::npos || name.find("crop") != std::string::npos || name.find("stem") != std::string::npos ||
        name.find("bush") != std::string::npos || name.find("seagrass") != std::string::npos || name.find("kelp") != std::string::npos ||
        name.find("lichen") != std::string::npos || name.find("seed") != std::string::npos) {
        return grassCol;
    }
    if (name.find("dirt") != std::string::npos || name.find("podzol") != std::string::npos) return mce::Color(0.45f, 0.30f, 0.15f, 1.0f);

    if (name.find("pumpkin") != std::string::npos || name.find("melon") != std::string::npos) {
        if (name.find("melon") != std::string::npos) return mce::Color(0.50f, 0.65f, 0.15f, 1.0f);
        return mce::Color(0.90f, 0.45f, 0.05f, 1.0f);
    }

    if (name.find("red_sand") != std::string::npos) return mce::Color(0.75f, 0.40f, 0.15f, 1.0f);

    if (name.find("end_stone") != std::string::npos) return mce::Color(0.86f, 0.89f, 0.65f, 1.0f);
    if (name.find("sandstone") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.60f, 1.0f);
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
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.18f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.06f, 0.04f, 0.09f, 1.0f);
    if (name.find("crimson_nylium") != std::string::npos) return mce::Color(0.55f, 0.15f, 0.15f, 1.0f);
    if (name.find("warped_nylium") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.40f, 1.0f);
    if (name.find("shroomlight") != std::string::npos) return mce::Color(1.0f, 0.60f, 0.20f, 1.0f);
    if (name.find("quartz_ore") != std::string::npos) return mce::Color(0.65f, 0.55f, 0.55f, 1.0f);
    if (name.find("nether_gold_ore") != std::string::npos) return mce::Color(0.65f, 0.35f, 0.15f, 1.0f);

    if (name.find("fallen") != std::string::npos || name.find("dead") != std::string::npos ||
        name.find("litter") != std::string::npos || name.find("brown") != std::string::npos) {
        if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos) {
            return mce::Color(0.55f, 0.38f, 0.20f, 1.0f);
        }
    }

    if (name.find("leaf") != std::string::npos || name.find("leaves") != std::string::npos || name.find("azalea") != std::string::npos) {
        if (name.find("cherry") != std::string::npos) return mce::Color(0.90f, 0.65f, 0.75f, 1.0f);
        if (name.find("mangrove") != std::string::npos) return mce::Color(0.15f, 0.30f, 0.10f, 1.0f);
        // [苍白橡树叶] 原版使用固定灰白色调 (不受生物群系着色)，呈现标志性"苍白"外观
        // 匹配实际树叶颜色 RGB(110,115,107)——基于截图采样，原 (0.75,0.75,0.70) 过亮与实际不符
        if (name.find("pale") != std::string::npos) return mce::Color(0.431f, 0.451f, 0.420f, 1.0f);
        return foliageCol;
    }

    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.95f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.65f, 0.85f, 0.95f, 1.0f);
    if (name.find("snow") != std::string::npos) return mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.18f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.30f, 0.22f, 0.14f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.85f, 0.80f, 0.60f, 1.0f);

    if (name.find("terracotta") != std::string::npos || name.find("hardened_clay") != std::string::npos) {
        if (name.find("white_") != std::string::npos) return mce::Color(0.82f, 0.70f, 0.63f, 1.0f);
        if (name.find("orange_") != std::string::npos) return mce::Color(0.63f, 0.33f, 0.14f, 1.0f);
        if (name.find("magenta_") != std::string::npos) return mce::Color(0.58f, 0.34f, 0.42f, 1.0f);
        if (name.find("light_blue_") != std::string::npos) return mce::Color(0.44f, 0.42f, 0.53f, 1.0f);
        if (name.find("yellow_") != std::string::npos) return mce::Color(0.73f, 0.52f, 0.11f, 1.0f);
        if (name.find("lime_") != std::string::npos) return mce::Color(0.41f, 0.46f, 0.20f, 1.0f);
        if (name.find("pink_") != std::string::npos) return mce::Color(0.63f, 0.30f, 0.30f, 1.0f);
        if (name.find("gray_") != std::string::npos) return mce::Color(0.22f, 0.16f, 0.15f, 1.0f);
        if (name.find("light_gray_") != std::string::npos || name.find("silver") != std::string::npos) return mce::Color(0.53f, 0.42f, 0.38f, 1.0f);
        if (name.find("cyan_") != std::string::npos) return mce::Color(0.34f, 0.35f, 0.35f, 1.0f);
        if (name.find("purple_") != std::string::npos) return mce::Color(0.46f, 0.28f, 0.33f, 1.0f);
        if (name.find("blue_") != std::string::npos) return mce::Color(0.29f, 0.23f, 0.35f, 1.0f);
        if (name.find("brown_") != std::string::npos) return mce::Color(0.30f, 0.20f, 0.13f, 1.0f);
        if (name.find("green_") != std::string::npos) return mce::Color(0.30f, 0.33f, 0.15f, 1.0f);
        if (name.find("red_") != std::string::npos) return mce::Color(0.56f, 0.24f, 0.18f, 1.0f);
        if (name.find("black_") != std::string::npos) return mce::Color(0.14f, 0.09f, 0.08f, 1.0f);
        return mce::Color(0.59f, 0.35f, 0.22f, 1.0f);
    }

    // [苍白橡木] 灰白色木材，区别于普通橡木的暖棕色
    if (name.find("pale_oak") != std::string::npos) return mce::Color(0.68f, 0.66f, 0.60f, 1.0f);
    // [深板岩] 冷调蓝灰色岩石, 必须在 wood/stairs/slab 检查之前,
    // 否则 deepslate_bricks_slab / deepslate_stairs 等变体会被误判为木头的暖棕色
    // 实际深板岩颜色约 RGB(100,100,110), 偏冷蓝灰
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.39f, 0.39f, 0.43f, 1.0f);
    if (name.find("planks") != std::string::npos || name.find("oak") != std::string::npos || name.find("spruce") != std::string::npos || name.find("birch") != std::string::npos || name.find("jungle") != std::string::npos || name.find("acacia") != std::string::npos || name.find("dark_oak") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos || name.find("stem") != std::string::npos || name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos || name.find("fence") != std::string::npos || name.find("door") != std::string::npos || name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos || name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    // [基岩] 极深灰色
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);
    // [矿石] 统一处理所有矿石方块，每种带特征矿物色调（redstone/nether_gold/quartz 已在上方处理）
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
    // [泥方块/泥砖] 深棕色（mud_bricks 含 "brick" 会匹配 stone 规则，需提前处理）
    if (name.find("mud") != std::string::npos) {
        if (name.find("brick") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.20f, 1.0f);
        return mce::Color(0.25f, 0.22f, 0.18f, 1.0f);
    }
    // [滴水石] 灰棕色（含 "stone" 但应偏棕色，需在 stone 检查前处理）
    // 匹配原版贴图平均色 RGB(134,108,93)，无生物群系着色
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    // [菌丝体] 灰紫色（蘑菇岛地表）
    if (name.find("mycelium") != std::string::npos) return mce::Color(0.48f, 0.42f, 0.42f, 1.0f);
    // [紫水晶] 淡紫色
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.58f, 0.48f, 0.68f, 1.0f);
    // [铜块] 橙棕色（铜矿石已在 ore 检查中处理）
    if (name.find("copper") != std::string::npos) return mce::Color(0.72f, 0.45f, 0.28f, 1.0f);
    // [粗矿块] 棕色调
    if (name.find("raw_") != std::string::npos) return mce::Color(0.50f, 0.38f, 0.25f, 1.0f);
    // [方解石] 原版为平滑的灰白/米白色方块（紫晶洞外壳），避免落入哈希默认色
    if (name.find("calcite") != std::string::npos) return mce::Color(0.86f, 0.86f, 0.82f, 1.0f);
    if (name.find("stone") != std::string::npos || name.find("cobble") != std::string::npos || name.find("andesite") != std::string::npos || name.find("diorite") != std::string::npos || name.find("granite") != std::string::npos || name.find("tuff") != std::string::npos || name.find("brick") != std::string::npos || name.find("wall") != std::string::npos || name.find("gravel") != std::string::npos || name.find("clay") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    unsigned int h = 0;
    for (char c : name) h = h * 31 + c;
    float r = 0.45f + ((h >> 16) & 0xFF) / 255.0f * 0.4f;
    float g = 0.45f + ((h >> 8) & 0xFF) / 255.0f * 0.4f;
    float b = 0.45f + (h & 0xFF) / 255.0f * 0.4f;
    return mce::Color(r, g, b, 1.0f);
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
    return name == "minecraft:air" || name == "air" ||
           name.find("barrier") != std::string::npos ||
           name.find("light_block") != std::string::npos ||
           name.find("structure_void") != std::string::npos ||
           name.find("placeholder") != std::string::npos ||
           name.find("info_update") != std::string::npos;
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
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        outName.clear();
        return false;
    }
}

// ==================== 洞穴地图系统 (Xaero's Cave Map 1:1 复刻) ====================

// [洞穴辅助] 判断方块是否为"覆盖层"（透明/非实心），在洞穴列扫描中跳过
// 对应 Xaero's MapWriter.isInvisible: air, liquid, glass, torch, grass, flowers, leaves
inline bool IsCaveOverlayBlockName(std::string const& name) noexcept {
    if (IsAirLikeName(name)) return true;
    if (IsLiquidBlockName(name)) return true;  // 液体在洞穴扫描中视为可穿透层
    if (name.find("glass") != std::string::npos) return true;
    if (name.find("torch") != std::string::npos) return true;
    if (name.find("short_grass") != std::string::npos || name.find("tallgrass") != std::string::npos) return true;
    if (name.find("flower") != std::string::npos || name.find("bush") != std::string::npos) return true;
    if (name.find("leaves") != std::string::npos) return true;
    if (name.find("vine") != std::string::npos) return true;
    if (name.find("sapling") != std::string::npos) return true;
    if (name.find("mushroom") != std::string::npos) return true;
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
    if (name.find("wheat") != std::string::npos || name.find("carrot") != std::string::npos ||
        name.find("potato") != std::string::npos || name.find("beetroot") != std::string::npos ||
        name.find("crop") != std::string::npos) return true;
    return false;
}

// [洞穴检测] 检测玩家是否在地下洞穴中
// 对应 Xaero's CaveStartCalculator.getCaving: 检查玩家头顶是否有实心方块遮挡
// 返回值: true=在洞穴中, caveStartY=洞穴起始Y(地表高度)
inline bool DetectCaveStart(BlockSource& region, int playerX, int playerY, int playerZ, int& outCaveStartY) noexcept {
    // 获取地表高度
    short surfaceY = SafeGetSurfaceY(region, playerX, playerZ);
    if (surfaceY <= -64) return false;  // 无效地表

    // [树下防误判] getAboveTopSolidBlock 可能返回树叶 Y, 需跳过树叶等覆盖层找到真实地表
    // 向下扫描跳过树叶/透明方块/植被/液体, 找到第一个实心方块作为真实地表
    for (int y = (int)surfaceY - 1; y > playerY; y--) {
        std::string name;
        if (!SafeGetBlockName(region, playerX, y, playerZ, name)) continue;
        if (!IsAirLikeName(name) && !IsCaveOverlayBlockName(name) && !IsLiquidBlockName(name)) {
            surfaceY = (short)(y + 1);
            break;
        }
    }

    // 玩家头顶有 5+ 格实心方块 → 在洞穴中
    // getAboveTopSolidBlock 返回的是"最高的非空气方块 Y+1"
    // 如果 surfaceY > playerY + 5，说明玩家在地下
    if (surfaceY > playerY + 5) {
        outCaveStartY = (int)surfaceY;
        return true;
    }

    // 额外检查: 玩家头顶方块是否为实心（防止在屋檐下误判）
    // 检查玩家头顶 3-8 格是否有连续实心方块
    int solidCount = 0;
    for (int y = playerY + 2; y <= playerY + 8 && y < 320; y++) {
        std::string name;
        if (SafeGetBlockName(region, playerX, y, playerZ, name)) {
            if (!IsAirLikeName(name) && !IsCaveOverlayBlockName(name) &&
                !IsLiquidBlockName(name)) {
                solidCount++;
                if (solidCount >= 3) {
                    outCaveStartY = (int)surfaceY;
                    return true;
                }
            } else {
                solidCount = 0;
            }
        }
    }

    return false;
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
// 输出: outBlockName(方块名), outBlockY(方块Y), outDepth(深度)
// 返回值: true=找到洞穴方块, false=列内无洞穴 (纯实心石头, 渲染为透明)
inline bool ScanColumnCave(BlockSource& region, int x, int z, int startY, int caveDepth,
                           std::string& outBlockName, int& outBlockY, int& outDepth) noexcept {
    int bottomY = startY - caveDepth;
    if (bottomY < -64) bottomY = -64;

    bool seenAir = false;  // 是否已遇到空气 (洞穴空腔)
    int depth = 0;
    for (int y = startY; y >= bottomY; y--, depth++) {
        std::string name;
        if (!SafeGetBlockName(region, x, y, z, name)) continue;

        // 空气: 标记已进入洞穴空腔
        if (IsAirLikeName(name)) {
            seenAir = true;
            continue;
        }

        // 液体方块: 仅在空气下方返回 (熔岩湖/水池表面)
        if (IsLiquidBlockName(name)) {
            if (seenAir) {
                outBlockName = name;
                outBlockY = y;
                outDepth = depth;
                return true;
            }
            continue;  // 液体上方无空气, 跳过
        }

        // 其他覆盖层方块 (玻璃/火把/草/叶子等): 视为空气 (可穿透)
        if (IsCaveOverlayBlockName(name)) {
            seenAir = true;
            continue;
        }

        // 实心方块: 仅在空气下方返回 (洞穴地板/墙壁)
        if (seenAir) {
            outBlockName = name;
            outBlockY = y;
            outDepth = depth;
            return true;
        }
        // 实心方块上方无空气 = 纯石头区域, 跳过 (不渲染)
    }
    return false;  // 列内无洞穴空腔 = 透明 (黑色背景)
}

// [洞穴特殊方块颜色] 液体方块在洞穴中使用饱和颜色 (不受深度衰减)
// 对应 Xaero's MapPixel: 液体通过 fluidToBlock 转换, 颜色保持饱和
inline mce::Color GetCaveLiquidColor(std::string const& name) noexcept {
    if (name.find("lava") != std::string::npos) return mce::Color(1.0f, 0.40f, 0.05f, 1.0f);
    if (name.find("water") != std::string::npos) return mce::Color(0.15f, 0.45f, 0.90f, 1.0f);
    return mce::Color(0, 0, 0, 0);  // 非液体
}

// [洞穴方块专用颜色] 比通用 getBlockColor 更精细, 识别洞穴常见方块并赋予准确颜色
// 使洞穴地图色彩丰富: 矿石有对应颜色, 石头变种有区分, 深板岩为冷蓝灰
inline mce::Color GetCaveBlockColor(std::string const& name) noexcept {
    // 基岩 (世界底部)
    if (name.find("bedrock") != std::string::npos) return mce::Color(0.12f, 0.12f, 0.14f, 1.0f);
    // 深板岩类 (冷蓝灰, 匹配实际 RGB(100,100,110))
    if (name.find("deepslate") != std::string::npos) return mce::Color(0.39f, 0.39f, 0.43f, 1.0f);
    // 矿石 (对应矿物颜色, 增加视觉信息)
    if (name.find("diamond") != std::string::npos) return mce::Color(0.40f, 0.85f, 0.85f, 1.0f);
    if (name.find("gold") != std::string::npos) return mce::Color(0.95f, 0.82f, 0.25f, 1.0f);
    if (name.find("iron") != std::string::npos) return mce::Color(0.78f, 0.58f, 0.42f, 1.0f);
    if (name.find("coal") != std::string::npos) return mce::Color(0.18f, 0.18f, 0.18f, 1.0f);
    if (name.find("redstone") != std::string::npos) return mce::Color(0.78f, 0.15f, 0.15f, 1.0f);
    if (name.find("lapis") != std::string::npos) return mce::Color(0.20f, 0.42f, 0.88f, 1.0f);
    if (name.find("emerald") != std::string::npos) return mce::Color(0.20f, 0.80f, 0.38f, 1.0f);
    if (name.find("copper") != std::string::npos) return mce::Color(0.78f, 0.52f, 0.35f, 1.0f);
    if (name.find("netherite") != std::string::npos) return mce::Color(0.35f, 0.30f, 0.28f, 1.0f);
    if (name.find("quartz") != std::string::npos) return mce::Color(0.88f, 0.85f, 0.78f, 1.0f);
    // 石头变种 (暖→冷渐变区分)
    if (name.find("granite") != std::string::npos) return mce::Color(0.58f, 0.42f, 0.35f, 1.0f);
    if (name.find("diorite") != std::string::npos) return mce::Color(0.72f, 0.70f, 0.68f, 1.0f);
    if (name.find("andesite") != std::string::npos) return mce::Color(0.50f, 0.48f, 0.46f, 1.0f);
    if (name.find("tuff") != std::string::npos) return mce::Color(0.45f, 0.43f, 0.41f, 1.0f);
    if (name.find("calcite") != std::string::npos) return mce::Color(0.82f, 0.80f, 0.78f, 1.0f);
    if (name.find("dripstone") != std::string::npos) return mce::Color(0.525f, 0.424f, 0.365f, 1.0f);
    if (name.find("amethyst") != std::string::npos) return mce::Color(0.62f, 0.42f, 0.82f, 1.0f);
    // 泥土/沙石 (soul_sand/soul_soil 必须在 sand 之前, 因 "soul_sand" 含子串 "sand")
    if (name.find("soul_sand") != std::string::npos) return mce::Color(0.35f, 0.28f, 0.18f, 1.0f);
    if (name.find("soul_soil") != std::string::npos) return mce::Color(0.30f, 0.22f, 0.14f, 1.0f);
    if (name.find("dirt") != std::string::npos) return mce::Color(0.48f, 0.33f, 0.18f, 1.0f);
    if (name.find("sand") != std::string::npos) return mce::Color(0.78f, 0.72f, 0.52f, 1.0f);
    if (name.find("gravel") != std::string::npos) return mce::Color(0.52f, 0.47f, 0.44f, 1.0f);
    if (name.find("clay") != std::string::npos) return mce::Color(0.55f, 0.58f, 0.68f, 1.0f);
    // 苔藓/发光苔藓 (洞穴植被)
    if (name.find("moss") != std::string::npos) return mce::Color(0.35f, 0.55f, 0.30f, 1.0f);
    if (name.find("glow") != std::string::npos && name.find("berry") != std::string::npos) return mce::Color(0.75f, 0.45f, 0.20f, 1.0f);
    if (name.find("spore") != std::string::npos) return mce::Color(0.65f, 0.55f, 0.75f, 1.0f);
    if (name.find("rooted") != std::string::npos && name.find("dirt") != std::string::npos) return mce::Color(0.38f, 0.28f, 0.18f, 1.0f);
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
    if (name.find("ancient_debris") != std::string::npos) return mce::Color(0.55f, 0.35f, 0.20f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.25f, 0.22f, 0.22f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.20f, 0.18f, 0.20f, 1.0f);
    if (name.find("obsidian") != std::string::npos) return mce::Color(0.12f, 0.08f, 0.18f, 1.0f);
    // 木材/木板 (废弃矿井支撑)
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
// 防止部分加载区块返回临时错误 Y（地下结构等）
inline bool IsChunkReady(BlockSource& region, int x, int y, int z) noexcept {
    try {
        // hasChunksAt(pos, r, ignoreClientChunk): 检查 pos 周围半径 r 内的区块是否已加载
        // r=0 仅检查目标区块本身；false 表示不忽略客户端区块（更严格）
        return region.hasChunksAt(BlockPos(x, y, z), 0, false);
    } catch (...) {
        return false;
    }
}

// [v2 新增·辅助] 判断方块名是否为"接触即受伤/致命"的危险方块
// 用途：在落脚点选择时排除岩浆池、火、仙人掌、岩浆块、营火、甜浆果丛等
// 与 IsLiquidBlockName 互补：岩浆同时属于两者（液体+危险），其他如火/仙人掌仅属于危险
inline bool IsDangerousBlockName(std::string const& name) noexcept {
    return name.find("lava") != std::string::npos ||          // 岩浆（液体+致命）
           name.find("fire") != std::string::npos ||          // 火（普通火/灵魂火）
           name.find("cactus") != std::string::npos ||        // 仙人掌（接触伤害）
           name.find("magma") != std::string::npos ||         // 岩浆块（接触伤害）
           name.find("campfire") != std::string::npos ||      // 营火（含灵魂营火）
           name.find("sweet_berry") != std::string::npos ||   // 甜浆果丛（接触伤害）
           name.find("wither_rose") != std::string::npos ||   // 凋灵玫瑰（凋灵效果）
           name.find("pointed_dripstone") != std::string::npos; // 钟乳石（坠落伤害）
}

// [v2 新增·辅助] 判断方块名是否为"可站立固体"（玩家脚部下方应为此类）
// 排除：空气类、液体类、危险类 → 剩余视为可站立
inline bool IsStandableSolidName(std::string const& name) noexcept {
    return !IsAirLikeName(name) &&
           !IsLiquidBlockName(name) &&
           !IsDangerousBlockName(name);
}

// [v2 新增·辅助] 检查目标落脚点 (x, y, z) 周围 3x3 范围（同高度层）是否有液体/危险方块
// 用途：避免传送到"本列安全但紧邻水/岩浆"的位置（玩家可能被冲入危险区）
// 返回值：true=有邻居危险，false=邻居安全
inline bool HasAdjacentHazard(BlockSource& region, int x, int y, int z, int radius = 1) noexcept {
    try {
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dz = -radius; dz <= radius; ++dz) {
                if (dx == 0 && dz == 0) continue;  // 跳过中心（本列已验证）
                Block const& b = region.getBlock(BlockPos(x + dx, y, z + dz));
                std::string name = b.getTypeName();
                if (IsLiquidBlockName(name) || IsDangerousBlockName(name)) return true;
            }
        }
        return false;
    } catch (...) {
        return true;  // 异常视为有危险（保守策略）
    }
}

// [防线②③·v2 重写] 安全落脚点查找：给定 (x,z)，返回玩家可安全站立的实际 Y 坐标
//
// v1 bug 修复：
//   ✗ 旧版场景 a 返回"水面上方空气 Y"，注释"游泳姿态可接受"——完全错误！
//     /tp 传送后玩家受重力影响，水面上方空气 Y 会让玩家下落穿过水，最终落到水底
//   ✗ 旧版场景 b/c 接受"头部是液体"，玩家会头部浸水
//   ✗ 旧版无脚部下方固体验证，部分加载错误 Y 可导致传送到虚空
//   ✗ 旧版无危险方块检测，可能传送到岩浆/火/仙人掌上
//
// v2 设计原则：
//   1. 液体列（脚部是水/岩浆）→ 直接拒绝，强制 FindNearestSafeSpawn 搜索附近陆地
//   2. 头部必须是空气类（拒绝液体/固体），保证玩家有呼吸空间
//   3. 返回前验证：Y-1 是可站立固体，Y 是空气类，Y+1 是空气类
//   4. 危险方块（岩浆/火/仙人掌等）按液体列处理，直接拒绝
//
// 返回值：安全 Y（玩家脚部位置），或 -32000 表示该列无安全落脚点
inline short SafeFindSafeSpawnY(BlockSource& region, int x, int z) noexcept {
    short surfaceY = SafeGetSurfaceY(region, x, z);
    if (surfaceY <= -64 || surfaceY >= 319) return -32000;

    try {
        // 检查 surfaceY+1（玩家脚部位置）的方块类型
        Block const& feetBlock = region.getBlock(BlockPos(x, surfaceY + 1, z));
        std::string feetName = feetBlock.getTypeName();

        // 场景 a：脚部是液体/危险方块 → 拒绝该列（水/岩浆池/熔岩湖）
        // v2 关键修复：不再尝试水面上方空气，因为玩家会因重力落入水中
        // 强制 FindNearestSafeSpawn 搜索附近陆地列
        if (IsLiquidBlockName(feetName) || IsDangerousBlockName(feetName)) {
            return -32000;
        }

        // 场景 c：脚部是空气 → 正常地表，验证头部空间
        if (IsAirLikeName(feetName)) {
            Block const& headBlock = region.getBlock(BlockPos(x, surfaceY + 2, z));
            std::string headName = headBlock.getTypeName();
            // v2 修复：头部必须是空气类（移除"头部是液体也接受"的错误逻辑）
            if (IsAirLikeName(headName)) {
                // v2 新增：返回前验证脚部下方是可站立固体（防止部分加载错误 Y）
                Block const& groundBlock = region.getBlock(BlockPos(x, surfaceY, z));
                std::string groundName = groundBlock.getTypeName();
                if (IsStandableSolidName(groundName)) {
                    return surfaceY + 1;
                }
                // 下方不是固体 → 可能是部分加载的错误 Y，拒绝
                return -32000;
            }
            // 头部是固体（无头部空间）→ 向上扫描找 air+air 两格空间
            for (short y = surfaceY + 3; y < 319; ++y) {
                Block const& b = region.getBlock(BlockPos(x, y, z));
                std::string name = b.getTypeName();
                Block const& hb = region.getBlock(BlockPos(x, y + 1, z));
                std::string hn = hb.getTypeName();
                // v2 修复：头部必须是空气类（移除液体接受条件）
                // 同时验证 y-1 是可站立固体
                if (IsAirLikeName(name) && IsAirLikeName(hn)) {
                    Block const& belowBlock = region.getBlock(BlockPos(x, y - 1, z));
                    std::string belowName = belowBlock.getTypeName();
                    if (IsStandableSolidName(belowName)) {
                        return y;
                    }
                    // y-1 不是固体 → 继续向上扫描
                }
            }
            return -32000;
        }

        // 场景 b：脚部是固体（结构内/树干内）→ 向上扫描找第一个 air+air 空间
        // 注意：此处脚部是固体但不是危险方块（危险方块已在场景 a 拒绝）
        for (short y = surfaceY + 2; y < 319; ++y) {
            Block const& b = region.getBlock(BlockPos(x, y, z));
            std::string name = b.getTypeName();
            Block const& hb = region.getBlock(BlockPos(x, y + 1, z));
            std::string hn = hb.getTypeName();
            // v2 修复：头部必须是空气类（移除液体接受条件）
            if (IsAirLikeName(name) && IsAirLikeName(hn)) {
                // v2 新增：验证 y-1 是可站立固体（排除平台边缘/悬空结构）
                Block const& belowBlock = region.getBlock(BlockPos(x, y - 1, z));
                std::string belowName = belowBlock.getTypeName();
                if (IsStandableSolidName(belowName)) {
                    return y;
                }
                // y-1 不是固体 → 继续向上扫描
            }
        }
        return -32000;
    } catch (...) {
        // 部分加载区块触发 AV → 视为无安全落脚点
        return -32000;
    }
}

// [防线⑤·v2 重写] 附近安全点搜索：目标列无安全落脚点时，螺旋搜索附近列
//
// v2 改进：
//   1. 搜索半径 ±8 → ±16（在小岛/悬崖/湖泊边缘场景提供更多机会）
//   2. 双轮搜索策略：
//      - 第一轮（严格）：要求落脚点 3x3 邻居无液体/危险方块（避免紧邻水/岩浆）
//      - 第二轮（宽松）：仅要求本列安全（接受被水包围但本列干燥的"孤岛"落脚点）
//   3. 搜索顺序仍为螺旋：(0,0) → (±1,0),(0,±1) → ... → (±16,±16)
//
// v3 改进（避免传送失败）：
//   4. 第三轮（超大范围宽松）：±17~±32 搜索，仅在第一二轮都失败时启用
//      适用场景：目标点在巨大湖泊/岩浆湖中央，±16 范围仍在水中
//      性能：±32 = 65×65 = 4225 次查询，约 4ms，可接受
//
// 找到后修改 x/z/outY 为安全点坐标，返回 true；未找到返回 false
inline bool FindNearestSafeSpawn(BlockSource& region, int& x, int& z, short& outY, int maxRadius = 16) noexcept {
    // 第一轮：严格搜索（3x3 邻居无危险），半径 ±maxRadius
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;  // 只搜索半径 r 的环
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ);
                if (testY > -64 && testY < 319) {
                    // 严格检查：落脚点 3x3 邻居无液体/危险方块
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1)) {
                        x = testX;
                        z = testZ;
                        outY = testY;
                        return true;
                    }
                }
            }
        }
    }

    // 第二轮：宽松搜索（仅本列安全，接受孤岛落脚点），半径 ±maxRadius
    // 适用场景：目标点在湖泊中央小岛，本列安全但邻居都是水
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ);
                if (testY > -64 && testY < 319) {
                    x = testX;
                    z = testZ;
                    outY = testY;
                    return true;
                }
            }
        }
    }

    // 第三轮：超大范围宽松搜索（v3 新增，避免传送失败），半径 ±(maxRadius+1)~±32
    // 适用场景：目标点在巨大湖泊/岩浆湖中央，±16 范围仍全是水
    // 性能考量：±32 = 65×65 = 4225 次 getBlock，约 4ms，单次传送可接受
    for (int r = maxRadius + 1; r <= 32; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnY(region, testX, testZ);
                if (testY > -64 && testY < 319) {
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

// [洞穴/下界传送·安全落脚点查找] 给定 (x,z) 和参考 Y, 在 [minY, maxY] 范围内 refY 附近搜索可安全站立的 Y
// 用于洞穴/下界传送: 不从地表开始, 而是从玩家当前高度向上下扫描, 找到 air+air 且下方为固体的位置
// 避免传送到岩浆/方块内/虚空中
// minY/maxY 限制扫描范围: 下界 [2,125] 避开基岩层; 洞穴 [refY-48, refY+48] 避免穿过天花板到地表
inline short SafeFindSafeSpawnYNearY(BlockSource& region, int x, int z, int refY, int minY = -64, int maxY = 319) noexcept {
    if (refY < minY) refY = minY;
    if (refY > maxY) refY = maxY;
    try {
        // 先从 refY 向上扫描 (优先传送到同高度或略高), 上限 maxY (不穿过天花板/基岩顶)
        for (int y = refY; y <= maxY; ++y) {
            Block const& b = region.getBlock(BlockPos(x, y, z));
            std::string name = b.getTypeName();
            if (IsAirLikeName(name)) {
                if (y + 1 > maxY) break;  // 头部超出范围
                Block const& hb = region.getBlock(BlockPos(x, y + 1, z));
                std::string hn = hb.getTypeName();
                if (IsAirLikeName(hn)) {
                    if (y - 1 < minY) continue;  // 脚下超出范围
                    Block const& belowBlock = region.getBlock(BlockPos(x, y - 1, z));
                    std::string belowName = belowBlock.getTypeName();
                    if (IsStandableSolidName(belowName)) {
                        return (short)y;
                    }
                }
            }
        }
        // 再从 refY-1 向下扫描, 下限 minY
        for (int y = refY - 1; y >= minY; --y) {
            Block const& b = region.getBlock(BlockPos(x, y, z));
            std::string name = b.getTypeName();
            if (IsAirLikeName(name)) {
                if (y + 1 > maxY) continue;
                Block const& hb = region.getBlock(BlockPos(x, y + 1, z));
                std::string hn = hb.getTypeName();
                if (IsAirLikeName(hn)) {
                    if (y - 1 < minY) continue;
                    Block const& belowBlock = region.getBlock(BlockPos(x, y - 1, z));
                    std::string belowName = belowBlock.getTypeName();
                    if (IsStandableSolidName(belowName)) {
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
// 双轮: 严格 (3x3 邻居无危险) → 宽松 (仅本列安全)
inline bool FindNearestSafeSpawnNearY(BlockSource& region, int& x, int& z, short& outY, int refY, int maxRadius = 16, int minY = -64, int maxY = 319) noexcept {
    // 第一轮: 严格 (邻居无危险)
    for (int r = 0; r <= maxRadius; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int testX = x + dx;
                int testZ = z + dz;
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY);
                if (testY > -64 && testY < 319) {
                    if (!HasAdjacentHazard(region, testX, testY, testZ, 1)) {
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
                short testY = SafeFindSafeSpawnYNearY(region, testX, testZ, refY, minY, maxY);
                if (testY > -64 && testY < 319) {
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
            const Vec3 pos = player->getFeetPos();

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

        // ==========================================
        // [地表直达传送·增强版] 三级地表Y识别 + 两阶段探测 + 五道安全防线
        // 需求：1.地表检测 2.未渲染区域 3.禁用Y=320/缓降 4.无缝 5.边界安全 6.日志
        //      7.区块就绪检查 8.水面/地下验证 9.附近点回退 10.加载状态反馈
        // ==========================================
        if (MapRenderState::triggerTeleport.load()) {
            float targetX = MapRenderState::tpTargetX;
            float targetY = MapRenderState::tpTargetY;
            float targetZ = MapRenderState::tpTargetZ;
            std::string detectMethod = "user-specified";

            // targetY < -500（哨兵）或 == 320（未加载默认值）→ 系统决定地表
            bool needSurfaceDetect = (targetY < -500.0f) || (std::abs(targetY - 320.0f) < 0.1f);

            if (needSurfaceDetect) {
                int blockX = (int)std::floor(targetX);
                int blockZ = (int)std::floor(targetZ);
                BlockSource* region = this->getRegion();

                // [维度感知] 确定传送模式
                // 0=地表(主世界非洞穴), 1=洞穴(主世界洞穴/下界), 2=末地
                int teleportMode = 0;
                int dimId = MapRenderState::currentDimensionId;
                if (dimId == 1) {
                    teleportMode = 1;  // 下界: 洞穴模式传送
                } else if (dimId == 2) {
                    teleportMode = 2;  // 末地: 避免虚空
                } else if (MapRenderState::g_caveModeActive) {
                    teleportMode = 1;  // 主世界洞穴: 洞穴模式传送
                }
                int refY = (int)g_playerY;  // 洞穴/末地传送的参考Y (玩家当前高度)
                if (dimId == 1) {
                    refY = 125;  // 下界: 从顶部基岩下方开始往下搜索落脚点
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
                    chunkReady = IsChunkReady(*region, blockX, teleportMode == 0 ? 64 : refY, blockZ);
                }

                bool needProbe = false;

                if (teleportMode == 0) {
                    // === 地表传送: 缓存/实时高度 → SafeFindSafeSpawnY → FindNearestSafeSpawn ===
                    int16_t surfaceY = MapCacheManager::HEIGHT_UNKNOWN;

                    // [优先级1] 缓存高度图：可识别已扫描但已卸载的区域
                    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ);
                    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64) {
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

                    if (surfaceY != MapCacheManager::HEIGHT_UNKNOWN && surfaceY > -64 && region && chunkReady) {
                        // [防线②③] 命中缓存/实时 → 用 SafeFindSafeSpawnY 验证落脚点
                        short safeY = SafeFindSafeSpawnY(*region, blockX, blockZ);
                        if (safeY > -64 && safeY < 319) {
                            // 落脚点验证通过 → 直接传送
                            float finalY = (float)safeY;
                            LogTeleport("tp instant (" + std::to_string(blockX) + "," +
                                        std::to_string((int)finalY) + "," + std::to_string(blockZ) +
                                        ") method=" + detectMethod + "/safe-spawn");
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)blockX + 0.5f, finalY, (float)blockZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Done);
                        } else {
                            // [防线⑤·v2] 目标列无安全落脚点 → 螺旋搜索附近 ±16 格
                            int searchX = blockX, searchZ = blockZ;
                            short nearbyY = -32000;
                            if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16)) {
                                LogTeleport("tp nearby-fallback (" + std::to_string(searchX) + "," +
                                            std::to_string((int)nearbyY) + "," + std::to_string(searchZ) +
                                            ") [original (" + std::to_string(blockX) + "," +
                                            std::to_string(blockZ) + ") unsafe, searched ±16]");
                                char coordBuf[128];
                                std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                              (float)searchX + 0.5f, (float)nearbyY, (float)searchZ + 0.5f);
                                SendServerCommand(*player, coordBuf);
                                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Done);
                            } else {
                                // 附近 ±16 格均无安全点 → 退回两阶段探测
                                LogTeleport("tp no-safe-nearby (" + std::to_string(blockX) + "," +
                                            std::to_string(blockZ) + ") → fallback to probe");
                                needProbe = true;
                            }
                        }
                    } else {
                        // 区块未就绪或无缓存 → 两阶段探测
                        needProbe = true;
                    }
                } else {
                    // === 洞穴/下界/末地传送: 在玩家当前Y附近搜索安全落脚点 ===
                    // 不使用地表Y, 而是用 SafeFindSafeSpawnYNearY 在 refY 上下扫描 (范围 tpMinY..tpMaxY)
                    // 避免传送到岩浆/方块内/虚空中
                    if (teleportMode == 1) {
                        // [洞穴/下界] 总是先传送到安全高度再往下探测, 不走即时传送
                        // 下界: Phase 0 Y=128 (基岩顶上), 从 Y=125 往下搜
                        // 洞穴: Phase 0 Y=320 (地表之上), 从 playerY±48 搜
                        // 避免部分加载区块导致 SafeFindSafeSpawnYNearY 返回错误Y卡在方块里
                        needProbe = true;
                    } else if (region && chunkReady) {
                        int searchX = blockX, searchZ = blockZ;
                        short safeY = -32000;
                        if (FindNearestSafeSpawnNearY(*region, searchX, searchZ, safeY, refY, 16, tpMinY, tpMaxY)) {
                            // 找到安全落脚点 → 直接传送
                            LogTeleport("tp instant-cave (" + std::to_string(searchX) + "," +
                                        std::to_string((int)safeY) + "," + std::to_string(searchZ) +
                                        ") mode=" + std::to_string(teleportMode) + " refY=" + std::to_string(refY));
                            char coordBuf[128];
                            std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                          (float)searchX + 0.5f, (float)safeY, (float)searchZ + 0.5f);
                            SendServerCommand(*player, coordBuf);
                            MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Done);
                        } else {
                            // 未找到安全点 → 两阶段探测
                            LogTeleport("tp no-safe-cave (" + std::to_string(blockX) + "," +
                                        std::to_string(blockZ) + ") mode=" + std::to_string(teleportMode) +
                                        " → fallback to probe");
                            needProbe = true;
                        }
                    } else {
                        // 区块未就绪 → 两阶段探测
                        needProbe = true;
                    }
                }

                if (needProbe) {
                    // [两阶段探测] Phase 0 tp 触发区块加载, Phase 1 轮询找安全Y
                    MapRenderState::probeMode = teleportMode;
                    MapRenderState::probeMinY = tpMinY;
                    MapRenderState::probeMaxY = tpMaxY;
                    MapRenderState::probeRefY = refY;
                    MapRenderState::probeOriginalX = g_playerX;
                    MapRenderState::probeOriginalY = g_playerY;
                    MapRenderState::probeOriginalZ = g_playerZ;
                    MapRenderState::probeTargetX.store(blockX);
                    MapRenderState::probeTargetZ.store(blockZ);
                    MapRenderState::probeStartTime = std::chrono::steady_clock::now();
                    MapRenderState::probeLastY = -32000;
                    MapRenderState::probeStableCount = 0;
                    MapRenderState::pendingSurfaceProbe.store(true);
                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Loading);
                    MapRenderState::teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
                    MapRenderState::teleportFailReason.clear();

                    // Phase 0 探测传送：先传送到安全高度触发区块加载
                    // 下界: Y=128 (基岩顶层之上, 玩家安全等待区块加载)
                    // 其他维度: Y=320 (地表之上)
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
                MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Done);
            }

            MapRenderState::triggerTeleport.store(false);
        }

        // ==========================================
        // [两阶段探测·Phase 1 轮询·v2 增强版] 等待目标区块加载后传送到地表
        // 五道安全防线（v2 参数）：
        //   ① 500ms 宽限期（v1: 300ms，给慢网络更多时间）
        //   ② hasChunksAt 区块就绪验证（防止部分加载返回错误 Y）
        //   ③ 稳定性检查：连续 3 帧返回相同 Y 才接受（v1: 2 帧，进一步过滤）
        //   ④ SafeFindSafeSpawnY 落脚点验证（v2: 拒绝液体/危险方块，验证下方固体）
        //   ⑤ FindNearestSafeSpawn 附近点回退（v2: ±16，双轮严格/宽松搜索）
        // 超时：4 秒（v1: 2 秒，给慢网络更多机会）
        // v2 关键改进：删除"接受原始 Y+1"危险兜底，改为回退原位 + 失败状态
        // ==========================================
        if (MapRenderState::pendingSurfaceProbe.load()) {
            int probeX = MapRenderState::probeTargetX.load();
            int probeZ = MapRenderState::probeTargetZ.load();
            bool probeDone = false;

            auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - MapRenderState::probeStartTime).count();

            // [防线①] Phase 0 tp 后 500ms 宽限期（原 300ms）
            // 在此之前区块必然未就绪，跳过以减少无谓查询
            if (elapsedMs >= 500) {
                BlockSource* region = this->getRegion();
                if (region) {
                    // [防线②] hasChunksAt 验证区块已完全加载
                    int probeMode = MapRenderState::probeMode;
                    int checkY = (probeMode == 0) ? 64 : MapRenderState::probeRefY;
                    if (IsChunkReady(*region, probeX, checkY, probeZ)) {
                        if (probeMode != 0) {
                            // [洞穴/下界/末地·Phase 1] 稳定性检查 + 安全落脚点搜索
                            // 下界 probeRefY=125 (从基岩顶层往下搜); 洞穴/末地 probeRefY=玩家原始Y
                            int refY = MapRenderState::probeRefY;
                            // [稳定性检查] 目标列安全Y 3帧一致才认为区块数据稳定
                            // 防止部分加载区块返回临时错误Y导致传送到方块内
                            short liveY = SafeFindSafeSpawnYNearY(*region, probeX, probeZ, refY, MapRenderState::probeMinY, MapRenderState::probeMaxY);
                            if (liveY == MapRenderState::probeLastY) {
                                MapRenderState::probeStableCount++;
                            } else {
                                MapRenderState::probeLastY = liveY;
                                MapRenderState::probeStableCount = 1;
                            }

                            if (MapRenderState::probeStableCount >= MapRenderState::kProbeStableThreshold) {
                                // 区块稳定 → 螺旋搜索安全落脚点
                                int searchX = probeX, searchZ = probeZ;
                                short safeY = -32000;
                                if (FindNearestSafeSpawnNearY(*region, searchX, searchZ, safeY, refY, 16, MapRenderState::probeMinY, MapRenderState::probeMaxY)) {
                                    // [二次验证] 再次读取目标位置方块, 确保确实是 air+air+solid
                                    // 防止附近列(±16)区块数据不准确导致传送到方块内
                                    bool verified = false;
                                    try {
                                        Block const& vb = region->getBlock(BlockPos(searchX, safeY, searchZ));
                                        Block const& va = region->getBlock(BlockPos(searchX, safeY + 1, searchZ));
                                        Block const& vd = region->getBlock(BlockPos(searchX, safeY - 1, searchZ));
                                        if (IsAirLikeName(vb.getTypeName()) && IsAirLikeName(va.getTypeName()) && IsStandableSolidName(vd.getTypeName())) {
                                            verified = true;
                                        }
                                    } catch (...) {
                                        verified = false;
                                    }

                                    if (verified) {
                                        // 验证通过 → 第二次传送
                                        float finalY = (float)safeY;
                                        char coordBuf[128];
                                        std::snprintf(coordBuf, sizeof(coordBuf), "/tp @s %.2f %.2f %.2f",
                                                      (float)searchX + 0.5f, finalY, (float)searchZ + 0.5f);
                                        SendServerCommand(*player, coordBuf);
                                        LogTeleport("probe SUCCESS-cave (" + std::to_string(searchX) + "," +
                                                    std::to_string((int)finalY) + "," + std::to_string(searchZ) +
                                                    ") [mode=" + std::to_string(probeMode) + " refY=" + std::to_string(refY) +
                                                            ", verified safe spawn]");
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Done);
                                        probeDone = true;
                                    } else {
                                        // 验证失败 → 拦截传送, 避免卡在方块里
                                        LogTeleport("probe VERIFY-FAIL (" + std::to_string(searchX) + "," +
                                                    std::to_string((int)safeY) + "," + std::to_string(searchZ) +
                                                    ") [block data inconsistent, intercept]");
                                        char abortBuf[128];
                                        std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                                      MapRenderState::probeOriginalX, MapRenderState::probeOriginalY, MapRenderState::probeOriginalZ);
                                        SendServerCommand(*player, abortBuf);
                                        MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                                        MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                        probeDone = true;
                                    }
                                } else {
                                    // 区块稳定但未找到安全点 → 拦截传送
                                    LogTeleport("probe ABORT-cave (" + std::to_string(probeX) + "," +
                                                std::to_string(probeZ) + ") [chunk stable but no safe spawn, intercept]");
                                    char abortBuf[128];
                                    std::snprintf(abortBuf, sizeof(abortBuf), "/tp @s %.2f %.2f %.2f",
                                                  MapRenderState::probeOriginalX, MapRenderState::probeOriginalY, MapRenderState::probeOriginalZ);
                                    SendServerCommand(*player, abortBuf);
                                    MapRenderState::teleportState.store((int)MapRenderState::TeleportState::Failed);
                                    MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                    probeDone = true;
                                }
                            }
                            // 未稳定 → 不设 probeDone, 下帧重试, 4s 超时兜底
                        } else {
                        // [防线③] 稳定性检查：获取 Y，与上一帧对比
                        short liveY = SafeGetSurfaceY(*region, probeX, probeZ);
                        if (liveY > -64 && liveY < 319) {
                            if (liveY == MapRenderState::probeLastY) {
                                MapRenderState::probeStableCount++;
                            } else {
                                MapRenderState::probeLastY = liveY;
                                MapRenderState::probeStableCount = 1;
                            }

                            // 连续 3 帧返回相同 Y → 区块加载稳定（v2: 2→3 帧）
                            if (MapRenderState::probeStableCount >= MapRenderState::kProbeStableThreshold) {
                                MapRenderState::teleportState.store(
                                    (int)MapRenderState::TeleportState::Validating);

                                // [防线④] SafeFindSafeSpawnY 落脚点验证
                                short safeY = SafeFindSafeSpawnY(*region, probeX, probeZ);
                                if (safeY > -64 && safeY < 319) {
                                    // 落脚点安全 → 传送到安全 Y
                                    float finalY = (float)safeY;
                                    char coordBuf[128];
                                    std::snprintf(coordBuf, sizeof(coordBuf),
                                                  "/tp @s %d.00 %.2f %d.00", probeX, finalY, probeZ);
                                    SendServerCommand(*player, coordBuf);
                                    LogTeleport("probe SUCCESS (" + std::to_string(probeX) + "," +
                                                std::to_string((int)finalY) + "," + std::to_string(probeZ) +
                                                ") method=stable-safe [stable=" +
                                                std::to_string(MapRenderState::probeStableCount) +
                                                " frames, chunk verified, safe spawn]");
                                    MapRenderState::teleportState.store(
                                        (int)MapRenderState::TeleportState::Done);
                                    probeDone = true;
                                } else {
                                    // [防线⑤·v2] 目标列不安全 → 螺旋搜索附近 ±16（v2: ±8→±16）
                                    int searchX = probeX, searchZ = probeZ;
                                    short nearbyY = -32000;
                                    if (FindNearestSafeSpawn(*region, searchX, searchZ, nearbyY, 16)) {
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
                                                    ") [original (" + std::to_string(probeX) + "," +
                                                    std::to_string(probeZ) + ") unsafe, searched ±16]");
                                        MapRenderState::teleportState.store(
                                            (int)MapRenderState::TeleportState::Done);
                                        probeDone = true;
                                    } else {
                                        // [v3 关键改进] 三轮搜索（±16 严格→±16 宽松→±32 宽松）均失败
                                        // v2 旧逻辑：回退原位 + 失败状态（用户反馈"避免传送失败"）
                                        // v3 新逻辑：使用 liveY+1 兜底，但仅在 liveY 经过严格验证后：
                                        //   ① liveY 已通过 3 帧稳定性验证（probeStableCount >= 3）
                                        //   ② liveY 已通过 IsChunkReady 验证
                                        //   ③ 新增：getBlock(liveY) 必须是可站立固体（IsStandableSolidName）
                                        //   ④ 新增：getBlock(liveY+1) 不能是危险方块（允许水，玩家落到水里但不会死）
                                        // 这样 liveY+1 至少在地表，不会到地下/虚空/岩浆
                                        bool usedValidatedRaw = false;
                                        try {
                                            Block const& groundBlock = region->getBlock(BlockPos(probeX, liveY, probeZ));
                                            std::string groundName = groundBlock.getTypeName();
                                            Block const& feetBlock = region->getBlock(BlockPos(probeX, liveY + 1, probeZ));
                                            std::string feetName = feetBlock.getTypeName();

                                            if (IsStandableSolidName(groundName) && !IsDangerousBlockName(feetName)) {
                                                // liveY 下方是固体，liveY+1 不是危险方块 → 使用 liveY+1 兜底
                                                // 注意：liveY+1 可能是水（玩家会落到水里），但不会是地下/虚空/岩浆
                                                float finalY = (float)(liveY + 1);
                                                char coordBuf[128];
                                                std::snprintf(coordBuf, sizeof(coordBuf),
                                                              "/tp @s %d.00 %.2f %d.00", probeX, finalY, probeZ);
                                                SendServerCommand(*player, coordBuf);
                                                LogTeleport("probe SUCCESS-validated-raw (" + std::to_string(probeX) + "," +
                                                            std::to_string((int)finalY) + "," + std::to_string(probeZ) +
                                                            ") [no safe spawn within ±32, using validated liveY+1, ground=" +
                                                            groundName + ", feet=" + feetName + "]");
                                                MapRenderState::teleportState.store(
                                                    (int)MapRenderState::TeleportState::Done);
                                                usedValidatedRaw = true;
                                            }
                                        } catch (...) {
                                            // 异常 → 不使用 raw 兜底，走回退原位
                                        }

                                        if (!usedValidatedRaw) {
                                            // liveY 下方不是固体 或 liveY+1 是危险方块 或异常 → 回退原位 + 失败状态
                                            char abortBuf[128];
                                            std::snprintf(abortBuf, sizeof(abortBuf),
                                                          "/tp @s %.2f %.2f %.2f",
                                                          MapRenderState::probeOriginalX,
                                                          MapRenderState::probeOriginalY,
                                                          MapRenderState::probeOriginalZ);
                                            SendServerCommand(*player, abortBuf);
                                            LogTeleport("probe ABORT-unsafe-raw (" + std::to_string(probeX) + "," +
                                                        std::to_string(probeZ) + ") → original [liveY=" +
                                                        std::to_string(liveY) + " ground/feet unsafe]");
                                            MapRenderState::teleportState.store(
                                                (int)MapRenderState::TeleportState::Failed);
                                            MapRenderState::teleportStatusMsg.clear();
                                            MapRenderState::teleportFailReason =
                                                LanguageManager::GetText("TELEPORT_FAILED_MSG");
                                        }
                                        probeDone = true;
                                    }
                                }
                            }
                        } else {
                            // liveY 无效（-32000 AV 或 ≤-64 虚空）→ 重置稳定性计数
                            MapRenderState::probeStableCount = 0;
                        }
                        }
                    }
                    // 区块未就绪 → 继续等待（不更新计数）
                }
            }

            if (!probeDone) {
                // [超时降级] 4 秒（原 2 秒）：回退原位
                // 4s 仍小于 320→地表 ~6s 坠落时间，/tp 重置坠落距离保证安全
                if (elapsedMs >= 4000) {
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
                                ") [chunk not ready within 4s]");
                    MapRenderState::teleportState.store(
                        (int)MapRenderState::TeleportState::Failed);
                    MapRenderState::teleportStatusMsg.clear();
                    MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_TIMEOUT_MSG");
                    probeDone = true;
                }
            }

            if (probeDone) {
                MapRenderState::pendingSurfaceProbe.store(false);
                MapRenderState::probeStableCount = 0;
                MapRenderState::probeLastY = -32000;
            }
        }

        // ==========================================
        // [分层地图系统] 跨维度与跨地下层级监听器
        // ==========================================
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

        HWND hwnd = FindWindowW(L"Minecraft", NULL);
        if (!hwnd) hwnd = GetForegroundWindow();
        
        if (hwnd && GetForegroundWindow() == hwnd) {
            if (!MapRenderState::IsUIActive()) {
                CURSORINFO ci = {};
                ci.cbSize = sizeof(CURSORINFO);
                if (GetCursorInfo(&ci)) {
                    if (ci.flags == 0) {
                        RECT clientRect;
                        GetClientRect(hwnd, &clientRect);
                        
                        // 计算游戏客户区的正中心坐标
                        POINT ptCenter = { (clientRect.right - clientRect.left) / 2, (clientRect.bottom - clientRect.top) / 2 };
                        ClientToScreen(hwnd, &ptCenter);
                        
                        // 【防覆盖层透传】将隐形指针死死锁在屏幕正中心 2x2 像素的极小死区内
                        // 彻底杜绝指针在疯狂转动视角时漂移到边缘的 Xbox Game Bar 等悬浮性能面板上导致游戏失焦
                        RECT centerRect = { ptCenter.x - 1, ptCenter.y - 1, ptCenter.x + 1, ptCenter.y + 1 };
                        ClipCursor(&centerRect);
                    } else {
                        // 处于背包、设置或原生游戏暂停菜单状态（系统鼠标显现），放开剪裁范围
                        ClipCursor(NULL);
                    }
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
            if (isScanning) {
                isScanning = false;
                currentRow = -MAP_DATA_RADIUS;
                currentCol = -MAP_DATA_RADIUS;
                std::memset(g_mapColorsBack, 0, sizeof(g_mapColorsBack));
                std::memset(g_mapHeightsBack, 0, sizeof(g_mapHeightsBack));
                ticksSinceScan = 101;  // 强制下帧启动新扫描
            }
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

                    while (currentRow <= MAP_DATA_RADIUS && !timeBudgetExceeded) {
                        int dx = currentRow;
                        int arrX = dx + MAP_DATA_RADIUS;

                        while (currentCol <= MAP_DATA_RADIUS) {
                            int dz = currentCol;
                            int targetX = currentScanX + dx;
                            int targetZ = currentScanZ + dz;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            // 确定扫描起点 Y
                            // 手动 Top Y 模式: 使用 g_caveTopY 作为起点
                            // Auto 模式: 从每列地表实心方块 (surfY-1) 开始扫描
                            //   - surfY = getAboveTopSolidBlock 返回值 = 地表上方空气格 Y
                            //   - surfY-1 = 地表实心方块 Y, 从此处开始 seenAir=false,
                            //     避免从空气开始导致 seenAir=true 误将地表方块当作洞穴地板返回 (浅洞穴 bug)
                            //   - cap=8: 限制扫描起点不超过玩家头顶 8 格
                            int startY;
                            int scanDepth;
                            if (MapRenderState::currentDimensionId == 1) {
                                // [下界地图] 下界无天空, 从天花板下方扫描到地面
                                // startY = min(120, playerY+40): 确保覆盖玩家上方的方块
                                // scanDepth = 80: 覆盖下界完整高度范围
                                startY = std::min(120, (int)g_playerY + 40);
                                scanDepth = 80;
                            } else if (!MapRenderState::g_caveTopYAuto) {
                                // 手动 Top Y: 钳制到有效范围, 防御旧配置残留的 -99 哨兵值导致整图空白
                                startY = std::clamp(MapRenderState::g_caveTopY, -64, 320);
                                scanDepth = caveDepth;
                            } else {
                                short surfY = SafeGetSurfaceY(region, targetX, targetZ);
                                startY = std::min((int)surfY - 1, (int)g_playerY + 8);
                                scanDepth = caveDepth;
                            }

                            std::string blockName;
                            int blockY = 0, depth = 0;

                            if (startY > -64 && ScanColumnCave(region, targetX, targetZ, startY, scanDepth, blockName, blockY, depth)) {
                                g_mapHeightsBack[arrX][arrZ] = (float)blockY;

                                // 液体方块: 使用饱和颜色, 不应用深度衰减
                                mce::Color liquidCol = GetCaveLiquidColor(blockName);
                                if (liquidCol.a > 0.0f) {
                                    g_mapColorsBack[arrX][arrZ] = liquidCol;
                                } else {
                                    // 实心方块: 使用洞穴专用颜色表 (含矿石/石头变种/基岩等)
                                    mce::Color baseColor = GetCaveBlockColor(blockName);

                                    // 应用深度亮度衰减
                                    // 注意: 分母必须用 scanDepth (Full=128, Layered=caveDepth),
                                    // 否则 Full 模式下 depth 超过 caveDepth 后全部钳制到最低亮度 0.375,
                                    // 深度层次信息完全丢失
                                    float brightness = ComputeCaveBrightness(depth, scanDepth);
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(
                                        baseColor.r * brightness,
                                        baseColor.g * brightness,
                                        baseColor.b * brightness,
                                        1.0f  // 洞穴模式 alpha=1 (完全不透明)
                                    );
                                }
                            } else {
                                // 列内无方块 = 空气/未探索 = 透明 (显示为黑色背景)
                                g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                                g_mapHeightsBack[arrX][arrZ] = 0.0f;
                            }

                            currentCol++;

                            if ((currentCol & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > 4000) {
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
                        // 之前缺失此调用, 导致下界扫描数据从未写入缓存, dim_1/ 文件夹始终为空
                        using ColorGrid = mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
                        using HeightGrid = float[MAP_DATA_SIZE][MAP_DATA_SIZE];
                        auto asyncColors = new ColorGrid;
                        auto asyncHeights = new HeightGrid;
                        std::memcpy(asyncColors, g_mapColorsBack, sizeof(g_mapColorsBack));
                        std::memcpy(asyncHeights, g_mapHeightsBack, sizeof(g_mapHeightsBack));
                        int asyncX = currentScanX;
                        int asyncZ = currentScanZ;
                        std::vector<MapCacheManager::BiomeEntry> asyncBiomes;
                        asyncBiomes.swap(biomeEntries);

                        std::thread([asyncX, asyncZ, asyncColors, asyncHeights, asyncBiomes = std::move(asyncBiomes), asyncDim = MapRenderState::currentDimensionId]() {
                            if (MapRenderState::g_isShuttingDown.load()) {
                                delete[] asyncColors;
                                delete[] asyncHeights;
                                return;
                            }
                            // [防跨维度写入] 若维度已变化, 丢弃过时数据, 避免下界数据写入主世界缓存
                            if (MapRenderState::currentDimensionId != asyncDim) {
                                delete[] asyncColors;
                                delete[] asyncHeights;
                                return;
                            }
                            MapCacheManager::UpdateFromScan(asyncX, asyncZ, asyncColors, asyncHeights);
                            MapCacheManager::UpdateBiomesFromScan(asyncBiomes);
                            delete[] asyncColors;
                            delete[] asyncHeights;
                        }).detach();
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
                    static mce::Color s_cachedGrass(0,0,0,0), s_cachedFoliage(0,0,0,0), s_cachedWater(0,0,0,0);

                    auto scanStartTime = std::chrono::high_resolution_clock::now();
                    bool timeBudgetExceeded = false;

                    static std::hash<std::string> hasher;

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

                                    if (aboveName == "minecraft:air" || aboveName == "air" ||
                                        aboveName.find("barrier") != std::string::npos ||
                                        aboveName.find("light_block") != std::string::npos ||
                                        aboveName.find("structure_void") != std::string::npos ||
                                        aboveName.find("placeholder") != std::string::npos ||
                                        aboveName.find("unknown") != std::string::npos ||
                                        aboveName.find("info_update") != std::string::npos) {
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
                                                    getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                                }
                                                biomeEntries.push_back({cellX, cellZ, newBiomeName});
                                            } catch (...) {
                                                if (!s_biomeName.empty()) { s_biomeName = ""; }
                                            }
                                        }

                                        static std::unordered_map<size_t, mce::Color> s_globalColorCache;
                                        if (s_globalColorCache.size() > 20000) s_globalColorCache.clear();

                                        size_t cacheKey = hasher(blockName) ^ (hasher(s_biomeName) << 1);
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
                            } else {
                                g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                            }

                            currentCol++;

                            if ((currentCol & 31) == 0) {
                                auto now = std::chrono::high_resolution_clock::now();
                                if (std::chrono::duration_cast<std::chrono::microseconds>(now - scanStartTime).count() > 4000) {
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

                            using ColorGrid = mce::Color[MAP_DATA_SIZE][MAP_DATA_SIZE];
                            using HeightGrid = float[MAP_DATA_SIZE][MAP_DATA_SIZE];
                            auto asyncColors = new ColorGrid;
                            auto asyncHeights = new HeightGrid;
                            std::memcpy(asyncColors, g_mapColorsBack, sizeof(g_mapColorsBack));
                            std::memcpy(asyncHeights, g_mapHeightsBack, sizeof(g_mapHeightsBack));
                            int asyncX = currentScanX;
                            int asyncZ = currentScanZ;
                            // [新增] 将采集的生物群系条目转移到局部变量（O(1) swap），供异步线程写入缓存
                            std::vector<MapCacheManager::BiomeEntry> asyncBiomes;
                            asyncBiomes.swap(biomeEntries);

                            std::thread([asyncX, asyncZ, asyncColors, asyncHeights, asyncBiomes = std::move(asyncBiomes), asyncDim = MapRenderState::currentDimensionId]() {
                                // [关闭期安全] 若 disable() 已开始（缓存 Shutdown 可能已清空 g_loadedRegions，
                                // 后续 SwitchWorld 析构也会 delete 所有 region），此处继续写入会触达悬空指针。
                                // 立即释放本线程持有的扫描缓冲，让 MapCacheManager::Shutdown 完成干净退出。
                                if (MapRenderState::g_isShuttingDown.load()) {
                                    delete[] asyncColors;
                                    delete[] asyncHeights;
                                    return;
                                }
                                // [防跨维度写入] 若维度已变化, 丢弃过时数据, 避免数据写入错误维度缓存
                                if (MapRenderState::currentDimensionId != asyncDim) {
                                    delete[] asyncColors;
                                    delete[] asyncHeights;
                                    return;
                                }
                                MapCacheManager::UpdateFromScan(asyncX, asyncZ, asyncColors, asyncHeights);
                                MapCacheManager::UpdateBiomesFromScan(asyncBiomes);
                                delete[] asyncColors;
                                delete[] asyncHeights;
                            }).detach();
                        }
                        // else: 玩家在地下, 丢弃本次扫描数据, 保留 g_mapColors 和缓存中的纯净地表数据
                    }
                }
            } catch (...) {}
        }

        static int entityDelay = 0;
        if (++entityDelay >= 60) {
            entityDelay = 0;
            std::vector<RadarEntity> tempEntities;
            auto& level = player->getLevel();
            const auto& entities = level.getRuntimeActorList();
            
            for (auto* actor : entities) {
                if (!actor || actor == player) continue;
                if (!actor->isAlive()) continue;

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
    if (MapRenderState::IsUIActive()) return;
    origin(slot, item);
}

LL_TYPE_INSTANCE_HOOK(
    GameModeAttackHook,
    ll::memory::HookPriority::Normal,
    GameMode,
    &GameMode::$attack,
    bool,
    Actor& actor
) {
    if (MapRenderState::IsUIActive()) return false;
    return origin(actor);
}

    LL_TYPE_INSTANCE_HOOK(
        ActorIsImmobileHook,
        ll::memory::HookPriority::Normal,
        Actor,
        &Actor::$isImmobile,
        bool
    ) {
        if (MapRenderState::showBigMap && g_localPlayer && this == (Actor*)g_localPlayer) return true;
        return origin();
    }

    LL_TYPE_INSTANCE_HOOK(
        LocalPlayerSwingHook,
        ll::memory::HookPriority::Normal,
        LocalPlayer,
        &LocalPlayer::$swing,
        bool,
        ActorSwingSource swingSource
    ) {
        if (MapRenderState::IsUIActive()) return false;
        return origin(swingSource);
    }

    LL_TYPE_INSTANCE_HOOK(
        PlayerAttackHook,
        ll::memory::HookPriority::Highest,
        Player,
        &Player::$attack,
        ActorHurtResult,
        Actor& target,
        SharedTypes::Legacy::ActorDamageCause const& damageCause
    ) {
        if (MapRenderState::IsUIActive() && g_localPlayer && (Player*)this == g_localPlayer) return ActorHurtResult{};
        return origin(target, damageCause);
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
    if (MapRenderState::IsUIActive()) return false;
    return origin(item);
}