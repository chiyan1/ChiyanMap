#include "mod/ChiyanMap.h"
#include "hooks/HookRegistry.h"
#include "state/MapRenderState.h"
#include "state/MapCacheManager.h"
#include "state/WaypointManager.h"
#include "state/LanguageManager.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/utils/SystemUtils.h"
#include "ll/api/io/FileUtils.h"
#include "ll/api/data/Version.h"
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>

float g_playerX       = 0.0f;
float g_playerY       = 0.0f;
float g_playerZ       = 0.0f;
float g_playerYaw     = 0.0f;
bool  g_hasPlayer     = false;

class LocalPlayer;
class ClientInstance;

LocalPlayer* g_localPlayer    = nullptr;
ClientInstance* g_clientInstance = nullptr;

#include <mc/deps/core/math/Color.h>
#include <mc/deps/core/math/Vec3.h>

mce::Color g_mapColors[MAP_DATA_SIZE][MAP_DATA_SIZE]  = {};
float      g_mapHeights[MAP_DATA_SIZE][MAP_DATA_SIZE] = {};

Vec3 g_prevPhysicsPos{0.0f, 0.0f, 0.0f};
Vec3 g_currPhysicsPos{0.0f, 0.0f, 0.0f};
std::chrono::steady_clock::time_point g_lastPhysicsTime = std::chrono::steady_clock::now();

int g_playerBlockX = 0;
int g_playerBlockZ = 0;

std::atomic<bool> g_radarUpdated{false};
std::vector<RadarEntity> g_radarEntities;
// 注意：旧的 g_mapDataUpdated 已经删除以修复 LNK2001

namespace chiyan_map {

ChiyanMap& ChiyanMap::getInstance() {
    static ChiyanMap instance;
    return instance;
}

bool ChiyanMap::load() {
    // 提前初始化语言引擎，确保拦截器可以使用多语言控制台日志
    LanguageManager::Init();

    // [版本拦截防崩系统] 检测游戏客户端版本，防止因结构体/虚表偏移改变导致的进档崩溃
    auto exePathOpt = ll::sys_utils::getModulePath(nullptr);
    std::optional<ll::data::Version> gameVer = std::nullopt;
    if (exePathOpt) {
        gameVer = ll::file_utils::getVersion(*exePathOpt);
    }
    
    if (gameVer) {
        // 将 LeviLamina 默认输出的 1.26.20-4 格式安全转换为 1.26.20.4
        std::string verStr = fmt::format("{}", gameVer->to_string());
        std::replace(verStr.begin(), verStr.end(), '-', '.');

        // 使用字符串精准匹配，避免 Version 结构体映射差异导致的验证失败
        if (verStr != "1.26.20.4" && verStr != "1.26.20.04") {
            getSelf().getLogger().error("{}: {}", LanguageManager::GetText("LOG_VERSION_MISMATCH"), verStr);
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_STRICT"));
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
            return false;
        }
        getSelf().getLogger().info("{}: {}", LanguageManager::GetText("LOG_VERSION_PASS"), verStr);
    } else {
        getSelf().getLogger().warn("{}", LanguageManager::GetText("LOG_VERSION_UNKNOWN"));
    }

    registerAllHooks();
    MapCacheManager::Init();
    WaypointManager::Init(); // 初始化地标 JSON 引擎
    return true;
}

bool ChiyanMap::enable()  { return true; }

bool ChiyanMap::disable() {
    MapCacheManager::Shutdown();
    unregisterAllHooks();
    return true;
}

}

LL_REGISTER_MOD(chiyan_map::ChiyanMap, chiyan_map::ChiyanMap::getInstance());
