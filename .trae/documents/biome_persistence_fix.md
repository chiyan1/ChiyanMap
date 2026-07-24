# 修复地图生物群系显示错乱：持久化生物群系数据

## Context

**问题现象**：只有初始进入世界时已加载的区域能正常显示生物群系，其他已缓存但区块已卸载的区域显示异常。

**根本原因**（经代码探索确认）：

1. **扫描时**（`PlayerHook.h:786-800`）：扫描循环以 4×4 方块为单元格查询生物群系（`region.getBiome(BlockPos(targetX, topY-1, targetZ))`），但结果 `s_biomeName` **仅用于颜色着色**（`getBiomeTints`），**未传递给 `UpdateFromScan`**，扫描后丢弃。
2. **持久化时**（`MapCacheManager.cpp:80-84, 101-103`）：region `.bin` 文件仅存储 `colors[256×256×4]`（262KB）和 `heights[256×256]`（131KB），**无生物群系字段**。
3. **渲染时**（`DX11Hook.h:1462`）：鼠标悬停生物群系显示调用 `SafeGetBiomeName` → `BlockSource::getBiome`，**要求区块当前已加载**。对于已缓存但区块已卸载的区域 → 查询失败/异常 → 生物群系显示错乱。

**修复目标**：在扫描时（区块已加载）将生物群系名持久化到 region 缓存，渲染时优先查缓存，使所有已扫描区域都能正确显示生物群系，无需玩家重新抵达。

---

## 修改文件

### 1. `src/state/MapCacheManager.h` — 扩展 RegionData 与新增接口

**扩展 `RegionData` 结构**（第 21-30 行），新增生物群系字段：

```cpp
constexpr int REGION_SIZE = 256;
constexpr int BIOME_CELL_SIZE = 4;                                      // 4×4 方块为一个生物群系单元
constexpr int BIOME_CELLS_PER_REGION = REGION_SIZE / BIOME_CELL_SIZE;   // 64
constexpr uint8_t BIOME_INDEX_UNKNOWN = 255;                            // 哨兵：无生物群系数据

struct RegionData {
    uint8_t colors[REGION_SIZE * REGION_SIZE * 4] = {0};
    int16_t heights[REGION_SIZE * REGION_SIZE];
    // [新增] 生物群系：每 4×4 单元格存储 biomeTable 索引（255=未知）
    uint8_t biomeCells[BIOME_CELLS_PER_REGION * BIOME_CELLS_PER_REGION];
    std::vector<std::string> biomeTable;  // [新增] 本 region 内出现过的唯一生物群系名（去重）
    bool dirty = false;
    bool textureDirty = true;

    RegionData() {
        std::fill_n(heights, REGION_SIZE * REGION_SIZE, HEIGHT_UNKNOWN);
        std::fill_n(biomeCells, BIOME_CELLS_PER_REGION * BIOME_CELLS_PER_REGION, BIOME_INDEX_UNKNOWN);
    }
};
```

**新增接口声明**（第 44 行后）：

```cpp
// 生物群系扫描条目（扫描线程 → 缓存线程）
struct BiomeEntry {
    int cellWorldX;   // 世界单元格坐标（worldX >> 2）
    int cellWorldZ;
    std::string name; // 完整生物群系名（含命名空间，如 "minecraft:plains"）
};

// 从扫描数据批量写入生物群系缓存（线程安全，一次锁）
void UpdateBiomesFromScan(const std::vector<BiomeEntry>& entries);

// 查询缓存的生物群系名（未加载区域也能命中）
// 返回 false 表示无缓存数据（调用方应回退实时查询）
bool GetCachedBiomeName(int worldX, int worldZ, std::string& outName);
```

### 2. `src/state/MapCacheManager.cpp` — 实现持久化与查询

**扩展文件读取**（3 处：`IOWorkerCoro` 第 48-52 行、`UpdateFromScan` 第 159-162 行），在 colors+heights 之后读取生物群系段：

```cpp
in.read((char*)newRegion->colors, sizeof(newRegion->colors));
if (fileSize >= (std::streamoff)(sizeof(newRegion->colors) + sizeof(newRegion->heights))) {
    in.read((char*)newRegion->heights, sizeof(newRegion->heights));
}
// [新增] 生物群系段（新格式）
auto biomeSectionStart = sizeof(newRegion->colors) + sizeof(newRegion->heights);
if (fileSize > (std::streamoff)biomeSectionStart) {
    uint8_t biomeCount = 0;
    in.read((char*)&biomeCount, 1);
    newRegion->biomeTable.clear();
    newRegion->biomeTable.reserve(biomeCount);
    for (uint8_t i = 0; i < biomeCount; i++) {
        uint8_t len = 0;
        in.read((char*)&len, 1);
        std::string name(len, '\0');
        if (len > 0) in.read(&name[0], len);
        newRegion->biomeTable.push_back(name);
    }
    if (fileSize >= (std::streamoff)(biomeSectionStart + 1 + biomeCount /*近似*/) + (std::streamoff)sizeof(newRegion->biomeCells)) {
        in.read((char*)newRegion->biomeCells, sizeof(newRegion->biomeCells));
    }
}
```

**精确的向后兼容判断**：旧文件 `fileSize == 393216`（colors+heights）→ 跳过生物群系段，`biomeCells` 保持全 255（未知），下次扫描时填充。新文件 `fileSize > 393216` → 读取生物群系段。

**扩展文件写入**（2 处：`IOWorkerCoro` 第 82-83 行、`SwitchWorld` 第 101-102 行），在 colors+heights 之后写入：

```cpp
out.write((char*)item.second.colors, sizeof(item.second.colors));
out.write((char*)item.second.heights, sizeof(item.second.heights));
// [新增] 生物群系段
uint8_t biomeCount = (uint8_t)std::min<size_t>(item.second.biomeTable.size(), 255);
out.write((char*)&biomeCount, 1);
for (uint8_t i = 0; i < biomeCount; i++) {
    const std::string& name = item.second.biomeTable[i];
    uint8_t len = (uint8_t)std::min<size_t>(name.size(), 255);
    out.write((char*)&len, 1);
    if (len > 0) out.write(name.data(), len);
}
out.write((char*)item.second.biomeCells, sizeof(item.second.biomeCells));
```

**文件格式（新）**：
```
[colors: 262144 字节]       (现有)
[heights: 131072 字节]      (现有)
[biomeCount: 1 字节]        (新增, 0-255)
[biomeTable: biomeCount × (1 字节长度 + 变长名称)]  (新增)
[biomeCells: 4096 字节]     (新增, 每单元格 1 字节索引)
```
典型增量：~4.3KB/region（1.2% 增长）。

**实现 `UpdateBiomesFromScan`**：

```cpp
void UpdateBiomesFromScan(const std::vector<BiomeEntry>& entries) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    if (g_cacheDir.empty()) return;
    for (const auto& e : entries) {
        // 世界单元格坐标 → region 坐标
        int blockX = e.cellWorldX * BIOME_CELL_SIZE;
        int blockZ = e.cellWorldZ * BIOME_CELL_SIZE;
        int rx = (blockX < 0 ? (blockX + 1) / REGION_SIZE - 1 : blockX / REGION_SIZE);
        int rz = (blockZ < 0 ? (blockZ + 1) / REGION_SIZE - 1 : blockZ / REGION_SIZE);
        uint64_t hash = GetRegionHash(rx, rz);

        // 确保 region 在内存中（若未加载则跳过，下次扫描会补）
        auto it = g_loadedRegions.find(hash);
        if (it == g_loadedRegions.end() || it->second == nullptr) continue;

        RegionData* region = it->second;
        int localCellX = (blockX - rx * REGION_SIZE) / BIOME_CELL_SIZE;
        int localCellZ = (blockZ - rz * REGION_SIZE) / BIOME_CELL_SIZE;
        if (localCellX < 0 || localCellX >= BIOME_CELLS_PER_REGION ||
            localCellZ < 0 || localCellZ >= BIOME_CELLS_PER_REGION) continue;

        // 查找或插入 biomeTable
        uint8_t idx = BIOME_INDEX_UNKNOWN;
        for (size_t i = 0; i < region->biomeTable.size() && i < 255; i++) {
            if (region->biomeTable[i] == e.name) { idx = (uint8_t)i; break; }
        }
        if (idx == BIOME_INDEX_UNKNOWN) {
            if (region->biomeTable.size() >= 255) continue; // 表满，跳过（极少见）
            region->biomeTable.push_back(e.name);
            idx = (uint8_t)(region->biomeTable.size() - 1);
        }
        region->biomeCells[localCellZ * BIOME_CELLS_PER_REGION + localCellX] = idx;
        region->dirty = true;
    }
}
```

**实现 `GetCachedBiomeName`**（参照 `GetCachedSurfaceHeight` 模式，第 228-249 行）：

```cpp
bool GetCachedBiomeName(int worldX, int worldZ, std::string& outName) {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    int rx = (worldX < 0 ? (worldX + 1) / REGION_SIZE - 1 : worldX / REGION_SIZE);
    int rz = (worldZ < 0 ? (worldZ + 1) / REGION_SIZE - 1 : worldZ / REGION_SIZE);
    uint64_t hash = GetRegionHash(rx, rz);

    auto it = g_loadedRegions.find(hash);
    if (it == g_loadedRegions.end()) {
        g_loadedRegions[hash] = nullptr;
        g_loadQueue.push_back(hash);  // 排队异步加载，下次查询可命中
        return false;
    }
    if (it->second == nullptr) return false;  // 已排队未完成

    RegionData* region = it->second;
    int localCellX = (worldX - rx * REGION_SIZE) / BIOME_CELL_SIZE;
    int localCellZ = (worldZ - rz * REGION_SIZE) / BIOME_CELL_SIZE;
    if (localCellX < 0 || localCellX >= BIOME_CELLS_PER_REGION ||
        localCellZ < 0 || localCellZ >= BIOME_CELLS_PER_REGION) return false;

    uint8_t idx = region->biomeCells[localCellZ * BIOME_CELLS_PER_REGION + localCellX];
    if (idx == BIOME_INDEX_UNKNOWN || idx >= region->biomeTable.size()) return false;
    outName = region->biomeTable[idx];
    return !outName.empty();
}
```

### 3. `src/hooks/PlayerHook.h` — 扫描时采集生物群系

**采集点**（第 786-800 行），在单元格变更时记录生物群系条目（无论是否与上一单元格相同）：

```cpp
// 在扫描循环外声明（第 734 行 static 变量附近）
std::vector<MapCacheManager::BiomeEntry> biomeEntries;
biomeEntries.clear();

// 第 788-800 行修改为：
if (cellX != s_biomeCellX || cellZ != s_biomeCellZ) {
    s_biomeCellX = cellX; s_biomeCellZ = cellZ;
    try {
        auto const& biome = region.getBiome(BlockPos(targetX, topY - 1, targetZ));
        std::string newBiomeName = biome.mHash->getString();
        if (s_biomeName != newBiomeName) {
            s_biomeName = newBiomeName;
            getBiomeTints(s_biomeName, s_cachedGrass, s_cachedFoliage, s_cachedWater);
        }
        // [新增] 记录此单元格的生物群系（无论是否变化）
        biomeEntries.push_back({cellX, cellZ, newBiomeName});
    } catch (...) {
        if (!s_biomeName.empty()) { s_biomeName = ""; }
    }
}
```

**传递给缓存**（第 860-864 行异步线程内，`UpdateFromScan` 调用后）：

```cpp
std::thread([asyncX, asyncZ, asyncColors, asyncHeights, biomeEntries]() {
    MapCacheManager::UpdateFromScan(asyncX, asyncZ, asyncColors, asyncHeights);
    MapCacheManager::UpdateBiomesFromScan(biomeEntries);  // [新增]
    delete[] asyncColors;
    delete[] asyncHeights;
}).detach();
```

**注意**：`biomeEntries` 按值捕获（移动语义），线程安全（`UpdateBiomesFromScan` 内部加锁）。典型一次完整扫描产生 ~4096 条目（仅单元格变更时记录），未加载单元格不记录。

### 4. `src/hooks/DX11Hook.h` — 悬停显示优先查缓存

**修改查询逻辑**（第 1452-1463 行），缓存优先 → 实时回退：

```cpp
std::string rawName;
bool ok = false;
// [优先级1] 缓存生物群系（已扫描区域，区块可能已卸载）
if (MapCacheManager::GetCachedBiomeName(bx, bz, rawName)) {
    ok = true;
}
// [优先级2] 实时查询（仅对当前已加载区块有效，刚进入的新区域）
if (!ok && g_clientInstance) {
    BlockSource* region = g_clientInstance->getRegion();
    if (region) ok = SafeGetBiomeName(*region, bx, queryY, bz, rawName);
}
```

**效果**：
- 已扫描区域（区块已卸载）→ 缓存命中 → 正确显示 ✅
- 刚进入新区域（区块已加载，尚未扫描）→ 缓存未命中 → 实时查询 → 正确显示 ✅
- 从未到访区域 → 缓存未命中 + 实时失败 → 不显示（符合需求3的"未渲染区域不显示"）✅

---

## 复用的现有函数与模式

- `MapCacheManager::GetCachedSurfaceHeight`（`MapCacheManager.cpp:228-249`）— `GetCachedBiomeName` 的模式模板（区域查找+排队加载+局部坐标）
- `MapCacheManager::GetRegionHash`（`MapCacheManager.h:32`）— region 哈希
- 文件向后兼容模式（`MapCacheManager.cpp:50, 160`）— `fileSize >=` 检查，扩展为三段
- `SafeGetBiomeName`（`PlayerHook.h:390`）— 实时回退查询
- `TranslateBiomeName`（`PlayerHook.h:71`）— 生物群系名翻译（无需修改）

---

## 不修改的部分

- **小地图生物群系显示**（`DX11Hook.h:853-857`）：继续用 `translatedBiomeName`（玩家当前生物群系），不受影响
- **玩家当前生物群系更新**（`PlayerHook.h:616-628`）：继续实时查询 `getBiome`
- **扫描的颜色/高度采集逻辑**：完全保留，仅在单元格变更处增加一行 `biomeEntries.push_back`
- **region 纹理渲染管线**：不受影响

---

## 性能分析

| 环节 | 增量开销 | 说明 |
|------|---------|------|
| 扫描采集 | +~4096 次 `push_back`/次完整扫描 | 仅单元格变更时记录，相对 263K 次方块迭代可忽略 |
| `UpdateBiomesFromScan` | 一次锁 + ~4096 次哈希查找 | 单次锁获取，与 `UpdateFromScan` 并列 |
| 文件保存 | +~4.3KB/region | 1.2% 增长，每 2s 一次保存 |
| 文件加载 | +~4.3KB 读/region | 一次性，异步线程 |
| 悬停查询 | O(1) 缓存查找 | **比当前 `getBiome` 实时调用更快** |

满足需求4（性能不受明显影响，加载速度维持原有水平）。

---

## 边界情况处理

| 场景 | 处理 |
|------|------|
| 旧 region 文件（无生物群系段） | `fileSize == 393216` → 跳过生物群系读取，`biomeCells` 全 255 → 下次扫描填充 |
| 新 region 文件（有生物群系段） | 正常读取 |
| region 已加载但区块未加载 | `GetCachedBiomeName` 命中缓存 → 正确显示 |
| region 未加载 | `GetCachedBiomeName` 排队加载，本次返回 false → 下次悬停命中 |
| 从未扫描的区域（区块已加载） | 缓存未命中 → 实时 `SafeGetBiomeName` 回退 |
| 生物群系表满（255 个） | 跳过新条目（极罕见，单 region 不会出现） |
| 地狱维度 | 扫描已跳过（`PlayerHook.h:718`），无缓存；悬停显示已禁用（`canQuery` 检查） |
| 跨维度切换 | `SwitchWorld` 保存当前世界所有 region（含生物群系段）→ 清空 → 新世界独立缓存 |

---

## 验证

**编译**：
```powershell
& "C:\Users\chiyan\scoop\shims\xmake.exe" f -m release -c
& "C:\Users\chiyan\scoop\shims\xmake.exe" -r
```
验证零警告（`/EHa /W4`）。

**功能测试场景**（需求5：多生物群系类型）：
1. **进入世界，扫描周围区域** → 森林、平原、沙漠、雪地、海洋等生物群系在大地图悬停时正确显示
2. **离开已扫描区域，区块卸载** → 大地图悬停该区域 → 生物群系名称仍正确显示（缓存命中）✅ 核心修复点
3. **重启游戏，重新进入同一世界** → 加载旧 region 文件 → 大地图悬停已扫描区域 → 生物群系名称正确显示（从磁盘加载缓存）
4. **进入新区域（未扫描）** → 区块加载后悬停 → 实时查询显示正确生物群系
5. **悬停从未到访区域** → 不显示生物群系（符合需求3）
6. **多生物群系边界** → 鼠标在森林/平原交界移动 → 名称随单元格切换平滑变化
7. **小地图下方生物群系** → 仍显示玩家当前生物群系（不受影响）
8. **性能对比** → 扫描帧率、地图加载速度与修复前无明显差异

**向后兼容验证**：
- 保留旧 region 文件 → 加载正常（颜色/高度正常显示，生物群系在下次扫描后填充）
- 混合新旧 region 文件 → 均正常工作
