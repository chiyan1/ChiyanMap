#pragma once
#include <ll/api/memory/Hook.h>
#include <mc/client/renderer/screen/MinecraftUIRenderContext.h>
#include <mc/client/game/ClientInstance.h>
#include <optional>

#include "mod/ChiyanMap.h"
#include "state/MapRenderState.h"
#include "hooks/PlayerHook.h"
#include "hooks/DX11Hook.h" 

LL_TYPE_INSTANCE_HOOK(
    UIRenderContextFlushTextHook,
    ll::memory::HookPriority::Normal,
    MinecraftUIRenderContext,
    &MinecraftUIRenderContext::$flushText,
    void,
    float deltaTime,
    std::optional<float> obfuscateSwitchTime
) {
    origin(deltaTime, obfuscateSwitchTime);

    // 等游戏 UI 引擎开始运转后，挂钩 DXGI 底层
    static bool s_dxgiHooked = false;
    if (!s_dxgiHooked) { 
        s_dxgiHooked = true; 
        DX11Hook::init(); 
    }

    if (!g_clientInstance || !g_hasPlayer) return;

    // 更新帧计数，协助 PlayerHook 进行平滑扫描
    int callIndex = ++MapRenderState::frameCallCount;
    if (callIndex < MapRenderState::lastFrameTotalCalls) return;
}