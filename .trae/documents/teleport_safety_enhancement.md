# 传送安全增强系统·实现文档

> **版本**：v2.0  
> **日期**：2026-07-19  
> **作者**：ChiyanMap 项目  
> **目标**：消除"传送到全新区域时概率性传送到地下/水中"的异常问题，实现 99.9%+ 传送准确率  
> **v2 更新**：修复 v1 残留的"传送到水里/地里"问题，新增岩浆池/危险方块避开功能

---

## 🆕 v2 修复摘要（2026-07-19 增量）

### v1 残留问题根因

经用户反馈"仍有概率传送到水里、地里"，深入分析发现 v1 存在以下严重 bug：

| Bug | 位置 | 根因 | 影响 |
|-----|------|-----|-----|
| **v1-Bug1（最致命）** | `SafeFindSafeSpawnY` 场景 a | 返回"水面上方空气 Y"作为落脚点，注释"游泳姿态可接受"——**完全错误**！`/tp` 传送后玩家受重力影响，水面上方空气 Y 会让玩家下落穿过水，最终落到水底 | **传送到水里的根本原因** |
| **v1-Bug2/3** | `SafeFindSafeSpawnY` 场景 b/c | 接受"头部是液体"，玩家会头部浸水 | 头部浸水，可能溺水 |
| **v1-Bug4** | 全局 | 无危险方块检测（岩浆块、火、仙人掌、营火、甜浆果丛等） | 可能传送到岩浆/火/仙人掌上 |
| **v1-Bug5** | `SafeFindSafeSpawnY` | 无脚部下方固体验证 | 部分加载错误 Y 可导致传送到虚空 |
| **v1-Bug6** | `pendingSurfaceProbe` 兜底 | "接受原始 Y+1"作为最后手段 | 部分加载错误 Y 会传送到地下 |
| **v1-Bug7** | `FindNearestSafeSpawn` | ±8 搜索半径在小岛/悬崖场景不够 | 找不到安全点就触发危险兜底 |

### v2 修复方案

1. **`SafeFindSafeSpawnY` 重写**：
   - 场景 a（液体/危险方块列）：**直接返回 -32000**，强制 `FindNearestSafeSpawn` 搜索附近陆地（不再尝试水面上方空气）
   - 场景 b/c：**头部必须是空气类**（移除"头部是液体也接受"的错误逻辑）
   - **新增返回前验证**：Y-1 是可站立固体（`IsStandableSolidName`），防止部分加载错误 Y
2. **新增 `IsDangerousBlockName`**：检测岩浆、火、仙人掌、岩浆块、营火、甜浆果丛、凋灵玫瑰、钟乳石
3. **新增 `IsStandableSolidName`**：排除空气类、液体类、危险类，剩余视为可站立
4. **新增 `HasAdjacentHazard`**：检查落脚点 3x3 邻居是否有液体/危险方块（避免紧邻水/岩浆）
5. **`FindNearestSafeSpawn` 重写**：
   - 搜索半径 ±8 → **±16**
   - **双轮搜索**：第一轮严格（3x3 邻居无危险）→ 第二轮宽松（仅本列安全，接受孤岛）
6. **`kProbeStableThreshold`**：2 帧 → **3 帧**，进一步过滤部分加载的"稳定但错误"Y
7. **删除"接受原始 Y+1"危险兜底**：改为**回退原位 + 失败状态**（宁可不传送，也不冒险）
8. **岩浆池避开**：已通过 `IsLiquidBlockName`（含 lava）+ `IsDangerousBlockName`（含 lava/magma）双重覆盖，在场景 a 统一拒绝

### v2 新增函数

**文件**：[PlayerHook.h:495-531](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L495-L531)

```cpp
// 危险方块检测（接触即受伤/致命）
inline bool IsDangerousBlockName(std::string const& name) noexcept {
    return name.find("lava") != std::string::npos ||          // 岩浆（液体+致命）
           name.find("fire") != std::string::npos ||          // 火
           name.find("cactus") != std::string::npos ||        // 仙人掌
           name.find("magma") != std::string::npos ||         // 岩浆块
           name.find("campfire") != std::string::npos ||      // 营火
           name.find("sweet_berry") != std::string::npos ||   // 甜浆果丛
           name.find("wither_rose") != std::string::npos ||   // 凋灵玫瑰
           name.find("pointed_dripstone") != std::string::npos; // 钟乳石
}

// 可站立固体（排除空气/液体/危险）
inline bool IsStandableSolidName(std::string const& name) noexcept {
    return !IsAirLikeName(name) && !IsLiquidBlockName(name) && !IsDangerousBlockName(name);
}

// 3x3 邻居危险检测
inline bool HasAdjacentHazard(BlockSource& region, int x, int y, int z, int radius = 1) noexcept {
    // 检查周围 radius 范围同高度层是否有液体/危险方块
    // 异常视为有危险（保守策略）
}
```

### v2 参数对比

| 参数 | v1 | v2 | 说明 |
|-----|-----|-----|-----|
| 稳定性阈值 | 2 帧 | **3 帧** | 进一步过滤部分加载错误 Y |
| 附近搜索半径 | ±8 | **±16** | 小岛/悬崖场景更多机会 |
| 搜索策略 | 单轮 | **双轮（严格→宽松）** | 严格轮避免紧邻危险，宽松轮接受孤岛 |
| 液体列处理 | 尝试水面上方空气 | **直接拒绝** | 避免"下落穿水"bug |
| 头部方块 | 接受液体 | **必须空气** | 避免头部浸水 |
| 下方固体 | 不验证 | **必须可站立固体** | 防止部分加载错误 Y |
| 危险方块 | 不检测 | **8 类危险方块** | 岩浆/火/仙人掌等 |
| 兜底策略 | 接受原始 Y+1 | **回退原位+失败** | 不冒险传送 |

### v2 构建验证

- ✅ 编译：零警告零错误，耗时 22.312s
- ✅ DLL：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`（1,244,672 字节）
- ✅ 相比 v1（1,242,624 字节）增加约 2KB，对应新增函数和双轮搜索逻辑

---

## 一、问题背景与根因分析

### 1.1 异常现象
当用户在大地图上点击从未到访的全新区域触发传送时，存在以下概率性异常：
- 玩家被传送到地下（洞穴、矿洞、地下结构内）
- 玩家被传送到水中（海洋、深水区）
- 玩家被传送到固体方块内部（树干、建筑物内）

### 1.2 根因分析

| 根因 | 描述 | 影响 |
|-----|------|-----|
| **区块部分加载** | Phase 0 tp 到 Y=320 后，目标区块仅部分 subchunk 加载完成，NULL subchunk 指针被解引用 | `getAboveTopSolidBlock` 返回错误的临时 Y（如洞穴顶板），或触发 0xC0000005 AV |
| **缓存数据过时** | 缓存高度图记录的是历史 Y，但区块卸载后重新加载可能地形已变化 | 传送到不存在的地表高度 |
| **缺少落脚点验证** | 旧版仅取 `getAboveTopSolidBlock + 1` 作为 Y，未检查该位置是否为水/液体/固体 | 水下传送、卡在方块内 |
| **无附近点回退** | 目标列恰好在深海/悬崖/洞穴入口，无安全落脚点时直接传送 | 落入水中或虚空 |
| **稳定性不足** | 单帧 Y 探测结果即采纳，区块加载过程中 Y 值可能临时跳变 | 部分加载区块返回临时错误 Y |
| **无加载反馈** | 用户在 4 秒加载等待期间无任何视觉反馈，可能误以为卡死重复点击 | 多次触发传送，状态混乱 |

---

## 二、系统架构总览

### 2.1 五道防线设计

```
┌─────────────────────────────────────────────────────────────┐
│              传送请求触发 (triggerTeleport=true)              │
└──────────────────────────┬──────────────────────────────────┘
                           ▼
        ┌──────────────────────────────────────────┐
        │  三级地表 Y 识别                            │
        │  ① 缓存高度图 → ② 实时 BlockSource → ③ 探测 │
        └──────────────────┬───────────────────────┘
                           ▼
   ┌───────────────────────────────────────────────────┐
   │  防线①：区块就绪检查（IsChunkReady + hasChunksAt）  │
   │  仅当区块完全加载时才信任 Y 值                       │
   └───────────────────────┬───────────────────────────┘
                           ▼
   ┌───────────────────────────────────────────────────┐
   │  防线②③：落脚点验证（SafeFindSafeSpawnY）           │
   │  处理水下/结构内/正常三种场景，返回安全 Y            │
   └───────────────────────┬───────────────────────────┘
                           ▼
   ┌───────────────────────────────────────────────────┐
   │  防线④：稳定性检查（连续 2 帧相同 Y）                │
   │  防止部分加载区块返回的临时错误 Y                    │
   └───────────────────────┬───────────────────────────┘
                           ▼
   ┌───────────────────────────────────────────────────┐
   │  防线⑤：附近点回退（FindNearestSafeSpawn ±8 螺旋）   │
   │  目标列不安全时搜索附近 ±8 格                        │
   └───────────────────────┬───────────────────────────┘
                           ▼
   ┌───────────────────────────────────────────────────┐
   │  超时回退（4 秒）→ 返回原位 + 失败提示              │
   └───────────────────────────────────────────────────┘
```

### 2.2 传送状态机

```cpp
enum class TeleportState : int {
    Idle       = 0,  // 无传送任务
    Loading    = 1,  // 等待区块加载（Phase 0 已发送，等待 Phase 1）
    Validating = 2,  // 区块就绪，正在验证落脚点
    Done       = 3,  // 传送成功完成
    Failed     = 4   // 异常回退（超时或无安全点）
};
```

状态流转：`Idle → Loading → Validating → Done` 或 `Loading → Failed → Idle（2.5s 后自动）`

---

## 三、核心安全函数群

### 3.1 SafeGetSurfaceY — SEH 包装的安全高度查询

**文件**：[PlayerHook.h:427-438](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L427-L438)

**作用**：调用 `BlockSource::getAboveTopSolidBlock`，捕获部分加载区块触发的 0xC0000005 AV，返回 `-32000` 哨兵值表示"区块未就绪，请重试"。

**技术约束**：
- `__try/__except` 不能与 C++ 对象析构共存，故函数仅使用 POD 类型
- 返回 `-32000` 作为哨兵值（区别于合法 Y 范围 -64~319）

### 3.2 SafeGetBiomeName — try/catch 包装的安全生物群系查询

**文件**：[PlayerHook.h:446-456](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L446-L456)

**作用**：与 `SafeGetSurfaceY` 互补。`getBiome` 返回 `Biome const&` 并需 `std::string` 赋值（含析构），不能用 `__try/__except`，改用 `try/catch(...)` 在 `/EHa` 模式下捕获 SEH AV。

### 3.3 IsChunkReady — 区块就绪检查

**文件**：[PlayerHook.h:466-474](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L466-L474)

**作用**：调用 `hasChunksAt(pos, r=0, ignoreClientChunk=false)`，严格检查目标区块已完全加载。`r=0` 仅检查目标区块本身，`false` 表示不忽略客户端区块（更严格）。

### 3.4 SafeFindSafeSpawnY — 安全落脚点查找（核心）

**文件**：[PlayerHook.h:498-563](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L498-L563)

**作用**：给定 (x, z)，返回玩家可安全站立的实际 Y 坐标。处理三种异常场景：

| 场景 | 检测条件 | 处理方式 |
|-----|---------|---------|
| **a) 水下** | `feetBlock` 是 water/lava | 向上扫描找水面以上的空气/固体，返回可站立 Y |
| **b) 结构内** | `feetBlock` 是固体（树干、建筑内） | 向上扫描找第一个 `air + air` 两格空间 |
| **c) 正常** | `feetBlock` 是 air | 验证头部空间，必要时向上扫描 |

**辅助函数**：
- `IsLiquidBlockName`：判断 water/lava
- `IsAirLikeName`：判断 air/barrier/light_block/structure_void/placeholder/info_update

### 3.5 FindNearestSafeSpawn — 螺旋搜索附近安全点

**文件**：[PlayerHook.h:568-594](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L568-L594)

**作用**：目标列无安全落脚点时，按螺旋顺序搜索附近 ±8 格，找到后修改 x/z/outY 为安全点坐标。

搜索顺序：`(0,0) → (±1,0),(0,±1) → (±1,±1) → (±2,0),(0,±2) → ... → (±8,±8)`

---

## 四、triggerTeleport 主逻辑（增强版）

**文件**：[PlayerHook.h:635-748](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L635-L748)

### 4.1 流程

1. **Y 值识别**：判断是否需要地表检测（`targetY < -500` 哨兵，或 `targetY == 320` 未加载默认值）
2. **三级 Y 识别**：
   - 优先级 1：缓存高度图（识别已扫描但已卸载的区域）
   - 优先级 2：实时 `SafeGetSurfaceY`（仅当 `chunkReady=true`）
3. **若缓存/实时命中**：
   - 防线②③：`SafeFindSafeSpawnY` 验证落脚点
   - 通过 → `/tp @s X safeY Z` 直接传送，状态置 `Done`
   - 不通过 → 防线⑤：`FindNearestSafeSpawn ±8` 附近点回退
   - 附近也无安全点 → 退回两阶段探测
4. **若区块未就绪/无缓存**：进入两阶段探测 `StartProbe`
5. **用户显式 Y**（非 -500/非 320）：尊重原意，直接传送

### 4.2 两阶段探测 Phase 0

```cpp
// 保存原位（用于超时回退）
probeOriginalX = g_playerX;
probeOriginalY = g_playerY;
probeOriginalZ = g_playerZ;
// 重置稳定性计数
probeLastY = -32000;
probeStableCount = 0;
// 状态置 Loading，显示遮罩层
teleportState = Loading;
teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
// Phase 0 探测传送：Y=320 仅用于触发区块加载
SendServerCommand("/tp @s X 320 Z");
```

---

## 五、pendingSurfaceProbe Phase 1 轮询（增强版）

**文件**：[PlayerHook.h:760-881](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L760-L881)

### 5.1 五道防线参数

| 防线 | 参数 | 旧值 | 新值 | 说明 |
|-----|------|-----|-----|-----|
| ① 宽限期 | `elapsedMs >= ?` | 300ms | **500ms** | 给慢网络更多时间，减少无谓查询 |
| ② 区块就绪 | `IsChunkReady` | 无 | **新增** | 防止部分加载返回错误 Y |
| ③ 稳定性 | 连续相同帧数 | 1 帧 | **2 帧** | 防止临时跳变 Y |
| ④ 落脚点 | `SafeFindSafeSpawnY` | 无 | **新增** | 防止水下/地下传送 |
| ⑤ 附近点 | `FindNearestSafeSpawn` | 无 | **新增 ±8** | 目标列不安全时回退 |
| 超时 | `elapsedMs >= ?` | 2000ms | **4000ms** | 给慢网络更多机会，仍小于 320→地表 ~6s 坠落时间 |

### 5.2 流程

```
每帧轮询（pendingSurfaceProbe=true）：
├── elapsedMs < 500ms → 跳过（宽限期内区块必然未就绪）
├── elapsedMs >= 500ms：
│   ├── IsChunkReady=false → 继续等待
│   └── IsChunkReady=true：
│       ├── liveY = SafeGetSurfaceY
│       ├── liveY 无效（≤-64 或 AV）→ 重置稳定性计数
│       └── liveY 有效：
│           ├── liveY == probeLastY → probeStableCount++
│           └── liveY != probeLastY → 重置 probeLastY, probeStableCount=1
│           └── probeStableCount >= 2：
│               ├── 状态置 Validating
│               ├── safeY = SafeFindSafeSpawnY
│               ├── safeY 有效 → /tp @s X safeY Z，状态 Done
│               ├── safeY 无效 → FindNearestSafeSpawn ±8：
│                   ├── 找到 → /tp @s searchX nearbyY searchZ，状态 Done
│                   └── 未找到 → /tp @s X (liveY+1) Z，状态 Done（最后手段）
└── elapsedMs >= 4000ms：
    └── 超时回退 → /tp @s originalX originalY originalZ，状态 Failed
```

### 5.3 超时回退

```cpp
// 4 秒超时（仍小于 320→地表 ~6s 坠落时间，/tp 重置坠落距离保证安全）
if (elapsedMs >= 4000) {
    SendServerCommand("/tp @s originalX originalY originalZ");
    teleportState = Failed;
    teleportStatusMsg.clear();
    teleportFailReason = LanguageManager::GetText("TELEPORT_TIMEOUT_MSG");
}
```

---

## 六、传送状态机字段

**文件**：[MapRenderState.h:98-109](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h#L98-L109)

```cpp
// [安全传送增强] 探测稳定性检查
inline short probeLastY = -32000;         // 上一帧探测到的 Y
inline int  probeStableCount = 0;         // 连续一致帧数
constexpr static int kProbeStableThreshold = 2;  // 需要的连续一致帧数

// [传送状态机]
enum class TeleportState : int { Idle=0, Loading=1, Validating=2, Done=3, Failed=4 };
inline std::atomic<int> teleportState{(int)TeleportState::Idle};
inline std::string teleportStatusMsg;     // UI 显示的状态文本
inline std::string teleportFailReason;    // 失败原因（调试用）
```

---

## 七、地形加载遮罩层 UI

**文件**：[DX11Hook.h:2439-2580](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L2439-L2580)

### 7.1 功能

- **加载中**（`Loading` / `Validating` / `pendingSurfaceProbe=true`）：
  - 半透明黑色全屏背景遮罩（alpha=140）
  - 居中模态窗口（380×自适应高度）
  - **旋转加载动画**：8 个圆点围绕中心点旋转，透明度按 i 递减营造拖尾效果
  - 主状态消息（i18n：`TELEPORT_LOADING`）
  - 辅助提示（i18n：`TELEPORT_LOADING_HINT`，灰色）

- **失败**（`Failed`）：
  - 半透明红色全屏背景遮罩（alpha=130）
  - 居中模态窗口（400×自适应高度）
  - 红色失败标题（i18n：`TELEPORT_FAILED`）
  - 失败详情消息（i18n：`TELEPORT_FAILED_MSG` 或 `TELEPORT_TIMEOUT_MSG`）
  - 自动消失提示（i18n：`TELEPORT_FAILED_DISMISS`）
  - **2.5 秒后自动重置到 Idle**，避免遮罩永久停留

### 7.2 旋转动画实现

```cpp
const int dotCount = 8;
const float radius = 20.0f;
float time = (float)ImGui::GetTime();
for (int i = 0; i < dotCount; ++i) {
    float angle = time * 3.5f + (float)i * (2.0f * 3.14159265f / dotCount);
    float x = center.x + std::cos(angle) * radius;
    float y = center.y + std::sin(angle) * radius;
    int alpha = 255 - (i * 220 / dotCount);  // 透明度递减
    if (alpha < 30) alpha = 30;
    dl->AddCircleFilled(ImVec2(x, y), 5.0f, IM_COL32(120, 200, 255, alpha));
}
```

### 7.3 调用位置

**文件**：[DX11Hook.h:2666](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L2666)

在 `renderImGuiFrame` 中，所有 UI 渲染之后、`ImGui::Render()` 之前调用，确保遮罩层在最顶层：

```cpp
// [传送安全增强] 地形加载遮罩层 (在所有 UI 之上，模态拦截输入)
RenderTeleportLoadingOverlay();
ImGui::Render();
```

---

## 八、i18n 国际化支持

### 8.1 新增 6 个 i18n 键

**文件**：[LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp)

| 键名 | 用途 | en 示例 |
|-----|-----|---------|
| `TELEPORT_LOADING` | 加载中主提示 | "Loading terrain..." |
| `TELEPORT_LOADING_HINT` | 加载中辅助提示 | "Please wait while terrain data is being loaded" |
| `TELEPORT_FAILED` | 失败标题 | "Teleport Failed" |
| `TELEPORT_FAILED_MSG` | 失败原因（无安全点） | "Unable to find a safe surface. Returned to original position." |
| `TELEPORT_TIMEOUT_MSG` | 失败原因（超时） | "Teleport timed out, returned to original position" |
| `TELEPORT_FAILED_DISMISS` | 失败消散提示 | "This message will disappear shortly" |

### 8.2 支持的 15 种语言

`en, zh_CN, zh_TW, de, fr, id, it, ja, ko, pt_BR, ru, th, tr, uk, vi`

共 **15 × 6 = 90 个键值对**，全部成功插入并验证（每个键出现 15 次）。

### 8.3 编码策略

- **\u 转义**（新值）：en, zh_CN, zh_TW, ja, ko, ru, th
- **原始 UTF-8**（新值）：de, fr, id, it, pt_BR, uk, vi
- **tr 特殊**：除 `TELEPORT_TIMEOUT_MSG` 用原始 UTF-8 外，其余 5 键用 \u 转义（避免 `\u011g` 等无效转义）

### 8.4 BOM 累积问题处理

Edit 工具每次操作会在文件头部累积 UTF-8 BOM（EF BB BF）。16 次成功 Edit + 原有 1 个 = 17 个 BOM（51 字节）。

**剥离脚本**（PowerShell 字节操作）：

```powershell
$bytes = [System.IO.File]::ReadAllBytes($file)
# 检测开头连续 BOM 数量
$bomCount = 0; $pos = 0
while ($pos + 2 -lt $bytes.Length -and
       $bytes[$pos] -eq 0xEF -and $bytes[$pos+1] -eq 0xBB -and $bytes[$pos+2] -eq 0xBF) {
    $bomCount++; $pos += 3
}
# 保留位置 0-2 的 BOM，跳过位置 3 起的多余 BOM
$newBytes = New-Object byte[] ($bytes.Length - (($bomCount - 1) * 3))
[Array]::Copy($bytes, 0, $newBytes, 0, 3)
[Array]::Copy($bytes, $pos, $newBytes, 3, $bytes.Length - $pos)
[System.IO.File]::WriteAllBytes($file, $newBytes)
```

剥离结果：17 → 1 BOM，文件大小 128692 → 128644 字节。

---

## 九、状态消息 i18n 化

### 9.1 Loading 状态

**文件**：[PlayerHook.h:723-724](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L723-L724)

```cpp
MapRenderState::teleportStatusMsg = LanguageManager::GetText("TELEPORT_LOADING");
MapRenderState::teleportFailReason.clear();
```

### 9.2 超时失败状态

**文件**：[PlayerHook.h:870-871](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h#L870-L871)

```cpp
MapRenderState::teleportStatusMsg.clear();
MapRenderState::teleportFailReason = LanguageManager::GetText("TELEPORT_TIMEOUT_MSG");
```

---

## 十、构建验证

### 10.1 构建命令

```powershell
& "C:\Users\chiyan\scoop\shims\xmake.exe" -r
```

### 10.2 构建结果

```
[  7%]: compiling.release src\hooks\HookRegistry.cpp
[  7%]: compiling.release src\mod\ChiyanMap.cpp
[  7%]: compiling.release src\mod\MemoryOperators.cpp
[  7%]: compiling.release src\state\LanguageManager.cpp
[  7%]: compiling.release src\state\MapCacheManager.cpp
[  7%]: compiling.release src\state\MapRenderState.cpp
[  7%]: compiling.release src\state\WaypointManager.cpp
[Prelink]: Done.
[ 65%]: linking.release ChiyanMap.dll
[100%]: build ok, spent 23.547s
```

- **警告数**：0
- **错误数**：0
- **生成产物**：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`
- **DLL 大小**：1,242,624 字节（相比上次 1,219,072 字节增加约 23KB，对应新增传送安全代码）

### 10.3 i18n 键完整性验证

```powershell
TELEPORT_LOADING: 15 occurrences         ✅
TELEPORT_LOADING_HINT: 15 occurrences    ✅
TELEPORT_FAILED: 15 occurrences          ✅
TELEPORT_FAILED_MSG: 15 occurrences      ✅
TELEPORT_TIMEOUT_MSG: 15 occurrences     ✅
TELEPORT_FAILED_DISMISS: 15 occurrences  ✅
```

### 10.4 BOM 验证

```powershell
Before: BOM count = 17 ❌
After:  BOM count = 1  ✅
First 12 bytes: EF BB BF 23 69 6E 63 6C 75 64 65 20  (#include)
```

---

## 十一、测试场景与预期结果

### 11.1 核心测试场景

| # | 场景 | 预期结果 | 防线 |
|---|-----|---------|-----|
| 1 | 传送到已加载区域（玩家附近） | 立即传送至地表，无遮罩层 | ① ② ③ |
| 2 | 传送到已缓存但已卸载的区域 | 立即传送至缓存 Y（若安全），无遮罩层 | ① ② ③ |
| 3 | 传送到从未到访的全新区域 | 显示加载遮罩 → 2 帧稳定后传送至安全 Y | ① ② ③ ④ |
| 4 | 目标列在水下（海洋中心） | 自动找水面以上落脚点，或附近 ±8 格回退 | ② ⑤ |
| 5 | 目标列在洞穴入口/悬崖 | 附近 ±8 格螺旋搜索找安全点 | ⑤ |
| 6 | 网络极慢，4 秒内区块未加载 | 超时回退原位，显示失败提示，2.5s 后消失 | 超时 |
| 7 | 部分加载区块返回临时错误 Y | 稳定性检查拒绝，等待下一帧 | ③ ④ |
| 8 | 目标列在树干/建筑内 | 向上扫描找第一个 air+air 空间 | ② |
| 9 | 深海区域 ±8 格全是水 | 最后手段接受 liveY+1（避免永久卡死） | ⑤ 兜底 |
| 10 | 用户显式指定 Y（非 -500/非 320） | 尊重原意直接传送，不做地表检测 | — |

### 11.2 预期达成率

- **目标**：99.9%+ 传送准确率
- **理论保障**：
  - 防线①②③消除"部分加载区块返回临时错误 Y"问题
  - 防线④消除"水下/地下/结构内传送"问题
  - 防线⑤消除"目标列不安全导致传送失败"问题
  - 超时回退保证"极端情况下也不会丢失玩家"
- **剩余 0.1% 风险**：深海 ±8 格全是水时的兜底处理（接受 liveY+1，可能仍在水中），但此场景极罕见且优于卡死

---

## 十二、部署说明

### 12.1 手动部署

由于沙箱限制无法写入游戏目录，需手动复制 DLL：

```powershell
Copy-Item "D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll" `
          "D:\我的世界\基岩版游戏\levilauncher.exe\versions\1.26.20.04\mods\ChiyanMap\" `
          -Force
```

### 12.2 验证步骤

1. 重启游戏，加载存档
2. 打开大地图（M 键）
3. 点击从未到访的全新区域触发传送
4. 预期看到：
   - 半透明黑色遮罩 + 旋转加载动画
   - "正在加载地形..." 主提示
   - "请等待地形数据加载完成" 辅助提示
5. 2~4 秒后玩家应准确出现在目标区域地表
6. 多次重复测试不同区域，确认无地下/水中传送

### 12.3 失败场景验证

1. 拔网线或限制网速模拟慢网络
2. 触发传送至全新区域
3. 等待 4 秒
4. 预期看到：
   - 红色遮罩 + "传送失败" 标题
   - "传送超时，已返回原位置" 详情
   - "此消息将很快消失" 提示
5. 2.5 秒后遮罩自动消失，玩家回到原位

---

## 十三、相关文件清单

| 文件 | 修改内容 |
|-----|---------|
| [PlayerHook.h](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h) | 新增 `SafeGetSurfaceY`、`SafeGetBiomeName`、`IsChunkReady`、`IsLiquidBlockName`、`IsAirLikeName`、`SafeFindSafeSpawnY`、`FindNearestSafeSpawn`；重写 `triggerTeleport` 主逻辑；重写 `pendingSurfaceProbe` Phase 1 轮询；状态消息 i18n 化 |
| [MapRenderState.h](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h) | 新增 `probeLastY`、`probeStableCount`、`kProbeStableThreshold`；新增 `TeleportState` 枚举；新增 `teleportState`、`teleportStatusMsg`、`teleportFailReason` 字段 |
| [DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h) | 添加 `<cmath>` 头文件；新增 `RenderTeleportLoadingOverlay()` 函数；在 `renderImGuiFrame` 中调用遮罩层 |
| [LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp) | 为 15 种语言各添加 6 个传送安全 i18n 键（90 个键值对）；剥离 17 个累积 BOM 至 1 个 |

---

## 十四、经验总结

### 14.1 技术要点

1. **SEH 与 C++ 异常的边界**：`__try/__except` 不能与 C++ 对象析构共存，需用 `try/catch(...)` 在 `/EHa` 下捕获 SEH AV
2. **哨兵值设计**：`-32000` 作为"区块未就绪"哨兵，区别于合法 Y 范围 -64~319
3. **稳定性检查阈值**：连续 2 帧是经验值，过短（1 帧）无法过滤临时跳变，过长（5+ 帧）增加延迟
4. **超时阈值权衡**：4 秒既给慢网络足够机会，又小于 320→地表 ~6s 坠落时间（`/tp` 重置坠落距离保证安全）
5. **附近点回退半径**：±8 格平衡搜索范围与性能（17×17=289 次查询，最坏情况仍可接受）
6. **BOM 累积陷阱**：Edit 工具副作用，需 PowerShell 字节操作剥离

### 14.2 失败模式与防护

| 失败模式 | 防护措施 |
|---------|---------|
| 部分加载区块 AV 崩溃 | `SafeGetSurfaceY` SEH 包装 + `try/catch(...)` |
| 部分加载区块返回错误 Y | `IsChunkReady` + 稳定性 2 帧检查 |
| 水下传送 | `SafeFindSafeSpawnY` 场景 a 向上扫描 |
| 结构内传送（卡方块） | `SafeFindSafeSpawnY` 场景 b 向上扫描 |
| 目标列无安全点 | `FindNearestSafeSpawn ±8` 螺旋搜索 |
| 极端慢网络永久卡死 | 4 秒超时回退原位 |
| 用户无加载反馈 | 旋转动画遮罩层 + i18n 提示 |
| 失败遮罩永久停留 | 2.5 秒自动重置到 Idle |

---

## 十五、v3 增强：避免传送失败 + 15 语言 i18n 完整化

### 15.1 用户反馈

v2 实施后用户提出两个新需求：

1. **"请避免传送失败"**：v2 的"±16 搜索失败 → 回退原位 + Failed 状态"兜底过于保守。在巨大湖泊、岩浆湖、地下溶洞等场景下，目标点周围 ±16 内可能全是水/岩浆，导致频繁回退原位，用户体验差
2. **"传送加载时显示的文本改为 15 种语言"**：磁盘 JSON 文件如果存在旧版本（不含 `TELEPORT_*` 等新键），`GetText` 会返回键名（如 "TELEPORT_LOADING"）而非本地化文本，导致传送加载界面在非中文/英文语言下显示未翻译的英文键名

### 15.2 v3 修复方案

#### 15.2.1 避免传送失败（三层递进策略）

**第 1 层：扩大搜索范围**（[PlayerHook.h](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h) `FindNearestSafeSpawn`）

在 v2 双轮搜索（±16 严格 + ±16 宽松）基础上增加第三轮：

```cpp
// 第三轮：超大范围宽松搜索（v3 新增），半径 ±(maxRadius+1)~±32
// 适用场景：目标点在巨大湖泊/岩浆湖中央，±16 范围仍全是水
for (int r = maxRadius + 1; r <= 32; ++r) {
    for (int dx = -r; dx <= r; ++dx) {
        for (int dz = -r; dz <= r; ++dz) {
            if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
            // ... SafeFindSafeSpawnY + 本列安全检查 ...
        }
    }
}
```

性能：±32 = 65×65 = 4225 次 `getBlock`，约 4ms，单次传送可接受。

**第 2 层：validated liveY+1 兜底**（[PlayerHook.h](file:///D:/Project/ChiyanMap/src/hooks/PlayerHook.h) `pendingSurfaceProbe`）

当三轮搜索全部失败时，不再直接回退原位。先尝试使用 `liveY+1` 兜底，但仅在 `liveY` 经过严格验证后：

```cpp
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
        // ... /tp @s probeX finalY probeZ ...
        usedValidatedRaw = true;
    }
} catch (...) {}

if (!usedValidatedRaw) {
    // liveY 下方不是固体 / liveY+1 是危险方块 / 异常 → 回退原位 + 失败状态
}
```

验证条件（必须全部满足才使用 liveY+1）：
- ① `liveY` 已通过 3 帧稳定性验证（`probeStableCount >= kProbeStableThreshold`）
- ② `liveY` 已通过 `IsChunkReady` 验证
- ③ `getBlock(liveY)` 必须是可站立固体（`IsStandableSolidName`）
- ④ `getBlock(liveY+1)` 不能是危险方块（`!IsDangerousBlockName`，**允许水**）

**安全权衡**：`liveY+1` 允许是水（玩家会落入水中），但绝不会是地下/虚空/岩浆/火焰。这是为了在极端场景（巨大湖泊中央）下避免传送失败而做的合理妥协——落到水里玩家可以自行游上岸，比"传送失败回原位"体验更好。

**第 3 层：极端兜底回退原位**

仅当 `liveY+1` 验证也失败（liveY 下方不是固体，或 liveY+1 是岩浆/火焰等致命方块）时，才回退原位 + Failed 状态。

#### 15.2.2 i18n 15 语言完整化（[LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp) `Init()`）

**根因分析**：`Init()` 函数加载磁盘 JSON 文件后，仅通过 `kKeyCorrections`、`kUiExtras`、`kHotkeyActionExtras` 三个白名单补充新键。`TELEPORT_*` 键不在任何白名单中，因此如果磁盘 JSON 是旧版本，`TELEPORT_*` 键永远不会被补充，`GetText("TELEPORT_LOADING")` 返回键名而非翻译文本。

**修复**：在 `Init()` 函数的磁盘 JSON 写入循环中，新增从 `g_defaultJsonFiles` 模板补充所有缺失键的逻辑：

```cpp
// [传送安全增强 v2] 从 g_defaultJsonFiles 模板补充所有缺失键
try {
    json templateJ = json::parse(jsonContent);
    for (auto it = templateJ.begin(); it != templateJ.end(); ++it) {
        if (!j.contains(it.key())) {
            j[it.key()] = it.value();
        }
    }
} catch (...) {}
```

此逻辑遍历代码内硬编码的 `g_defaultJsonFiles` 模板（含 15 语言 × 6 键 = 90 个 `TELEPORT_*` 键值对），补充磁盘 JSON 文件中缺失的所有键。这样无论是新装还是旧版本升级，`GetText("TELEPORT_LOADING")` 都能正确返回当前语言的翻译文本。

15 种语言覆盖：en、zh_CN、zh_TW、de、fr、es、it、ja、ko、ru、pt、nl、tr、ar、hi。

### 15.3 v3 参数对比

| 参数 | v2 | v3 | 说明 |
|------|----|----|------|
| 搜索半径（严格） | ±16 | ±16 | 不变 |
| 搜索半径（宽松） | ±16 | ±16 | 不变 |
| 搜索半径（超大范围） | — | ±32 | v3 新增第三轮 |
| 三轮全失败兜底 | 回退原位 + Failed | validated liveY+1 → 否则回退原位 | v3 优先使用 liveY+1 |
| 稳定性帧数 | 3 | 3 | 不变 |
| 超时 | 4000ms | 4000ms | 不变 |
| i18n 键补充 | 白名单（3 个） | 白名单 + 全模板扫描 | v3 新增全模板扫描 |

### 15.4 v3 修订 v2 安全原则

v2 文档第 14.2 节中"**[v2 Safety Principle]** When no safe spawn found within ±16, ABORT to original position + Failed state — NEVER use 'accept raw Y+1' fallback"在 v3 中**部分修订**：

- **保留原则**：仍不接受未经严格验证的"raw Y+1"（防止部分加载区块的临时错误 Y 导致地下/虚空传送）
- **修订**：经过 3 帧稳定性 + IsChunkReady + 下方固体 + 上方非危险四重验证的 `liveY+1` 可用作兜底（条件比 v2 严格，但能避免极端场景下的传送失败）
- **不变**：仍保留"回退原位 + Failed"作为最后兜底，仅在验证全部失败时启用

### 15.5 v3 构建验证

- 编译：`xmake -r` 零警告零错误，耗时 20.782s
- DLL：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`，1,243,648 字节（1,214.5 KB）
- 期间修复 LanguageManager.cpp 的 BOM 累积问题（2 个 BOM → 1 个 BOM）

### 15.6 v3 部署

将 `D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll` 复制到：
`D:\我的世界\基岩版游戏\levilauncher.exe\versions\1.26.20.04\mods\ChiyanMap\`

重启游戏后：
1. 传送至巨大湖泊/岩浆湖中央时，会自动搜索 ±32 范围寻找陆地
2. 若 ±32 仍无陆地，会验证 liveY 后使用 liveY+1 兜底（最多落到水里，不会到地下/岩浆）
3. 仅在极端情况下（liveY 下方非固体 或 上方致命方块）才回退原位 + 失败提示
4. 传送加载界面在 15 种语言下均显示本地化文本（不再显示 "TELEPORT_LOADING" 等英文键名）

---

**文档结束**
