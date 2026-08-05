#include "hooks/HookRegistry.h"
#include "hooks/PlayerHook.h"
#include "hooks/UIRenderHook.h"
#include "hooks/DX11Hook.h"

void registerAllHooks() {
    // 只注册原生的游戏逻辑与 UI 钩子
    ClientInstanceUpdateHook::hook();
    UIRenderContextFlushTextHook::hook();

    LocalPlayerApplyTurnDeltaHook::hook();
    GameModeStartDestroyBlockHook::hook();
    GameModeUseItemHook::hook();
    GameModeAttackHook::hook();

    ActorIsImmobileHook::hook();
    LocalPlayerSwingHook::hook();
    GameModeUseItemOnHook::hook();
    GameModeContinueDestroyBlockHook::hook();

    MobSetCarriedItemHook::hook();
    GameModeBaseUseItemHook::hook();

    PlayerInventorySelectSlotHook::hook();
    PlayerInventorySetItemHook::hook();
    LocalPlayerPickBlockHook::hook();
    LocalPlayerJumpFromGroundHook::hook();
    LocalPlayerSetSneakingHook::hook();
    LocalPlayerIsImmobileHook::hook();
}

void unregisterAllHooks() {
    ClientInstanceUpdateHook::unhook();
    UIRenderContextFlushTextHook::unhook();

    LocalPlayerApplyTurnDeltaHook::unhook();
    GameModeStartDestroyBlockHook::unhook();
    GameModeUseItemHook::unhook();
    GameModeAttackHook::unhook();

    ActorIsImmobileHook::unhook();
    LocalPlayerSwingHook::unhook();
    GameModeUseItemOnHook::unhook();
    GameModeContinueDestroyBlockHook::unhook();

    MobSetCarriedItemHook::unhook();
    GameModeBaseUseItemHook::unhook();

    PlayerInventorySelectSlotHook::unhook();
    PlayerInventorySetItemHook::unhook();
    LocalPlayerPickBlockHook::unhook();
    LocalPlayerJumpFromGroundHook::unhook();
    LocalPlayerSetSneakingHook::unhook();
    LocalPlayerIsImmobileHook::unhook();
}

// 非 inline 包装: ChiyanMap.cpp 无法包含 PlayerHook.h (hook 宏会多重定义),
// 通过此包装函数桥接调用 inline 的 ShutdownCacheWriteThread()
void shutdownCacheWriteThread() {
    ShutdownCacheWriteThread();
}

// 非 inline 包装: ChiyanMap.cpp 无法包含 DX11Hook.h (会传递包含 PlayerHook.h 导致多重定义),
// 通过此包装函数桥接调用 inline 的 DX11Hook::shutdown()
void shutdownDX11Hook() {
    DX11Hook::shutdown();
}