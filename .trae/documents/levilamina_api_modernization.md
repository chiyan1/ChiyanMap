# LeviLamina API 现代化改造计划

## Context

ChiyanMap 项目当前对 LeviLamina 框架 API 的使用存在若干偏离官方推荐路径的地方：
1. **i18n**：`Init()` 已正确调用 `ll::i18n::getInstance().load()`，但 `GetText()` 内部仍保留 1052 行硬编码的 `biomeDict` 与 14 个 `if-else` 分支（TEXT_SCALE、MINIMAP_SCALE 等），与 `g_defaultJsonFiles` 双轨制，违反"翻译统一走 JSON 文件"原则。
2. **MapCacheManager IOWorker**：自建 `std::thread + sleep_for(20ms)` 轮询，未使用官方协程 + 线程池执行器。
3. **版本比较**：`ChiyanMap.cpp:66-81` 手动 `fmt::format` 拼字符串比较，未使用 `ll::data::Version` 的类型安全比较。
4. **客户端事件**：经深入调研与用户确认，**跳过**。现有 `WndProc` 钩子承担 ImGui/IME/鼠标拦截五项关键职责，LeviLamina `KeyInputEvent/MouseInputEvent` 是更高层抽象，无法替代核心逻辑，强行引入会形成两套并行输入系统。
5. **路径硬编码**：`mods/ChiyanMap/{lang,config.json,cache,waypoints}` 共 10 处硬编码字符串，未使用 `Mod::getLangDir()/getConfigDir()/getDataDir()` 访问器。

目标：应用 #1/#2/#3/#5 四项改进，使代码遵循 LeviLamina 官方 API 路径，同时保留现有用户数据（通过一次性路径迁移）。

## 改动概览

| 文件 | 改动类型 | 说明 |
|------|---------|------|
| `src/mod/ChiyanMap.cpp` | 修改 | 版本比较改用 `"1.26.20-4"_version` 字面量；新增 `MigrateLegacyPaths()` 一次性迁移 |
| `src/mod/ChiyanMap.h` | 修改 | 暴露 `getSelf()` 已存在，无需改 |
| `src/state/LanguageManager.cpp` | 修改 | i18n 硬编码合并入 JSON；路径改用 `getLangDir()/getConfigDir()`；GetText 简化 |
| `src/state/MapCacheManager.h` | 修改 | `std::thread*` 替换为协程句柄；新增 `std::binary_semaphore` 用于 Shutdown 同步 |
| `src/state/MapCacheManager.cpp` | 修改 | `IOWorker` 改写为 `IOWorkerCoro` 协程；`Init/Shutdown` 改用 `ll::coro::keepThis + ThreadPoolExecutor`；路径改用 `getDataDir()/"cache"` |
| `src/state/WaypointManager.cpp` | 修改 | 路径改用 `getDataDir()/"waypoints"` |
| `xmake.lua` | 不变 | `levilamina` 已含 coro/thread/data/i18n 子模块 |

---

## 改动 1：i18n 硬编码合并入 JSON（LanguageManager.cpp）

### 现状
- `g_defaultJsonFiles`（19-740 行）：15 种语言，每种 38 个通用 key
- `biomeDict`（886-911 行）：硬编码 36 个 `BIOME_*` key × 16 种语言（含 es 但 g_defaultJsonFiles 无 es）
- `GetText` 的 14 个 `if (key == "XXX")` 分支（914-1042 行）：`TEXT_SCALE / MINIMAP_SCALE / MINIMAP_POS_SETTINGS / X_OFFSET / Y_OFFSET / SAVE_AND_EXIT / DONT_SAVE / EDIT_MINIMAP_POS / NATIVE_IME_TOOLTIP / DEFAULT_POS / TOP_LEFT_POS / ROTATE_MINIMAP / SETTINGS_TOOLTIP / LOG_VERSION_*`
- `GetText` 末尾（1044-1050 行）才回退到 `ll::i18n::getInstance().get()`

### 改造
1. **抽取硬编码翻译为静态辅助函数**：
   ```cpp
   // 返回某语言的 BIOME_* + 额外 UI key 合集
   static std::unordered_map<std::string, std::string> BuildExtraTranslations(const std::string& langCode);
   ```
   内部把现有 `biomeDict` 与 14 个 `if-else` 分支的数据原样搬入，按 `langCode` 返回扁平 `map<string,string>`。

2. **`Init()` 重写**（替换 742-762 行）：
   ```cpp
   void Init() {
       auto langDir = chiyan_map::ChiyanMap::getInstance().getSelf().getLangDir();
       std::filesystem::create_directories(langDir);

       for (const auto& [langCode, jsonContent] : g_defaultJsonFiles) {
           auto filePath = langDir / (langCode + ".json");
           if (!std::filesystem::exists(filePath)) {
               json j = json::parse(jsonContent);
               for (const auto& [k, v] : BuildExtraTranslations(langCode)) j[k] = v;
               std::ofstream out(filePath);
               out << j.dump(4);
           }
       }

       auto& logger = chiyan_map::ChiyanMap::getInstance().getSelf().getLogger();
       if (auto res = ll::i18n::getInstance().load(langDir); !res) {
           logger.error("i18n load failed: {}", res.error().message());
       }
       ScanLanguages();
       LoadConfig();
   }
   ```

3. **`ScanLanguages()` 路径替换**（787 行）：`"mods/ChiyanMap/lang"` → `chiyan_map::ChiyanMap::getInstance().getSelf().getLangDir()`

4. **`LoadConfig()`/`SaveConfig()` 路径替换**（813、866 行）：`"mods/ChiyanMap/config.json"` → `chiyan_map::ChiyanMap::getInstance().getSelf().getConfigDir() / "config.json"`

5. **`GetText()` 简化**（替换 884-1051 行）：
   ```cpp
   const char* GetText(const std::string& key) {
       auto it = g_translationCache.find(key);
       if (it != g_translationCache.end()) return it->second.c_str();
       std::string_view sv = ll::i18n::getInstance().get(key, g_currentLanguage);
       if (sv.empty()) {
           // 兜底：未知 key 返回 key 本身，便于发现遗漏
           g_translationCache[key] = key;
       } else {
           g_translationCache[key] = std::string(sv);
       }
       return g_translationCache[key].c_str();
   }
   ```
   删除 `biomeDict` 静态局部变量与全部 `if (key == "XXX")` 分支。

### 注意
- g_defaultJsonFiles 不含 "es"（西班牙语），但 biomeDict 含 es。`BuildExtraTranslations("es")` 返回非空 map，但 `Init()` 不会为 es 写文件（因为 g_defaultJsonFiles 不遍历 es）。GetText 调用 `ll::i18n.get(key, "es")` 时，由于磁盘上无 `es.json`，会回退到 en。这是可接受的行为，与现状一致（现状下 es 也只在 biomeDict 内有翻译，UI 文本仍走 en）。
- `printf` 日志改用 `getLogger()`，遵循官方 IO 规范。
- 保留 `g_translationCache` 的非锁访问（pre-existing 设计），不引入新风险。

---

## 改动 2：MapCacheManager 协程化

### 现状（MapCacheManager.cpp:16-73）
```cpp
std::atomic<bool> g_running{false};
std::thread* g_ioThread = nullptr;

void IOWorker() {
    while (g_running) {
        std::this_thread::sleep_for(20ms);
        // ... 两通道逻辑
    }
}

void Init() { g_running = true; g_ioThread = new std::thread(IOWorker); }
void Shutdown() { g_running = false; g_ioThread->join(); delete g_ioThread; }
```

### 改造
1. **头文件调整**（MapCacheManager.h）：
   ```cpp
   // 移除
   #include <thread>
   // 新增
   #include <semaphore>
   #include "ll/api/coro/CoroTask.h"
   
   //extern 字段调整
   extern std::atomic<bool> g_running;
   extern std::binary_semaphore g_ioDone;  // 替代 g_ioThread
   ```
   删除 `extern std::thread* g_ioThread;`。

2. **cpp 文件改造**：
   ```cpp
   #include "ll/api/coro/CoroTask.h"
   #include "ll/api/coro/SleepAwaiter.h"
   #include "ll/api/thread/ThreadPoolExecutor.h"
   #include "mod/ChiyanMap.h"
   
   std::binary_semaphore g_ioDone{0};
   
   ll::coro::CoroTask<> IOWorkerCoro() {
       auto lastSaveTime = std::chrono::steady_clock::now();
       while (g_running) {
           co_await ll::coro::SleepAwaiter{std::chrono::milliseconds(20)};
           // ... 原 IOWorker 循环体原样保留（两通道逻辑不变）
       }
       co_return;
   }
   
   void Init() {
       g_running = true;
       ll::coro::keepThis(IOWorkerCoro).launch(
           ll::thread::ThreadPoolExecutor::getDefault(),
           [](auto&&) { g_ioDone.release(); }
       );
   }
   
   void Shutdown() {
       g_running = false;
       // 最多等 20ms + 一次 IO 处理时间
       g_ioDone.acquire();
       // ... 后续清理原样保留
   }
   ```

3. **`SwitchWorld` 路径替换**（93 行）：
   ```cpp
   g_cacheDir = (chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir() / "cache" / worldId / ("dim_" + std::to_string(dimensionId))).string() + "/";
   ```
   注意 `std::filesystem::path::operator/` 在 Windows 下返回反斜杠路径，对 `std::ofstream` 无影响；但 `g_cacheDir` 末尾的 `/` 保留以兼容现有 `currentDir + "region_..."` 字符串拼接逻辑。

### 关键 API 确认
- `ll::coro::CoroTask<>`（CoroTask.h:12）
- `co_await ll::coro::SleepAwaiter{duration}`（SleepAwaiter.h:8）
- `ll::coro::keepThis(F)` 返回 CoroTask（CoroTask.h:125）
- `CoroTask::launch(executor, callback)` callback 收到 `ExpectedResult`（CoroTask.h:83）
- `ll::thread::ThreadPoolExecutor::getDefault()` 返回默认线程池执行器

### 风险评估
- 协程在 `g_running = false` 后最多再跑 20ms 一次循环即退出，`g_ioDone.acquire()` 不会长时间阻塞。
- `launch` 内部已 `try/catch`，异常情况下仍会调用 callback 释放信号量。
- `SleepAwaiter` 通过 `Executor::executeAfter(handle, dur)` 调度，无系统线程阻塞。

---

## 改动 3：版本比较类型化（ChiyanMap.cpp）

### 现状（ChiyanMap.cpp:64-86）
```cpp
std::string rawVer = fmt::format("{}.{}.{}-{}", gameVer->major, gameVer->minor, gameVer->patch, gameVer->build.value_or("4"));
// ...
if (rawVer != "1.26.20-4") { /* 拒绝 */ }
```
注意：`1.26.20-4` 按 semver 解析时 `-4` 进入 `preRelease`，而非 `build`。原代码用 `build.value_or("4")` 凑字符串，恰好匹配但语义错位。

### 改造
```cpp
#include "ll/api/data/Version.h"  // 已存在
using namespace ll::data::literals;

constexpr auto kRequiredVersion = "1.26.20-4"_version;

if (gameVer) {
    std::string displayVer = gameVer->to_string();  // "1.26.20-4"
    size_t dashPos = displayVer.find('-');
    if (dashPos != std::string::npos) {
        std::string tail = displayVer.substr(dashPos + 1);
        if (tail.length() == 1) displayVer.replace(dashPos, 1, ".0");
        else                    displayVer.replace(dashPos, 1, ".");
    }

    if (*gameVer != kRequiredVersion) {
        getSelf().getLogger().error("{}: {}", LanguageManager::GetText("LOG_VERSION_MISMATCH"), displayVer);
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_STRICT"));
        getSelf().getLogger().error("{}", LanguageManager::GetText("LOG_VERSION_ABORT"));
        return false;
    }
    getSelf().getLogger().info("{}: {}", LanguageManager::GetText("LOG_VERSION_PASS"), displayVer);
} else {
    getSelf().getLogger().warn("{}", LanguageManager::GetText("LOG_VERSION_UNKNOWN"));
}
```

### API 确认
- `ll::data::Version::operator==`（Version.h:286）由 `<=>` 派生，比较 `major/minor/patch/preRelease`（不含 build）
- `ll::data::literals::operator""_version`（Version.h:319）从字符串字面量构造 Version
- `Version::to_string()`（Version.h:251）输出 `"1.26.20-4"` 格式

---

## 改动 5：路径访问器 + 一次性迁移

### 现状（10 处硬编码）
- `LanguageManager.cpp`：`mods/ChiyanMap/lang` ×4（743、746、756、787）、`mods/ChiyanMap/config.json` ×2（813、866）
- `MapCacheManager.cpp:93`：`mods/ChiyanMap/cache/...`
- `WaypointManager.cpp:86,92,93`：`mods/ChiyanMap/waypoints/...`

### 改造映射
| 旧路径 | 新路径（API） |
|--------|---------------|
| `mods/ChiyanMap/lang` | `getSelf().getLangDir()` |
| `mods/ChiyanMap/config.json` | `getSelf().getConfigDir() / "config.json"` |
| `mods/ChiyanMap/cache/...` | `getSelf().getDataDir() / "cache" / ...` |
| `mods/ChiyanMap/waypoints/...` | `getSelf().getDataDir() / "waypoints" / ...` |

`getLangDir()` 返回 `mods/ChiyanMap/lang`（不变）。其余三项实际路径会变（多了 `config/` 或 `data/` 前缀），需一次性迁移。

### 一次性迁移（ChiyanMap.cpp 新增）
```cpp
static void MigrateLegacyPaths() {
    auto& self = chiyan_map::ChiyanMap::getInstance().getSelf();
    auto modDir    = self.getModDir();
    auto dataDir   = self.getDataDir();
    auto configDir = self.getConfigDir();

    // config.json: mods/ChiyanMap/config.json → mods/ChiyanMap/config/config.json
    auto oldConfig = modDir / "config.json";
    auto newConfig = configDir / "config.json";
    if (std::filesystem::exists(oldConfig) && !std::filesystem::exists(newConfig)) {
        std::filesystem::create_directories(configDir);
        std::filesystem::rename(oldConfig, newConfig);
    }

    // cache: mods/ChiyanMap/cache/ → mods/ChiyanMap/data/cache/
    auto oldCache = modDir / "cache";
    auto newCache = dataDir / "cache";
    if (std::filesystem::exists(oldCache) && !std::filesystem::exists(newCache)) {
        std::filesystem::create_directories(dataDir);
        std::filesystem::rename(oldCache, newCache);
    }

    // waypoints: mods/ChiyanMap/waypoints/ → mods/ChiyanMap/data/waypoints/
    auto oldWp = modDir / "waypoints";
    auto newWp = dataDir / "waypoints";
    if (std::filesystem::exists(oldWp) && !std::filesystem::exists(newWp)) {
        std::filesystem::create_directories(dataDir);
        std::filesystem::rename(oldWp, newWp);
    }
}
```

### 调用时机
在 `ChiyanMap::load()` 中作为**第一行**调用，先于 `LanguageManager::Init()`：
```cpp
bool ChiyanMap::load() {
    MigrateLegacyPaths();
    LanguageManager::Init();
    // ... 版本检查、Hook 注册、MapCacheManager::Init、WaypointManager::Init
}
```

### WaypointManager.cpp 改造
```cpp
void Init() {
    auto wpDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir() / "waypoints";
    std::filesystem::create_directories(wpDir);
}

void SwitchWorld(const std::string& worldId, int dimensionId) {
    {
        std::lock_guard<std::mutex> lock(g_wpMutex);
        auto wpDir = chiyan_map::ChiyanMap::getInstance().getSelf().getDataDir() / "waypoints";
        std::filesystem::create_directories(wpDir);
        g_waypointFile = (wpDir / (worldId + "_dim" + std::to_string(dimensionId) + ".json")).string();
    }
    LoadWaypoints();
}
```

### 头文件依赖
- 在 `LanguageManager.cpp`、`MapCacheManager.cpp`、`WaypointManager.cpp` 顶部 `#include "mod/ChiyanMap.h"`
- 无循环包含风险（ChiyanMap.h 只含 `ll/api/mod/NativeMod.h`，state 头不含 ChiyanMap.h）

---

## 验证方案

### 编译验证
```powershell
xmake f -m release
xmake
```
预期：零警告（项目已 `/W4`），生成 `ChiyanMap.dll`。

### 功能验证（手动）
1. **路径迁移**：保留旧 `mods/ChiyanMap/{config.json,cache/,waypoints/}`，启动游戏 → 检查旧路径已清空、新路径 `mods/ChiyanMap/{config/config.json,data/cache/,data/waypoints/}` 已生成且数据完整。
2. **i18n**：
   - 删除 `mods/ChiyanMap/lang/*.json`，重启 → 验证 15 个 JSON 重新生成且包含 `BIOME_PLAINS`、`TEXT_SCALE` 等新 key。
   - 切换语言至 zh_CN/ja/ko 等 → 验证小地图侧栏、生物群系标签、设置面板文本正确本地化。
   - 在 zh_CN.json 中手动改 `BIGMAP_TITLE` 为自定义文本 → 重启验证保留（不被覆盖）。
3. **协程 IO**：进入世界 → 在 `mods/ChiyanMap/data/cache/<world>/dim_0/` 看到 `region_*.bin` 持续生成；退出世界/卸载模组 → 验证 `Shutdown` 不阻塞、所有 dirty region 已落盘。
4. **版本检查**：
   - 在 1.26.20.04 客户端启动 → 日志输出 `游戏客户端版本验证通过: 1.26.20.04`。
   - 模拟版本不符（修改拦截逻辑或不同版本客户端）→ 日志输出三行 `LOG_VERSION_*` 错误并中止加载。
5. **回归测试**：M/U/N/Y/J 热键、ESC 关闭 UI、小地图拖拽缩放、路径点增删改查、跨维度切换 → 全部正常。

### 日志核验
启动日志应包含：
```
[ChiyanMap] 游戏客户端版本验证通过: 1.26.20.04
```
不再出现：
```
[ChiyanMap] i18n load failed!   ← printf 旧日志
```

---

## 不在范围

- **客户端事件系统**（改进点 #4）：用户已确认跳过。`DX11Hook.h` 的 `WndProcHook` 与 `PlayerHook.h` 的输入相关 Hook（`LocalPlayerApplyTurnDeltaHook`、`PlayerInventorySelectSlotHook` 等）保持现状。
- **g_translationCache 线程安全**：pre-existing 设计，本次不引入锁，避免改动 GetText 性能特征。
- **Config 改用 `ll::config::saveConfig/loadConfig` + Reflection**：当前手写 JSON 读写工作正常，改造收益有限，留作后续。
