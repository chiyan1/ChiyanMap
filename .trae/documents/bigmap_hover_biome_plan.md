# 全屏大地图鼠标悬停生物群系显示

## Context

当前全屏大地图顶部显示**玩家当前所在生物群系**（`MapRenderState::rawBiomeName`/`translatedBiomeName`），由 `PlayerHook.h:616-628` 每帧更新。用户希望改为显示**鼠标指针悬停位置**的生物群系，以便在浏览大地图时了解任意位置的地形信息。小地图下方的生物群系显示（玩家当前生物群系）保持不变。

**关键约束**：
- 渲染线程（DX11 Present hook）访问 BlockSource 已有先例（右键菜单 `DX11Hook.h:1573` 的 `SafeGetSurfaceY`）
- 部分加载区块可能触发 AV 崩溃（刚修复的 0xC0000005），需异常保护
- `/EHa` 模式下 `catch(...)` 可捕获 SEH AV，但 `__try/__except` 不能与 `std::string` 析构共存

## 修改文件

### 1. `src/state/MapRenderState.h` — 新增悬停生物群系状态字段

在 `translatedBiomeName`（第 17 行）后新增独立字段组，与玩家生物群系隔离：

```cpp
// [大地图鼠标悬停生物群系] 独立于玩家当前生物群系，不影响小地图
inline std::string hoverBiomeRawName = "";          // 鼠标位置原始生物群系名
inline std::string hoverBiomeTranslatedName = "";   // 鼠标位置翻译后生物群系名
inline float hoverBiomeAlpha = 0.0f;                 // 当前显示透明度 (0=隐藏, 1=完全显示)
inline float hoverBiomeTargetAlpha = 0.0f;           // 目标透明度
inline int hoverBiomeFrameCounter = 0;               // 节流帧计数器
inline float hoverBiomeLastQueryX = 0.0f;            // 上次查询的世界 X
inline float hoverBiomeLastQueryZ = 0.0f;            // 上次查询的世界 Z
inline bool hoverBiomeHasValidResult = false;        // 是否有有效查询结果
```

### 2. `src/hooks/PlayerHook.h` — 新增 `SafeGetBiomeName` 辅助函数

在 `SafeGetSurfaceY`（第 382 行）后新增。因 `getBiome` 返回 `Biome const&` 且需 `std::string` 赋值（含析构），不能用 `__try/__except`，改用 `try/catch(...)`（/EHa 下可捕获 SEH AV），与 `PlayerHook.h:616,773` 现有 `getBiome` 调用风格一致：

```cpp
inline bool SafeGetBiomeName(BlockSource& region, int x, int y, int z,
                             std::string& outRawName) noexcept {
    try {
        auto const& biome = region.getBiome(BlockPos(x, y, z));
        outRawName = biome.mHash->getString();
        return !outRawName.empty();
    } catch (...) {
        return false;  // 部分加载区块 AV → 视为未就绪
    }
}
```

### 3. `src/hooks/DX11Hook.h` — 替换大地图生物群系显示（第 1415-1422 行）

将原 8 行玩家生物群系渲染替换为鼠标悬停逻辑，核心流程：

1. **大地图开启检测**：首次打开时重置 alpha=0，实现淡入
2. **节流**：帧计数≥6 或鼠标世界位移>4 格才查询
3. **前置条件**：`isHoveringCanvas && currentDimensionId != 1`
4. **region 渲染检查**：`g_regionSRVs` 查找鼠标所在 region
5. **生物群系查询**：`SafeGetBiomeName`，Y 用 `GetCachedSurfaceHeight` 回退玩家 Y
6. **alpha lerp**：`alpha += (target - alpha) * clamp(12*dt, 0, 1)`（帧率无关）
7. **绘制**：仅 `alpha > 0.01 && hasValidResult` 时绘制，颜色按 alpha 缩放

关键代码结构：
```cpp
{
    // 大地图刚打开时重置状态
    static bool wasBigMapOpen = false;
    if (!wasBigMapOpen && MapRenderState::showBigMap) {
        MapRenderState::hoverBiomeAlpha = 0.0f;
        MapRenderState::hoverBiomeTargetAlpha = 0.0f;
        MapRenderState::hoverBiomeHasValidResult = false;
        MapRenderState::hoverBiomeFrameCounter = 6;
    }
    wasBigMapOpen = MapRenderState::showBigMap;

    bool canQuery = isHoveringCanvas && (MapRenderState::currentDimensionId != 1);
    MapRenderState::hoverBiomeFrameCounter++;
    bool frameThrottle = (MapRenderState::hoverBiomeFrameCounter >= 6);
    float dxh = hoverWx - MapRenderState::hoverBiomeLastQueryX;
    float dzh = hoverWz - MapRenderState::hoverBiomeLastQueryZ;
    bool movedEnough = (dxh*dxh + dzh*dzh) > 16.0f;

    if (canQuery && (frameThrottle || (movedEnough && MapRenderState::hoverBiomeFrameCounter >= 2))) {
        MapRenderState::hoverBiomeFrameCounter = 0;
        int bx = (int)std::floor(hoverWx);
        int bz = (int)std::floor(hoverWz);
        int rx = (int)std::floor(hoverWx / 256.0f);
        int rz = (int)std::floor(hoverWz / 256.0f);
        uint64_t hash = MapCacheManager::GetRegionHash(rx, rz);

        bool regionRendered = false;
        auto srvIt = g_regionSRVs.find(hash);
        if (srvIt != g_regionSRVs.end() && srvIt->second) regionRendered = true;

        if (regionRendered) {
            int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(bx, bz);
            int queryY = (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64)
                       ? (int)cachedY : (g_hasPlayer ? (int)std::floor(g_playerY) : 64);
            std::string rawName;
            bool ok = false;
            if (g_clientInstance) {
                BlockSource* region = g_clientInstance->getRegion();
                if (region) ok = SafeGetBiomeName(*region, bx, queryY, bz, rawName);
            }
            if (ok) {
                // 剥离命名空间前缀
                std::string displayRaw = rawName;
                size_t colonPos = displayRaw.find(":");
                if (colonPos != std::string::npos) displayRaw = displayRaw.substr(colonPos + 1);
                MapRenderState::hoverBiomeRawName = displayRaw;
                MapRenderState::hoverBiomeTranslatedName = TranslateBiomeName(rawName);
                MapRenderState::hoverBiomeHasValidResult = true;
                MapRenderState::hoverBiomeLastQueryX = hoverWx;
                MapRenderState::hoverBiomeLastQueryZ = hoverWz;
                MapRenderState::hoverBiomeTargetAlpha = 1.0f;
            } else {
                MapRenderState::hoverBiomeTargetAlpha = 0.0f;
                MapRenderState::hoverBiomeHasValidResult = false;
            }
        } else {
            MapRenderState::hoverBiomeTargetAlpha = 0.0f;
            MapRenderState::hoverBiomeHasValidResult = false;
        }
    } else if (!canQuery) {
        MapRenderState::hoverBiomeTargetAlpha = 0.0f;
        MapRenderState::hoverBiomeHasValidResult = false;
    }

    // alpha 平滑过渡
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.1f) dt = 0.1f;
    float lerpF = std::clamp(12.0f * dt, 0.0f, 1.0f);
    MapRenderState::hoverBiomeAlpha +=
        (MapRenderState::hoverBiomeTargetAlpha - MapRenderState::hoverBiomeAlpha) * lerpF;

    // 绘制（仅 alpha 足够时）
    if (MapRenderState::hoverBiomeAlpha > 0.01f && MapRenderState::hoverBiomeHasValidResult) {
        std::string combined = MapRenderState::hoverBiomeRawName + " (" +
                               MapRenderState::hoverBiomeTranslatedName + ")";
        char biomeBuf[512];
        snprintf(biomeBuf, sizeof(biomeBuf), LanguageManager::GetText("BIOME_LABEL"), combined.c_str());
        ImVec2 bts = mFont->CalcTextSizeA(mFontSize, FLT_MAX, 0.0f, biomeBuf);
        ImU32 bgCol = IM_COL32(0, 0, 0, (ImU32)(180 * MapRenderState::hoverBiomeAlpha));
        ImU32 textCol = IM_COL32(180, 255, 180, (ImU32)(255 * MapRenderState::hoverBiomeAlpha));
        draw_list->AddRectFilled(
            ImVec2(io.DisplaySize.x/2 - bts.x/2 - 20, 15),
            ImVec2(io.DisplaySize.x/2 + bts.x/2 + 20, 15 + bts.y + 15),
            bgCol, 5.0f);
        draw_list->AddText(mFont, mFontSize,
            ImVec2(io.DisplaySize.x/2 - bts.x/2, 22), textCol, biomeBuf, NULL, 0.0f, NULL);
    }
}
```

### 不修改的部分
- **小地图生物群系**（`DX11Hook.h:853-857`）：继续用 `translatedBiomeName`（玩家当前生物群系）
- **玩家生物群系更新**（`PlayerHook.h:616-628`）：继续更新 `rawBiomeName`/`translatedBiomeName`

## 复用的现有函数
- `TranslateBiomeName`（`PlayerHook.h:71`）— 生物群系名翻译
- `SafeGetSurfaceY`（`PlayerHook.h:371`）— SEH 地表高度查询（参考模式）
- `MapCacheManager::GetCachedSurfaceHeight`（`MapCacheManager.h:55`）— 缓存地表高度
- `MapCacheManager::GetRegionHash`（`MapCacheManager.h:32`）— region 哈希
- `LanguageManager::GetText("BIOME_LABEL")` — 语言键复用

## 边界情况处理

| 场景 | 处理 |
|------|------|
| 鼠标离开画布 | `isHoveringCanvas=false` → targetAlpha=0 → 淡出 |
| 鼠标在未渲染 region | `g_regionSRVs` 查无 → targetAlpha=0 |
| 区块未加载/部分加载 | `SafeGetBiomeName` catch(...) → false → targetAlpha=0 |
| 地狱维度 | `currentDimensionId==1` → canQuery=false → 隐藏 |
| 大地图刚打开 | 重置 alpha=0 + 强制查询 → 淡入 |
| 拖动地图 | 鼠标世界坐标变 → movedEnough 触发查询 |
| 缩放地图 | 鼠标世界坐标变 → movedEnough 触发查询 |

## 验证

编译：`& "C:\Users\chiyan\scoop\shims\xmake.exe" f -m release -c; & "C:\Users\chiyan\scoop\shims\xmake.exe" -r`

测试场景：
1. 开大地图，鼠标悬停不同生物群系 → 名称随鼠标变化
2. 鼠标移到未探索区域 → 生物群系名称平滑淡出
3. 鼠标移出画布 → 淡出
4. 进地狱 → 不显示生物群系
5. 缩放/拖动地图 → 生物群系正确更新
6. 小地图下方生物群系 → 仍显示玩家当前生物群系（不受影响）
7. 鼠标快速划过 → 无崩溃，过渡平滑

---

## 实现结果

### 编译验证

- **编译命令**：`xmake f -m release -c && xmake -r`
- **编译结果**：✅ build ok, spent 22.531s
- **警告数量**：0（`/EHa /W4` 严格警告级别下零警告）
- **输出产物**：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`
- **DLL 大小**：1,125,888 字节（~1099 KB）
- **增量变化**：+1,536 字节（vs 上次 1,124,352），来自新增 hoverBiome* 字段、SafeGetBiomeName 函数、扩展的渲染逻辑

### 实际文件改动

| 文件 | 改动 | 行号 |
|------|------|------|
| `src/state/MapRenderState.h` | 新增 8 个 `hoverBiome*` inline 字段 | 19-27 |
| `src/hooks/PlayerHook.h` | 新增 `SafeGetBiomeName` 函数（try/catch 包装） | 384-400 |
| `src/hooks/DX11Hook.h` | 替换原 8 行玩家生物群系显示为 100 行鼠标悬停逻辑 | 1415-1514 |

### 关键设计实现确认

1. **鼠标位置检测**（需求1）：复用既有 `hoverWx`/`hoverWz`（DX11Hook.h:1397-1398），通过 `bigMapZoom`/`bigMapOffsetX/Z` 反算世界坐标
2. **生物群系数据查询**（需求2）：`SafeGetBiomeName` 通过 `getBiome(BlockPos)` → `mHash->getString()` 获取，与 PlayerHook.h:616 既有模式一致
3. **已渲染区域检查**（需求3）：`g_regionSRVs.find(hash)` 查找非空 SRV，未渲染 → targetAlpha=0
4. **小地图不受影响**（需求4）：小地图显示（DX11Hook.h:853-857）继续用 `translatedBiomeName`（玩家当前生物群系），新增字段完全独立
5. **显示位置优化**（需求5）：保持原顶部居中位置（y=15-50），不遮挡底部坐标显示（y=DisplaySize.y-25）和右侧侧边栏（x=DisplaySize.x-240）
6. **平滑过渡**（需求6）：`alpha += (target-alpha) * clamp(12*dt, 0, 1)` 帧率无关 lerp，背景/文字 alpha 同步缩放
7. **多场景测试**（需求7）：节流策略（帧计数≥6 或位移>4 格）+ 边界处理（鼠标离开/未渲染/未加载/地狱维度）

### 安全性

- **部分加载区块 AV 防护**：`SafeGetBiomeName` 用 `try/catch(...)`（/EHa 下可捕获 SEH AV），与 `SafeGetSurfaceY` 的 `__try/__except` 互补
- **`__try/__except` 限制规避**：因 `getBiome` 返回 `Biome const&` 需 `std::string` 赋值（含析构），不能用 SEH，改用 C++ 异常机制
- **大地图开启重置**：`static wasBigMapOpen` 检测开启瞬间，重置 alpha=0 实现淡入，避免残留显示
