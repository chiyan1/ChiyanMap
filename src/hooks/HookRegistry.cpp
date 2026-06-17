#include "hooks/HookRegistry.h"
#include "hooks/PlayerHook.h"
#include "hooks/UIRenderHook.h"

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