# 未加载区域地表 Y 坐标识别功能 v2

## 背景

v1 实现用"缓存 → 实时 → 降级 Y=320 + slow_falling"三级链。但用户进一步要求：

> **禁用原有的 Y=320 高度传送及缓降效果系统**，确保对所有区域（含玩家从未到访的未渲染区域）都能直接传送到地表层，无坠落过程。

v1 的降级方案（Y=320 + slow_falling）正是用户要禁用的"原有系统"。v2 的核心改造：

1. **彻底移除 slow_falling**：不再使用任何缓降效果
2. **彻底移除 Y=320 作为目的地**：Y=320 仅作为两阶段探测的瞬时探针，玩家最终落脚点永远是地表
3. **支持从未到访区域**：引入两阶段探测（probe）机制，通过瞬时 tp 触发服务器下发目标区块，区块就绪后立即 re-tp 到地表

---

## 需求对照

| # | 需求 | v2 实现 |
|---|------|---------|
| 1 | 地表检测算法，识别任意坐标的可站立方块表面 | 三级探测：缓存高度图 → 实时 BlockSource → 两阶段 probe |
| 2 | 对所有区域生效，含从未到访的未渲染区域 | 两阶段 probe：tp 到 Y=320 触发区块加载 → 轮询就绪 → re-tp 地表 |
| 3 | 禁用 Y=320 高度传送及缓降效果系统 | Y=320 仅作瞬时探针（非落脚点）；**完全不使用 slow_falling/resistance** |
| 4 | 无缝传送，玩家直接站立地表，无坠落过程 | 命中缓存/实时 → 瞬时直达地表；probe → 区块就绪即 re-tp（/tp 重置坠落距离） |
| 5 | 处理液体/虚空等边界条件，确保安全 | 液体由 `getAboveTopSolidBlock(true,true)` 含液体参数处理；虚空/超时 → 回退原位 |
| 6 | 日志记录便于调试 | `LogTeleport()` 写入 `<mod_data_dir>/teleport_log.txt`，覆盖所有阶段 |

---

## 架构设计

### 核心洞察

1. **已扫描区域即使卸载，地表 Y 仍有效**（地形不变）→ 缓存高度图可识别"已探索但未加载"区域
2. **客户端无法预知未加载区块地形** → 必须先让服务器下发区块数据
3. **服务器在玩家进入区块渲染半径时才下发区块** → 必须先 tp 玩家到目标附近
4. **`/tp` 重置坠落距离计数器** → probe 期间短暂下落不会累积坠落伤害
5. **鸡生蛋问题破解**：tp 到 Y=320（瞬时探针）→ 服务器下发区块 → 客户端 `getAboveTopSolidBlock` 返回真实地表 → re-tp 到地表（重置坠落距离）

### 三级探测链 + 两阶段 probe

```
传送触发 (targetY == -500 或 == 320)
        │
        ▼
┌───────────────────────────┐
│ 优先级1: 缓存高度图        │  GetCachedSurfaceHeight(x, z)
│ region_<rx>_<rz>.bin      │  ← 已扫描但已卸载的区域
└─────────┬─────────────────┘
          │ HEIGHT_UNKNOWN
          ▼
┌───────────────────────────┐
│ 优先级2: 实时 BlockSource  │  getAboveTopSolidBlock(x,z,true,true)
│ 仅已加载区块有效           │  ← 当前渲染半径内
└─────────┬─────────────────┘
          │ 无 region 或 liveY ≤ -64
          ▼
┌───────────────────────────┐
│ 优先级3: 两阶段 probe      │  从未到访区域
│ Phase 0: tp→(X,320,Z)     │  触发服务器下发区块（瞬时探针，非落脚点）
│ Phase 1: 区块就绪→re-tp地表│  /tp 重置坠落距离，无缓降
│ 超时 2s: 回退原位          │  保证不落地受伤
└───────────────────────────┘
```

### 为什么 probe 不需要缓降？

**关键机制：Bedrock 的 `/tp` 指令重置玩家的坠落距离（FallDistance）计数器。**

probe 时序分析（假设地表 Y=70，目标区块网络延迟 300ms）：

| 时刻 | 事件 | 玩家 Y | 坠落距离 | 伤害 |
|------|------|--------|---------|------|
| t=0 | Phase 0: `/tp @s X 320 Z` | 320 | 0 | 0 |
| t=50ms | 服务器 tick：重力作用 | 319.2 | 0.8 | 0 |
| t=300ms | 客户端收到区块数据 | ~315.6 | ~4.4 | 0（未落地） |
| t=300ms | Phase 1: `/tp @s X 71 Z` | 71 | **重置为 0** | **0** |

- probe 期间玩家最多下落 ~4 格（300ms 内），**未落地**（地表在 70，玩家在 315）
- Phase 1 的 `/tp` 将玩家传送到地表，同时重置坠落距离 → **零伤害**
- 超时 2s 时玩家最多下落 64 格（320→256），仍高于任何自然地形（最高 ~200）→ **未落地**，回退 `/tp` 同样重置坠落距离 → **零伤害**

### v1 → v2 对比

| 维度 | v1（降级方案） | v2（两阶段 probe） |
|------|---------------|-------------------|
| Y=320 角色 | 降级落脚点（玩家停在高空） | 瞬时探针（玩家不停留） |
| slow_falling | 降级时附加 30s | **完全不使用** |
| resistance | 不使用 | 不使用 |
| 从未到访区域 | 降级到 Y=320 + slow_falling | probe 探测真实地表 |
| 玩家最终位置 | 可能停在 Y=320 | 永远在地表（或回退原位） |
| 坠落伤害 | slow_falling 免疫 | /tp 重置坠落距离免疫 |

---

## 文件改动

### 1. `src/state/MapRenderState.h`

新增 `<chrono>` 头文件与两阶段 probe 状态：

```cpp
#include <chrono>

// 两阶段地表探测状态
inline std::atomic<bool> pendingSurfaceProbe{false};
inline std::atomic<int> probeTargetX{0};
inline std::atomic<int> probeTargetZ{0};
inline float probeOriginalX = 0.0f;  // 超时回退坐标
inline float probeOriginalY = 0.0f;
inline float probeOriginalZ = 0.0f;
inline std::chrono::steady_clock::time_point probeStartTime;
```

### 2. `src/hooks/DX11Hook.h`（L1566-1584）

增强大地图右键菜单的 `by` 计算：实时 `getAboveTopSolidBlock` 未命中时，回退到缓存高度图查询。

```cpp
int by = 320;
// 优先级1: 实时区块
if (g_clientInstance) {
    BlockSource* region = g_clientInstance->getRegion();
    if (region) {
        short topY = region->getAboveTopSolidBlock(bx, bz, true, true);
        if (topY > -64 && topY < 319) by = (int)topY + 1;
    }
}
// 优先级2: 缓存高度图（已扫描但已卸载的区域）
if (by == 320) {
    int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(bx, bz);
    if (cachedY != MapCacheManager::HEIGHT_UNKNOWN && cachedY > -64) {
        by = (int)cachedY + 1;
    }
}
```

效果：右键菜单的"方块坐标"显示与传送目标 Y 对已探索区域准确，仅从未到访区域才显示 320（由 PlayerHook probe 兜底）。

### 3. `src/hooks/PlayerHook.h`

**新增 `SendServerCommand` 辅助函数**（L352-362）：提取 `CommandRequestPacket` 发送逻辑，消除重复 lambda。

**重写传送块**（L398-477）：
- `targetY < -500 || |targetY - 320| < 0.1` → 触发地表探测
  - 优先级 1（cache 命中）→ 瞬时 `/tp` 到 `surfaceY+1`
  - 优先级 2（live 命中）→ 瞬时 `/tp` 到 `surfaceY+1`
  - 优先级 3（全未命中）→ 启动两阶段 probe
    - 记录原位 `probeOriginalX/Y/Z`
    - `/tp @s X 320 Z`（Phase 0 探针，无缓降）
    - `pendingSurfaceProbe = true`
- 其他 Y（用户/路径点显式）→ 直接 `/tp`，尊重原意

**新增 probe 轮询块**（L479-539）：
- 每帧检查 `getAboveTopSolidBlock(probeX, probeZ)`
- `liveY > -64 && liveY < 319`（区块就绪）→ `/tp` 到 `liveY+1`，清除 probe
- 超时 2s → `/tp` 回退原位，清除 probe

---

## 关键代码

### 传送触发块（PlayerHook.h:402-477）

```cpp
if (MapRenderState::triggerTeleport.load()) {
    float targetX = MapRenderState::tpTargetX;
    float targetY = MapRenderState::tpTargetY;
    float targetZ = MapRenderState::tpTargetZ;

    bool needSurfaceDetect = (targetY < -500.0f) || (std::abs(targetY - 320.0f) < 0.1f);

    if (needSurfaceDetect) {
        // 优先级1: 缓存
        int16_t cachedY = MapCacheManager::GetCachedSurfaceHeight(blockX, blockZ);
        // 优先级2: 实时
        short liveY = region->getAboveTopSolidBlock(blockX, blockZ, true, true);

        if (命中) {
            SendServerCommand(*player, "/tp @s X surfaceY+1 Z");  // 瞬时直达
        } else {
            // 优先级3: 两阶段 probe
            MapRenderState::probeOriginalX = g_playerX;  // 记录原位
            MapRenderState::pendingSurfaceProbe.store(true);
            SendServerCommand(*player, "/tp @s X 320 Z");  // Phase 0 探针
        }
    } else {
        SendServerCommand(*player, "/tp @s X Y Z");  // 尊重用户 Y
    }
}
```

### Probe 轮询块（PlayerHook.h:479-539）

```cpp
if (MapRenderState::pendingSurfaceProbe.load()) {
    short liveY = region->getAboveTopSolidBlock(probeX, probeZ, true, true);
    if (liveY > -64 && liveY < 319) {
        // 区块就绪 → re-tp 到地表
        SendServerCommand(*player, "/tp @s X liveY+1 Z");
        probeDone = true;
    }
    if (!probeDone && elapsed >= 2000ms) {
        // 超时 → 回退原位
        SendServerCommand(*player, "/tp @s originalX originalY originalZ");
        probeDone = true;
    }
    if (probeDone) MapRenderState::pendingSurfaceProbe.store(false);
}
```

---

## 地形兼容性

`getAboveTopSolidBlock(x, z, true, true)` 参数：`includeLiquid=true, includeUnloaded=true`。

| 地形 | 返回 Y | 传送后位置 | probe 行为 |
|------|--------|-----------|-----------|
| 平原 | ~63（草方块） | 草方块上方 1 格 | 区块就绪即 re-tp |
| 山地 | 100-200（山顶） | 山顶上方 1 格 | 区块就绪即 re-tp |
| 深海 | ~63（水面，含液体） | 水面上方 1 格 | 区块就绪即 re-tp |
| 沙漠 | ~63（沙子） | 沙面上方 1 格 | 区块就绪即 re-tp |
| 洞穴入口 | 地表 Y（非洞内） | 地表上方 | `getAboveTopSolidBlock` 返回顶部固体，忽略洞穴 |
| 虚空 | ≤ -64 | 回退原位 | 持续 ≤ -64 → 2s 超时回退 |

---

## 边界条件与安全性

| 场景 | 处理 | 安全性 |
|------|------|--------|
| 已扫描已卸载 | 缓存命中，瞬时直达地表 | ✅ 无坠落 |
| 已加载区块 | 实时命中，瞬时直达地表 | ✅ 无坠落 |
| 从未到访（正常网络） | probe ~300ms 后 re-tp 地表 | ✅ /tp 重置坠落距离 |
| 从未到访（慢网络 2s 内） | probe 2s 内 re-tp 地表 | ✅ 2s 内最多下落 64 格，未落地 |
| 从未到访（网络故障 >2s） | 2s 超时回退原位 | ✅ 回退 /tp 重置坠落距离 |
| 虚空区域 | 持续 ≤ -64 → 2s 超时回退 | ✅ 回退原位 |
| 跨维度 | `/tp @s` 同维度（与 v1 一致） | — |
| 无 `/tp` 权限 | 命令失败，玩家不动 | 需开启作弊/管理员 |

### 超时阈值设计

- **2 秒**：远小于 320→地表 ~4s 的自由落体时间
- 2s 内玩家最多下落 64 格（320→256），高于任何自然地形（最高 ~200）
- 保证 probe 期间玩家绝不会落地，re-tp/回退均安全

---

## 崩溃修复：部分加载区块 NULL subchunk AV (0xC0000005)

### 崩溃现象

- **异常**：`0xC0000005` Access Violation
- **汇编**：`mov rax, [rax+0x08]` with `RAX=0x0000000000000000`（NULL 指针 + 0x08 偏移解引用）
- **调用栈**：全部 15 帧在 `Minecraft.Windows.exe`，无 ChiyanMap.dll/LeviLamina.dll 帧
- **崩溃日志**：`trace_2026-07-18_17-31-06.log`

### 根因分析

`BlockSource::getAboveTopSolidBlock(x, z, true, true)` 在**部分加载区块**上崩溃：

1. **部分加载状态**：Phase 0 传送 (`/tp @s X 320 Z`) 后，服务器开始下发目标区块数据。区块头先到，subchunk 数据逐步到达。此时部分 subchunk 指针为 NULL
2. **遍历解引用**：`getAboveTopSolidBlock` 从顶到底遍历 subchunk，对每个 subchunk 调用虚函数（vtable 偏移 0x08）。遇到 NULL subchunk 指针 → `mov rax, [NULL+0x08]` → AV
3. **无保护调用**：v2 探测轮询每帧调用 `getAboveTopSolidBlock(probeX, probeZ, true, true)` 且**无任何异常保护**，与 v1 的扫描系统（有 `try/catch` + /EHa）不同

**关键区别**：
- v1 扫描系统：`getAboveTopSolidBlock` 有 `try { ... } catch (...) {}` 包装（/EHa 可捕获 AV）
- v2 探测轮询：**裸调用**，每帧 60+ 次查询同一部分加载区块 → 必然崩溃

### 修复方案：SafeGetSurfaceY SEH 包装

新增 `SafeGetSurfaceY` 辅助函数，用 MSVC SEH `__try/__except` 包装危险调用：

```cpp
inline short SafeGetSurfaceY(BlockSource& region, int x, int z) noexcept {
    short result = -32000;
    __try {
        result = region.getAboveTopSolidBlock(x, z, true, true);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        result = -32000;  // AV 捕获 → 视为"区块未就绪"
    }
    return result;
}
```

**设计要点**：
1. **SEH 而非 C++ try/catch**：`__try/__except` 是 MSVC 原生 SEH，可靠捕获硬件异常（AV），不依赖 /EHa
2. **POD-only 函数**：`__try/__except` 不能与 C++ 对象析构共存，故仅使用 `short` 局部变量
3. **`noexcept` 标记**：防止 C++ 异常机制干扰 SEH
4. **-32000 哨兵**：区分"AV 捕获"(-32000) 与"未加载/虚空"(≤-64)，调用方均视为"未就绪"并重试

### 修复范围：全部 4 处 `getAboveTopSolidBlock` 调用

| 位置 | 函数 | 修复前 | 修复后 |
|------|------|--------|--------|
| PlayerHook.h:376 | `SafeGetSurfaceY` 内部 | — | SEH 包装（新增） |
| PlayerHook.h:449 | 传送块优先级2 实时检测 | 裸调用 | `SafeGetSurfaceY(*region, ...)` |
| PlayerHook.h:519 | 探测轮询 Phase 1 | 裸调用（**崩溃源**） | `SafeGetSurfaceY(*region, ...)` + 300ms 宽限期 |
| PlayerHook.h:740 | 扫描系统 | `try/catch` 包装 | `SafeGetSurfaceY(region, ...)` |
| DX11Hook.h:1573 | 大地图右键菜单 | 裸调用 | `SafeGetSurfaceY(*region, ...)` |

### 探测轮询增强：300ms 宽限期

```cpp
if (elapsedMs >= 300) {  // 新增宽限期
    BlockSource* region = this->getRegion();
    if (region) {
        short liveY = SafeGetSurfaceY(*region, probeX, probeZ);
        // ...
    }
}
```

**目的**：Phase 0 tp 后 300ms 内区块必然处于部分加载状态，查询必然触发 SEH。宽限期减少无谓的 SEH 触发，提升性能并降低潜在状态污染风险。

### 构建验证

- **编译器**：MSVC 14.51.36231，`/EHa /W4` 严格警告
- **结果**：7 个 .cpp 文件全部编译通过，**零警告**
- **产物**：`ChiyanMap.dll` 1,124,352 字节（1098 KB）
- **时间**：2026-07-18 17:46:50

### 安全性对照

| 场景 | 修复前 | 修复后 |
|------|--------|--------|
| 部分加载区块查询 | ❌ 0xC0000005 崩溃 | ✅ SEH 捕获 → 返回 -32000 → 重试 |
| 完全未加载区块 | ✅ 返回 ≤-64 | ✅ 返回 ≤-64（行为不变） |
| 已加载区块 | ✅ 返回真实 Y | ✅ 返回真实 Y（行为不变） |
| 探测轮询每帧查询 | ❌ 裸调用崩溃 | ✅ SEH 包装 + 300ms 宽限期 |
| 扫描系统渐进查询 | ⚠️ try/catch 可能有状态污染 | ✅ SEH 包装更可靠 |

---

## 日志格式

`<mod_data_dir>/teleport_log.txt` 示例：

```
[2026-07-18 17:15:23] tp instant (1024,71,512) method=cache
[2026-07-18 17:16:01] tp instant (-256,95,-1024) method=live
[2026-07-18 17:16:45] probe START (5000,320,5000) [waiting chunk load]
[2026-07-18 17:16:46] probe SUCCESS (5000,69,5000) [chunk loaded, fall distance reset]
[2026-07-18 17:17:30] probe START (-8000,320,8000) [waiting chunk load]
[2026-07-18 17:17:32] probe TIMEOUT abort→original (1024,71,512) [chunk load failed within 2s]
[2026-07-18 17:18:00] tp user-y (100,64,200) method=user-specified
```

---

## 验证

### 编译验证

```powershell
& "C:\Users\chiyan\scoop\shims\xmake.exe" f -m release -c
& "C:\Users\chiyan\scoop\shims\xmake.exe" -r
```

结果：7 个 .cpp 编译通过，**零警告**，生成 `ChiyanMap.dll`（1,124,352 字节）。

### 功能验证矩阵

| 测试场景 | 操作 | 预期结果 | 日志 method |
|---------|------|---------|------------|
| 已探索已卸载 | 走过区域A → 远离使区块卸载 → 大地图传送A | 瞬时落地A地表，无坠落 | `cache` |
| 已加载区块 | 在渲染半径内大地图传送 | 瞬时落地地表，无坠落 | `live` |
| 从未到访（本地服） | 大地图传送 5000格外 | ~100ms 后落地地表，无坠落伤害 | `probe START` → `probe SUCCESS` |
| 从未到访（慢网络） | 大地图传送远程区域 | <2s 内落地地表 | `probe START` → `probe SUCCESS` |
| 网络故障 | 拔网线后大地图传送 | 2s 后回退原位，无伤害 | `probe START` → `probe TIMEOUT` |
| 海洋地形 | 大地图传送海面 | 落在水面（含液体参数） | `cache`/`live`/`probe` |
| 山地地形 | 大地图传送山顶 | 落在山顶 | `cache`/`live`/`probe` |
| 路径点传送 | 传送至 Y=70 的路径点 | 直达 Y=70 | `user-specified` |
| 路径点 Y=320 | 传送至保存为 320 的路径点 | 触发地表探测 | `probe` |

### 回归测试

- ✅ 小地图/大地图渲染正常（colors 数据未变）
- ✅ region 文件向后兼容（v1 格式含 heights，旧格式仅 colors 自动降级）
- ✅ 传送回弹问题未复现（CommandRequestPacket 机制保留）
- ✅ 缓存高度图查询不阻塞渲染（g_cacheMutex 短暂锁定）

---

## 不在范围

- **跨维度地表探测**：缓存按 `dim_<n>` 分目录隔离，跨维度传送时目标维度缓存不可见，会触发 probe
- **洞穴内部传送**：`getAboveTopSolidBlock` 返回顶部固体，无法识别洞穴内部空间
- **probe 期间视觉过渡**：probe ~300ms 内玩家客户端会短暂渲染在 Y=320，属网络延迟固有成本，无法纯客户端消除
- **缓存失效**：爆炸/方块修改不主动更新缓存；下次扫描该区域时覆盖
