# 生物群系持久化显示机制（最终方案）

## Context

**方案演进历史**：

| 版本 | 方案 | 结果 |
|------|------|------|
| Task D | 鼠标悬停显示玩家当前生物群系 | ✅ 完成基础功能 |
| Task E | 持久化生物群系 + 缓存优先显示 | ⚠️ 用户反馈仍有显示错乱 |
| Task F | 严格隐藏（仅区块已加载时显示，不读缓存） | ❌ 用户否决：要求持久化数据用于显示 |
| **Task G（本方案）** | **缓存优先显示 + SafeGetSurfaceY 门控实时回退** | ✅ 最终方案 |

**用户最终需求**（原话）：
> "抵达哪个区域就保存哪个区域的生物群系，并在离开后打开全屏大地图鼠标指针悬停在那里会显示。也就是说，要持久地保存生物群系数据，并用来显示。"

**核心策略**：
1. **持久化保存**：玩家抵达区域时，扫描采集生物群系并持久化到 `region_*.bin`（既有机制，不变）
2. **缓存优先显示**：鼠标悬停时，优先从持久化缓存读取生物群系（即使区块已卸载，仍可显示）
3. **实时回退**：缓存未命中时（刚进入尚未扫描的新区域），用 `SafeGetSurfaceY` 验证区块已加载后实时查询
4. **数据完整性**：持久化存储机制保持不变，跨会话保留

---

## 修改范围

### 1. `src/hooks/DX11Hook.h` — 替换悬停生物群系查询逻辑（第 1452-1497 行）

**Task F（严格隐藏，已废弃）**：
```
regionRendered → SafeGetSurfaceY(验证区块加载) → [已加载] SafeGetBiomeName(实时) → 显示
                                              → [未加载] 严格隐藏（不读缓存）
```

**Task G（最终方案）**：
```
regionRendered → [优先级1] GetCachedBiomeName(缓存优先，持久化显示)
              → [优先级2] 缓存未命中 → SafeGetSurfaceY(验证区块加载) → SafeGetBiomeName(实时) → 显示
              → 两者都失败 → 隐藏
```

**最终代码块**（`DX11Hook.h:1452-1497`）：

```cpp
if (regionRendered) {
    std::string rawName;
    bool ok = false;
    // [优先级1] 缓存生物群系（持久化显示核心）
    // 玩家抵达过的区域，生物群系已由扫描采集并持久化到 region_*.bin
    // 即使区块已卸载（玩家离开），仍可从缓存读取并显示
    if (MapCacheManager::GetCachedBiomeName(bx, bz, rawName)) {
        ok = true;
    }
    // [优先级2] 实时查询（仅对当前已加载区块有效，刚进入尚未扫描的新区域）
    // 使用 SafeGetSurfaceY 验证区块已加载，避免部分加载区块返回错误生物群系
    if (!ok && g_clientInstance) {
        BlockSource* region = g_clientInstance->getRegion();
        if (region) {
            // 区块加载探测：SafeGetSurfaceY 返回有效 Y 表示区块已加载
            //  - 返回 > -64 且 < 319：区块已加载，有真实地表
            //  - 返回 ≤ -64：区块完全未加载（哨兵）
            //  - 返回 -32000：部分加载触发 AV（SEH __except 捕获）
            short liveY = SafeGetSurfaceY(*region, bx, bz);
            if (liveY > -64 && liveY < 319) {
                // 区块已加载 → 在地表 Y 实时查询生物群系（Y 精确）
                ok = SafeGetBiomeName(*region, bx, (int)liveY, bz, rawName);
            }
        }
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
        // 无缓存且区块未加载 → 淡出
        MapRenderState::hoverBiomeTargetAlpha = 0.0f;
        MapRenderState::hoverBiomeHasValidResult = false;
    }
} else {
    // 未渲染 region → 不显示
    MapRenderState::hoverBiomeTargetAlpha = 0.0f;
    MapRenderState::hoverBiomeHasValidResult = false;
}
```

### 2. 不修改的部分（持久化机制全部保留）

以下代码**保持不变**，满足"持久化保存"需求：

- **`MapCacheManager.h`**：`biomeCells`/`biomeTable`/`BiomeEntry`/`UpdateBiomesFromScan`/`GetCachedBiomeName` 全部保留
- **`MapCacheManager.cpp`**：
  - `WriteBiomeSection`/`ReadBiomeSection` 文件读写（跨会话持久化）
  - `UpdateBiomesFromScan` 批量写入缓存（扫描 → 内存）
  - `GetCachedBiomeName` 缓存查询（显示调用）
  - `IOWorkerCoro` 每 2s 落盘（内存 → 磁盘）
  - `SwitchWorld` 切世界时落盘
- **`PlayerHook.h`**：
  - 扫描循环中 `biomeEntries.push_back` 采集生物群系（第 801-802 行）
  - 异步线程 `UpdateBiomesFromScan` 调用（第 865-874 行）
  - `SafeGetSurfaceY`（第 371 行）— 复用为实时回退的区块加载探测器
  - `SafeGetBiomeName`（第 390 行）— 复用为实时回退的生物群系查询

---

## 逻辑流程图

```
大地图开启 + 鼠标悬停
        │
        ▼
┌───────────────────────────┐
│ 节流检查                   │  帧计数 ≥6 或 鼠标位移 >4 格
│ (frameThrottle || moved)  │  否 → 跳过本轮，保持上一次结果
└─────────┬─────────────────┘
          │ 是
          ▼
┌───────────────────────────┐
│ region 渲染检查             │  g_regionSRVs.find(hash) != end && 非空
│ (regionRendered)          │  否 → targetAlpha=0（隐藏）
└─────────┬─────────────────┘
          │ 是
          ▼
┌───────────────────────────┐
│ [优先级1] 缓存查询          │  GetCachedBiomeName(bx, bz, rawName)
│ (持久化生物群系)           │  玩家抵达过的区域已持久化
└─────────┬─────────────────┘
          │
    ┌─────┴─────┐
    │ 缓存命中? │
    ├──是───────┤
    │           │ 否
    │           ▼
    │   ┌───────────────────────────┐
    │   │ [优先级2] 实时查询          │  g_clientInstance->getRegion()
    │   │ SafeGetSurfaceY(验证加载)  │  liveY > -64 且 < 319?
    │   └─────────┬─────────────────┘
    │             │ 是
    │             ▼
    │   ┌───────────────────────────┐
    │   │ SafeGetBiomeName(实时)     │  在地表 Y 查询生物群系
    │   └─────────┬─────────────────┘
    │             │
    └─────┬───────┘
          │
    ┌─────┴─────┐
    │ ok?       │
    ├──是───────┤──否──→ targetAlpha=0（隐藏）
    │           │       hasValidResult=false
    ▼           │
┌───────────────┴───────────┐
│ 更新显示状态                │  rawName/translatedName/hasValidResult=true
│ targetAlpha = 1.0          │  lastQueryX/Z = hoverWx/hoverWz
└─────────┬─────────────────┘
          │
          ▼ (统一路径)
┌───────────────────────────┐
│ alpha 平滑过渡              │  alpha += (target-alpha) * clamp(12*dt, 0, 1)
│ (帧率无关 lerp)            │
└─────────┬─────────────────┘
          │
          ▼
┌───────────────────────────┐
│ 绘制                       │  alpha > 0.01 && hasValidResult → 绘制
│ (背景+文字 alpha 同步缩放)  │  否 → 不绘制（完全透明）
└───────────────────────────┘
```

---

## 三方案对比

| 维度 | Task E（缓存优先） | Task F（严格隐藏） | **Task G（最终）** |
|------|-------------------|-------------------|-------------------|
| 数据源 | 缓存优先 + 实时回退 | **仅实时** | 缓存优先 + 实时回退 |
| 实时回退的区块验证 | ❌ 无（可能查部分加载区块） | ✅ SafeGetSurfaceY | ✅ SafeGetSurfaceY |
| 实时回退的 Y 坐标 | 缓存近似值 / 玩家 Y | liveY（精确） | liveY（精确） |
| 离开区域后悬停 | 显示缓存 | **隐藏**（用户否决） | **显示缓存**（用户要求） |
| 互斥锁 | 2-3 次 | 0 次 | 1-2 次 |
| 用户验收 | ❌ 显示错乱 | ❌ 否决（不显示缓存） | ⏳ 待验证 |

**Task G 相比 Task E 的改进**：
1. 实时回退路径增加 `SafeGetSurfaceY` 区块加载验证，避免部分加载区块返回错误生物群系
2. 实时回退使用 `liveY`（精确地表 Y）替代 `queryY`（缓存近似值或玩家 Y），查询更准确
3. 移除了 `GetCachedSurfaceHeight` 调用（实时回退不再需要，`SafeGetSurfaceY` 直接给出 liveY）

---

## 持久化与显示的同步机制

```
玩家移动 → 新区块加载 → 扫描触发（每 16 格或 100 tick）
                              │
                    ┌─────────┴─────────┐
                    ▼                   ▼
            getBiome（着色用）    biomeEntries.push_back（采集）
                    │                   │
                    │                   ▼
                    │       UpdateBiomesFromScan（批量写入）
                    │                   │
                    │                   ▼
                    │       RegionData.biomeCells + biomeTable（内存）
                    │                   │
                    │                   ▼ (每 2s 或 SwitchWorld)
                    │       WriteBiomeSection → region_*.bin（磁盘）
                    │
                    ▼
            地图纹理更新（既有）

    ┌──────────────────────────────────────────────────┐
    │ 大地图悬停显示（Task G 双路径）                      │
    │   [优先级1] GetCachedBiomeName ← 内存/磁盘缓存    │  ← 离开后仍可显示
    │   [优先级2] SafeGetSurfaceY → SafeGetBiomeName    │  ← 新区域实时查询
    └──────────────────────────────────────────────────┘
```

**同步保证**：
- 玩家抵达区域 → 区块加载 → 扫描采集 → `UpdateBiomesFromScan` → 内存 → 每 2s 落盘
- 悬停显示优先级1 → 从内存/磁盘读取持久化数据（与扫描采集同源：`region.getBiome`）
- 悬停显示优先级2 → 实时查询（`SafeGetBiomeName`），同样调用 `region.getBiome`
- 两条路径数据源一致，天然同步

---

## 边界情况处理

| 场景 | 处理 | 正确性 |
|------|------|--------|
| 玩家抵达过的区域，离开后悬停 | **缓存命中 → 显示持久化生物群系** | ✅ 核心需求 |
| 玩家当前所在区块，悬停 | 缓存命中（已扫描）→ 显示；或缓存未命中→实时查询→显示 | ✅ 准确 |
| 刚进入尚未扫描的新区域 | 缓存未命中 → SafeGetSurfaceY 验证加载 → 实时查询 → 显示 | ✅ 准确 |
| 部分加载区块（Phase 0 探测中） | 缓存未命中 → SafeGetSurfaceY 返回 -32000 → 跳过实时查询 → 隐藏 | ✅ 安全 |
| 从未到访区域（无纹理） | `regionRendered=false` → 隐藏 | ✅ 隐藏 |
| 玩家快速移动，区块不断加载/卸载 | 缓存优先 + 节流，已扫描区域始终可显示 | ✅ 稳定 |
| 地狱维度 | `canQuery=false`（既有逻辑）→ 隐藏 | ✅ 隐藏 |
| 大地图刚打开 | 重置 alpha=0 + 强制查询（既有逻辑）→ 缓存命中则淡入 | ✅ 淡入 |
| 小地图下方生物群系 | 不受影响（仍用 `translatedBiomeName`，玩家当前生物群系） | ✅ 不变 |
| 跨会话（重启游戏） | 持久化数据从 `region_*.bin` 加载 → 缓存命中 → 显示 | ✅ 跨会话保留 |

---

## 性能分析

| 指标 | Task F（严格隐藏） | **Task G（最终）** | 变化 |
|------|-------------------|-------------------|------|
| 缓存命中时互斥锁 | 0 次 | 1 次（`GetCachedBiomeName`） | +1 次（可接受） |
| 缓存未命中时互斥锁 | 0 次 | 1 次（`GetCachedBiomeName` 失败也加锁） | +1 次 |
| 实时查询区块验证 | SafeGetSurfaceY（每次） | SafeGetSurfaceY（仅缓存未命中时） | ✅ 更少 |
| 帧率影响 | 基准 | 持平（节流后每秒最多 10 次查询，1 次锁开销可忽略） | ✅ 持平 |
| 内存占用 | 不变 | 不变 | 持平 |

### 性能保障措施

1. **节流策略不变**：帧计数 ≥6 或鼠标世界位移 >4 格才查询，平均 ~100ms 一次
2. **region 渲染快速过滤**：`g_regionSRVs.find(hash)`（O(1)）先过滤未渲染区域
3. **缓存命中快速返回**：`GetCachedBiomeName` 内部一次锁 + O(1) 数组查找，开销极低
4. **实时回退有门控**：仅缓存未命中时才走 SafeGetSurfaceY + SafeGetBiomeName，避免无谓 SEH 开销
5. **alpha lerp 保持平滑**：过渡逻辑不变，帧率无关

---

## 编译验证

### 实际编译结果（2026-07-18 19:39）

- **配置**：MSVC 2026 (x64)，`/EHa /W4` 严格警告模式
- **结果**：✅ `build ok, spent 19.796s`，**零警告、零错误**
- **产物**：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`
- **DLL 大小**：1,137,664 字节（1,111 KB）
- **二次验证**：`xmake -r 2>&1 | Select-String "warning|error"` 返回空输出

### 编译产物对比表

| 版本 | DLL 大小 | 变化 | 说明 |
|------|---------|------|------|
| Task E（缓存优先，原始） | 1,124,352 字节 | 基准 | 持久化 + 缓存优先查询 |
| Task F（严格隐藏） | 1,137,152 字节 | +12,800 字节 | 移除缓存查询，纯实时 |
| **Task G（最终方案）** | **1,137,664 字节** | **+13,312 字节**（vs Task E） | 缓存优先 + SafeGetSurfaceY 门控实时回退 |

---

## 测试场景

| # | 场景 | 预期结果 | 验证要点 |
|---|------|---------|---------|
| 1 | 玩家抵达区域A，离开后悬停A | **显示A的生物群系**（缓存命中） | ✅ 核心需求 |
| 2 | 玩家当前所在区块悬停 | 显示当前生物群系（缓存或实时） | 实时性 |
| 3 | 刚进入新区域（尚未扫描）悬停 | 显示实时生物群系（SafeGetSurfaceY 验证后查询） | 实时回退 |
| 4 | 部分加载区块悬停 | 隐藏（SafeGetSurfaceY 返回 -32000） | 安全性 |
| 5 | 从未到访区域悬停 | 隐藏（regionRendered=false） | 边界 |
| 6 | 快速移动（疾跑/骑马） | 已扫描区域始终显示，无卡顿 | 性能 |
| 7 | 不同生物群系类型（森林/沙漠/雪地/海洋/山地） | 准确显示对应名称 | 准确性 |
| 8 | 生物群系边界（森林/平原交界） | 鼠标移过边界时名称切换 | 精度 |
| 9 | 大地图缩放（0.2x ~ 40x） | 缩放不影响生物群系准确性 | 鲁棒性 |
| 10 | 小地图下方生物群系 | 仍显示玩家当前生物群系（不受影响） | 隔离性 |
| 11 | 重启游戏后悬停曾到访区域 | 显示持久化生物群系（从 region_*.bin 加载） | 跨会话 |
| 12 | 地狱维度 | 不显示生物群系 | 维度隔离 |

---

## 修改文件清单

| 文件 | 改动 | 行数变化 |
|------|------|---------|
| `src/hooks/DX11Hook.h` | 替换第 1452-1497 行 `if (regionRendered)` 块（Task F 严格隐藏 → Task G 缓存优先+门控实时） | +4 行 |
| 其他文件 | **不修改**（持久化代码全部保留） | 0 |

---

## 实现完成总结

### 已完成

1. ✅ **代码修改**：`DX11Hook.h` 第 1452-1497 行替换为缓存优先 + SafeGetSurfaceY 门控实时回退
2. ✅ **编译验证**：零警告零错误，生成 1,137,664 字节 DLL
3. ✅ **持久化保留**：`MapCacheManager.h/cpp` + `PlayerHook.h` 扫描采集代码全部不变
4. ✅ **小地图不受影响**：小地图生物群系显示路径未触及

### 核心行为

| 场景 | 行为 |
|------|------|
| 鼠标悬停已抵达过的区域（区块已卸载） | **显示持久化生物群系**（缓存命中）← 用户核心需求 |
| 鼠标悬停当前已加载区块 | 显示生物群系（缓存命中或实时查询） |
| 鼠标悬停刚进入尚未扫描的新区域 | 实时查询（SafeGetSurfaceY 验证后） |
| 鼠标悬停部分加载区块 | 隐藏（避免错误数据） |
| 鼠标悬停从未到访区域 | 隐藏（无数据） |
| 小地图下方生物群系 | 显示玩家当前生物群系（不变） |

### 用户测试建议

完成编译后，建议用户按以下顺序验证：

1. **核心验证（场景1）**：进入世界，移动到区域A（等待扫描采集），继续移动离开A使区块卸载，打开大地图，鼠标悬停A → 应**显示A的生物群系**（持久化显示生效）
2. **实时性验证（场景3）**：进入新区域，立即悬停 → 应显示当前生物群系（实时回退）
3. **跨会话验证（场景11）**：重启游戏，悬停曾到访区域 → 应显示持久化生物群系
4. **小地图不变（场景10）**：小地图下方仍显示玩家当前生物群系
5. **性能验证（场景6）**：快速移动时已扫描区域始终显示，无卡顿
