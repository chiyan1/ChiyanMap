#pragma once
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <cstring>
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
    if (LanguageManager::g_currentLanguage != "zh_CN" && LanguageManager::g_currentLanguage != "zh_TW") {
        std::string formattedName = rawName;
        for (size_t i = 0; i < formattedName.length(); ++i) {
            if (formattedName[i] == '_') formattedName[i] = ' ';
            if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
        }
        return formattedName;
    }

    std::string lower = rawName;
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 32;
    
    if (lower.find("plains") != std::string::npos && lower.find("ice") == std::string::npos && lower.find("snow") == std::string::npos) return "平原";
    if (lower.find("desert") != std::string::npos) return "沙漠";
    if (lower.find("extreme_hills") != std::string::npos || lower.find("windswept") != std::string::npos) return "风啸山丘";
    if (lower.find("forest") != std::string::npos && lower.find("dark") == std::string::npos && lower.find("birch") == std::string::npos && lower.find("crimson") == std::string::npos && lower.find("warped") == std::string::npos) return "森林";
    if (lower.find("taiga") != std::string::npos) return "针叶林";
    if (lower.find("swamp") != std::string::npos) {
        if (lower.find("mangrove") != std::string::npos) return "红树林沼泽";
        return "沼泽";
    }
    if (lower.find("river") != std::string::npos) return "河流";
    if (lower.find("hell") != std::string::npos || lower.find("nether") != std::string::npos) return "下界荒野";
    if (lower.find("the_end") != std::string::npos) return "末地";
    if (lower.find("ocean") != std::string::npos) {
        if (lower.find("frozen") != std::string::npos) return "冻洋";
        if (lower.find("warm") != std::string::npos) return "暖洋";
        if (lower.find("cold") != std::string::npos) return "冷洋";
        return "海洋";
    }
    if (lower.find("ice_plains") != std::string::npos || lower.find("snowy_plains") != std::string::npos) return "积雪平原";
    if (lower.find("ice_spikes") != std::string::npos) return "冰刺平原";
    if (lower.find("mushroom") != std::string::npos) return "蘑菇岛";
    if (lower.find("beach") != std::string::npos) return "海滩";
    if (lower.find("jungle") != std::string::npos) {
        if (lower.find("bamboo") != std::string::npos) return "竹林";
        return "丛林";
    }
    if (lower.find("birch_forest") != std::string::npos) return "白桦森林";
    if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos) return "黑橡木森林";
    if (lower.find("savanna") != std::string::npos) return "热带草原";
    if (lower.find("mesa") != std::string::npos || lower.find("badlands") != std::string::npos) return "恶地";
    if (lower.find("cherry") != std::string::npos) return "樱花树林";
    if (lower.find("crimson_forest") != std::string::npos) return "绯红森林";
    if (lower.find("warped_forest") != std::string::npos) return "诡异森林";
    if (lower.find("soul_sand_valley") != std::string::npos) return "灵魂沙峡谷";
    if (lower.find("basalt_deltas") != std::string::npos) return "玄武岩三角洲";
    if (lower.find("meadow") != std::string::npos) return "草甸";
    if (lower.find("grove") != std::string::npos) return "雪林";
    if (lower.find("snowy_slopes") != std::string::npos) return "积雪的山坡";
    if (lower.find("jagged_peaks") != std::string::npos) return "尖峭山峰";
    if (lower.find("frozen_peaks") != std::string::npos) return "冰封山峰";
    if (lower.find("stony_peaks") != std::string::npos) return "裸岩山峰";
    if (lower.find("deep_dark") != std::string::npos) return "深暗之域";
    if (lower.find("pale_garden") != std::string::npos) return "苍白花园";
    
    return "未知群系";
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
    else if (lower.find("dark_forest") != std::string::npos || lower.find("roofed_forest") != std::string::npos) {
        grass   = mce::Color(0.20f, 0.30f, 0.12f, 1.0f);
        foliage = mce::Color(0.15f, 0.25f, 0.10f, 1.0f);
    }
    else if (lower.find("meadow") != std::string::npos || lower.find("grove") != std::string::npos) {
        grass   = mce::Color(0.28f, 0.42f, 0.28f, 1.0f);
        foliage = mce::Color(0.18f, 0.32f, 0.20f, 1.0f);
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

    if (name.find("wart") != std::string::npos) return mce::Color(0.65f, 0.10f, 0.10f, 1.0f);
    if (name.find("chorus") != std::string::npos) return mce::Color(0.60f, 0.40f, 0.60f, 1.0f);

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

    if (name.find("netherrack") != std::string::npos) return mce::Color(0.45f, 0.12f, 0.12f, 1.0f);
    if (name.find("magma") != std::string::npos) return mce::Color(0.60f, 0.20f, 0.08f, 1.0f);
    if (name.find("lava") != std::string::npos) return mce::Color(0.95f, 0.30f, 0.0f, 1.0f);
    if (name.find("nether_brick") != std::string::npos) return mce::Color(0.25f, 0.10f, 0.15f, 1.0f);
    if (name.find("blackstone") != std::string::npos) return mce::Color(0.15f, 0.15f, 0.18f, 1.0f);
    if (name.find("basalt") != std::string::npos) return mce::Color(0.30f, 0.30f, 0.32f, 1.0f);
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
        return foliageCol;
    }

    if (name.find("blue_ice") != std::string::npos) return mce::Color(0.45f, 0.65f, 0.95f, 1.0f);
    if (name.find("packed_ice") != std::string::npos) return mce::Color(0.55f, 0.75f, 0.95f, 1.0f);
    if (name.find("ice") != std::string::npos || name.find("frosted") != std::string::npos) return mce::Color(0.65f, 0.85f, 0.95f, 1.0f);
    if (name.find("snow") != std::string::npos) return mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
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

    if (name.find("planks") != std::string::npos || name.find("oak") != std::string::npos || name.find("spruce") != std::string::npos || name.find("birch") != std::string::npos || name.find("jungle") != std::string::npos || name.find("acacia") != std::string::npos || name.find("dark_oak") != std::string::npos) return mce::Color(0.65f, 0.45f, 0.25f, 1.0f);
    if (name.find("wood") != std::string::npos || name.find("log") != std::string::npos || name.find("stem") != std::string::npos || name.find("stairs") != std::string::npos || name.find("slab") != std::string::npos || name.find("fence") != std::string::npos || name.find("door") != std::string::npos || name.find("trapdoor") != std::string::npos || name.find("sign") != std::string::npos || name.find("chest") != std::string::npos) return mce::Color(0.55f, 0.40f, 0.20f, 1.0f);
    if (name.find("stone") != std::string::npos || name.find("cobble") != std::string::npos || name.find("andesite") != std::string::npos || name.find("diorite") != std::string::npos || name.find("granite") != std::string::npos || name.find("tuff") != std::string::npos || name.find("brick") != std::string::npos || name.find("wall") != std::string::npos || name.find("gravel") != std::string::npos || name.find("clay") != std::string::npos) return mce::Color(0.55f, 0.55f, 0.55f, 1.0f);

    unsigned int h = 0;
    for (char c : name) h = h * 31 + c;
    float r = 0.45f + ((h >> 16) & 0xFF) / 255.0f * 0.4f;
    float g = 0.45f + ((h >> 8) & 0xFF) / 255.0f * 0.4f;
    float b = 0.45f + (h & 0xFF) / 255.0f * 0.4f;
    return mce::Color(r, g, b, 1.0f);
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
    g_clientInstance = this;

    auto* player = this->getLocalPlayer();
        if (player && this->isWorldActive()) {
            const Vec3& pos = player->getPosition();

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
        // [防窒息 & 神躯护体] 智能高度推算与无敌降落引擎
        // ==========================================
        if (MapRenderState::triggerTeleport.load()) {
            float targetX = MapRenderState::tpTargetX;
            float targetY = MapRenderState::tpTargetY;
            float targetZ = MapRenderState::tpTargetZ;
            
            bool needsSafeFall = false;

            if (targetY < -500.0f) {
                BlockSource* region = this->getRegion();
                if (region) {
                    short topY = region->getAboveTopSolidBlock((int)targetX, (int)targetZ, true, true);
                    if (topY > -60 && topY < 319) {
                        targetY = (float)topY + 1.0f; 
                    } else {
                        targetY = 320.0f; 
                        needsSafeFall = true; 
                    }
                } else {
                    targetY = 320.0f;
                    needsSafeFall = true;
                }
            } else if (std::abs(targetY - 320.0f) < 0.1f) {
                // 【完成需求3】仅当精确传送到高空 Y=320 时，才触发无敌抗性护体
                needsSafeFall = true;
            }

            std::thread([needsSafeFall, targetX, targetY, targetZ]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
                
                auto sendCommand = [](const char* cmd) {
                    bool clipSuccess = false;
                    for (int i = 0; i < 5; ++i) { 
                        if (OpenClipboard(nullptr)) {
                            EmptyClipboard();
                            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, std::strlen(cmd) + 1);
                            if (hMem) {
                                std::memcpy(GlobalLock(hMem), cmd, std::strlen(cmd) + 1);
                                GlobalUnlock(hMem);
                                SetClipboardData(CF_TEXT, hMem);
                            }
                            CloseClipboard();
                            clipSuccess = true;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    }
                    if (!clipSuccess) return;

                    keybd_event(VK_RETURN, 0, 0, 0); keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(150)); 
                    
                    keybd_event(VK_CONTROL, 0, 0, 0); keybd_event('A', 0, 0, 0);
                    keybd_event('A', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    keybd_event(VK_BACK, 0, 0, 0); keybd_event(VK_BACK, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));

                    keybd_event(VK_CONTROL, 0, 0, 0); keybd_event('V', 0, 0, 0);
                    keybd_event('V', 0, KEYEVENTF_KEYUP, 0); keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    
                    keybd_event(VK_RETURN, 0, 0, 0); keybd_event(VK_RETURN, 0, KEYEVENTF_KEYUP, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
                };

                if (needsSafeFall) {
                    char cmd1[128];
                    snprintf(cmd1, sizeof(cmd1), "/effect @s resistance 30 255 true");
                    sendCommand(cmd1);
                }

                char cmd2[128];
                snprintf(cmd2, sizeof(cmd2), "/tp @s %d %d %d", (int)targetX, (int)targetY, (int)targetZ);
                sendCommand(cmd2);
                
            }).detach();
            
            MapRenderState::triggerTeleport.store(false);
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
                    std::string translatedName = TranslateBiomeName(rawName);
                    
                    if (LanguageManager::g_currentLanguage == "zh_CN" || LanguageManager::g_currentLanguage == "zh_TW") {
                        std::string formattedName = rawName;
                        for (size_t i = 0; i < formattedName.length(); ++i) {
                            if (formattedName[i] == '_') formattedName[i] = ' ';
                            if (i == 0 || formattedName[i - 1] == ' ') formattedName[i] = (char)std::toupper(formattedName[i]);
                        }
                        MapRenderState::currentBiomeName = formattedName + " (" + translatedName + ")";
                    } else {
                        MapRenderState::currentBiomeName = translatedName;
                    }
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
                        POINT ptTopLeft = { clientRect.left, clientRect.top };
                        POINT ptBottomRight = { clientRect.right, clientRect.bottom };
                        ClientToScreen(hwnd, &ptTopLeft);
                        ClientToScreen(hwnd, &ptBottomRight);
                        RECT screenRect = { ptTopLeft.x, ptTopLeft.y, ptBottomRight.x, ptBottomRight.y };
                        ClipCursor(&screenRect);
                    } else {
                        ClipCursor(NULL);
                    }
                }
            }
        }

        static int currentScanX  = -99999;
        static int currentScanZ  = -99999;
        static bool isScanning   = false;
        static int  currentRow   = -MAP_DATA_RADIUS;
        static int  ticksSinceScan = 0; 

        int px = g_playerBlockX;
        int pz = g_playerBlockZ;
        ticksSinceScan++;

        if (!isScanning && (px != currentScanX || pz != currentScanZ || ticksSinceScan > 60)) {
            currentScanX  = px;
            currentScanZ  = pz;
            ticksSinceScan = 0;
            
            if (MapRenderState::currentDimensionId == 1) {
                isScanning = false;
            } else {
                isScanning = true;
                currentRow = -MAP_DATA_RADIUS;
            }
        }

        if (isScanning) {
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

                    while (rowsThisFrame < 15 && currentRow <= MAP_DATA_RADIUS) {
                        int dx = currentRow;
                        for (int dz = -MAP_DATA_RADIUS; dz <= MAP_DATA_RADIUS; dz++) {
                            int targetX = currentScanX + dx;
                            int targetZ = currentScanZ + dz;
                            int arrX    = dx + MAP_DATA_RADIUS;
                            int arrZ    = dz + MAP_DATA_RADIUS;

                            short topY = region.getAboveTopSolidBlock(targetX, targetZ, true, true);
                            g_mapHeightsBack[arrX][arrZ] = (float)topY;

                            if (topY > -64) {
                                Block const& block = region.getBlock(BlockPos(targetX, topY - 1, targetZ));
                                std::string blockName = block.getTypeName();

                                if (blockName.find("snow") != std::string::npos) {
                                    g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                    continue;
                                }

                                {
                                    Block const& blockAbove = region.getBlock(BlockPos(targetX, topY, targetZ));
                                    std::string aboveName = blockAbove.getTypeName();

                                    if (aboveName == "minecraft:air" || aboveName == "air" ||
                                        aboveName.find("barrier") != std::string::npos ||
                                        aboveName.find("light_block") != std::string::npos ||
                                        aboveName.find("structure_void") != std::string::npos ||
                                        aboveName.find("placeholder") != std::string::npos ||
                                        aboveName.find("unknown") != std::string::npos ||
                                        aboveName.find("info_update") != std::string::npos) {
                                    }
                                    else if (aboveName.find("snow") != std::string::npos) {
                                        g_mapColorsBack[arrX][arrZ] = mce::Color(0.95f, 0.98f, 1.0f, 1.0f);
                                        s_lastBlockName = "";
                                        continue;
                                    }
                                    else {
                                        blockName = aboveName;
                                    }
                                }

                                int cellX = targetX >> 2;
                                int cellZ = targetZ >> 2;
                                bool tintChanged = false;

                                if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
                                    s_biomeCellX = cellX; s_biomeCellZ = cellZ;
                                    try {
                                        auto const& biome = region.getBiome(BlockPos(targetX, topY - 1, targetZ));
                                        std::string newBiomeName = biome.mHash->getString();
                                        if (s_biomeName != newBiomeName) {
                                            s_biomeName = newBiomeName;
                                            getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                            tintChanged = true; 
                                        }
                                    } catch (...) { 
                                        if (!s_biomeName.empty()) { s_biomeName = ""; tintChanged = true; }
                                    }
                                }

                                if (!tintChanged && blockName == s_lastBlockName) {
                                    g_mapColorsBack[arrX][arrZ] = s_lastBlockColor;
                                    continue;
                                }
                                
                                s_lastBlockName = blockName;
                                s_lastBlockColor = getBlockColor(blockName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
                                g_mapColorsBack[arrX][arrZ] = s_lastBlockColor;
                            } else {
                                g_mapColorsBack[arrX][arrZ] = mce::Color(0.0f, 0.0f, 0.0f, 0.0f);
                                s_lastBlockName = "";
                            }
                        }
                        currentRow++;
                        rowsThisFrame++;
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
                        MapCacheManager::UpdateFromScan(currentScanX, currentScanZ, g_mapColorsBack, g_mapHeightsBack);
                    }
                }
            } catch (...) {}
        }

        static int entityDelay = 0;
        if (++entityDelay >= 2) {
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
    if (MapRenderState::showBigMap && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return false;
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
    if (MapRenderState::showBigMap && g_localPlayer && this == g_localPlayer) return;
    origin(isSneaking);
}

LL_TYPE_INSTANCE_HOOK(
    LocalPlayerIsImmobileHook,
    ll::memory::HookPriority::Normal,
    Player,
    &Player::$isImmobile,
    bool
) {
    if (MapRenderState::showBigMap && g_localPlayer && (Player*)this == (Player*)g_localPlayer) return true;
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
    if (MapRenderState::showBigMap && g_localPlayer && this == (Actor*)g_localPlayer) return;
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