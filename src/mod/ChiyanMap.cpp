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
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include <chrono>
#include <atomic>
#include <vector>
#include <string>
#include <algorithm>

ll::event::ListenerPtr g_mouseListener = nullptr;

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
        // 手动构造原始版本字符串 (LeviLamina 输出格式如 "1.26.40-5")
        std::string rawVer = fmt::format("{}.{}.{}-{}", gameVer->major, gameVer->minor, gameVer->patch, gameVer->build.value_or("5"));
        std::string displayVer = rawVer;

        // 将 1.26.40-5 格式美化为玩家熟悉的 1.26.40.05 用于日志友好输出
        size_t dashPos = displayVer.find('-');
        if (dashPos != std::string::npos) {
            std::string tail = displayVer.substr(dashPos + 1);
            if (tail.length() == 1) {
                displayVer.replace(dashPos, 1, ".0"); // 单个数字补零
            } else {
                displayVer.replace(dashPos, 1, ".");  // 双数直接换点
            }
        }

        // 严格限定游戏版本为 1.26.40.05，不匹配则不加载
        if (displayVer != "1.26.40.05" && rawVer != "1.26.40-5") {
            getSelf().getLogger().error("{}: {}", LanguageManager::GetText("LOG_VERSION_MISMATCH"), displayVer);
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_STRICT"));
            getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
            return false;
        }
        getSelf().getLogger().info("{}: {}", LanguageManager::GetText("LOG_VERSION_PASS"), displayVer);
    } else {
        // 无法识别游戏客户端版本，严格拒绝加载以防崩溃
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_UNKNOWN"));
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
        return false;
    }

    registerAllHooks();
    MapCacheManager::Init();
    WaypointManager::Init(); // 初始化地标 JSON 引擎
    return true;
}

bool ChiyanMap::enable() {
    g_mouseListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::input::MouseInputEvent>(
        [](ll::event::input::MouseInputEvent& ev) {
            if (MapRenderState::g_isShuttingDown.load()) return;
            if (MapRenderState::IsUIActive()) {
                ev.cancel();
            }
        }
    );
    return true;
}

bool ChiyanMap::disable() {
    // [修复] 置位关闭标志并清空全局指针，使所有钩子入口直接 pass-through 放行，
    // 防止进程退出阶段访问已释放的 D3D/ImGui/Player 资源；
    // 退出时不主动卸载跳板内存，确保 Minecraft 全局析构函数通过跳板时依然安全执行原函数
    MapRenderState::g_isShuttingDown.store(true);
    if (g_mouseListener) {
        ll::event::EventBus::getInstance().removeListener(g_mouseListener);
        g_mouseListener = nullptr;
    }
    g_hasPlayer = false;
    g_localPlayer = nullptr;
    g_clientInstance = nullptr;

    shutdownCacheWriteThread();
    MapCacheManager::Shutdown();
    shutdownDX11Hook();
    return true;
}

}

LL_REGISTER_MOD(chiyan_map::ChiyanMap, chiyan_map::ChiyanMap::getInstance());
