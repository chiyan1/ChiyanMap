# LanguageManager i18n 现代化收尾计划

## Context

ChiyanMap 项目的 LeviLamina API 现代化改造已推进 3/4：
- ✅ 版本比较类型化（`"1.26.20-4"_version` 字面量）
- ✅ MapCacheManager 协程化（`ll::coro::CoroTask` + `ThreadPoolExecutor` + `std::binary_semaphore`）
- ✅ 路径访问器 + 一次性迁移（`MigrateLegacyPaths()` 已在 `ChiyanMap::load()` 首行；`WaypointManager`、`MapCacheManager` 已改用 `getDataDir()`）

**唯一剩余工作**：`src/state/LanguageManager.cpp`（1052 行）仍是原始状态，包含三大问题：
1. `g_defaultJsonFiles`（19–740 行）+ `biomeDict`（886–911 行）双轨翻译源
2. `GetText()` 中 14 个 `if (key == "XXX")` 硬编码分支（914–1042 行）
3. 6 处硬编码路径 + 1 处 `printf` 日志

目标：将所有硬编码翻译合并入磁盘 JSON 文件，简化 `GetText()` 为纯缓存 + `ll::i18n::getInstance().get()`，路径改用 `getLangDir()/getConfigDir()`，日志改用 `getLogger()`。

---

## 改动 1：新增 `BuildExtraTranslations()` 静态辅助函数

### 位置
插入到 `g_defaultJsonFiles` 定义（740 行）之后、`Init()` 定义（742 行）之前。

### 设计
```cpp
// 返回某语言的 BIOME_* + 额外 UI key 合集，用于合并入磁盘 JSON 文件。
// 数据来源：原 GetText() 中的 biomeDict + 14 个 if-else 分支，原样搬迁。
// 注意：LOG_VERSION_* 不在此处（已在 g_defaultJsonFiles 中）。
static std::unordered_map<std::string, std::string> BuildExtraTranslations(const std::string& langCode) {
    static const std::unordered_map<std::string, std::unordered_map<std::string, std::string>> kExtra = {
        {"en", {
            {"BIOME_PLAINS","Plains"}, /* ... 36 个 BIOME_* ... */ {"BIOME_UNKNOWN","Unknown Biome"},
            {"TEXT_SCALE","Text Scale"},
            {"MINIMAP_SCALE","Scale"},
            {"MINIMAP_POS_SETTINGS","Minimap Layout Settings"},
            {"X_OFFSET","X Offset"},
            {"Y_OFFSET","Y Offset"},
            {"SAVE_AND_EXIT","Save and Exit"},
            {"DONT_SAVE","Don't Save"},
            {"EDIT_MINIMAP_POS","Edit Minimap Layout"},
            {"NATIVE_IME_TOOLTIP","Click to open native input box to type Chinese/etc."},
            {"DEFAULT_POS","Restore Default"},
            {"TOP_LEFT_POS","Move to Top-Left"},
            {"ROTATE_MINIMAP","Rotate Minimap"},
            {"SETTINGS_TOOLTIP","Open Settings"}
        }},
        {"zh_CN", { /* 中文翻译 */ }},
        {"zh_TW", { /* 繁体翻译 */ }},
        {"ja", { /* 日文翻译 */ }},
        {"ko", { /* 韩文翻译 */ }},
        {"ru", { /* 俄文翻译 */ }},
        {"fr", { /* 法文翻译 */ }},
        {"de", { /* 德文翻译 */ }},
        {"pt_BR", { /* 葡萄牙语翻译 */ }},
        {"it", { /* 意大利语翻译 */ }},
        {"es", { /* 西班牙语翻译（仅 BIOME_*，UI key 不存在 → 回退 en） */ }},
        {"id", { /* 印尼语翻译 */ }},
        {"th", { /* 泰文翻译 */ }},
        {"tr", { /* 土耳其语翻译 */ }},
        {"uk", { /* 乌克兰语翻译 */ }},
        {"vi", { /* 越南语翻译 */ }}
    };
    auto it = kExtra.find(langCode);
    return it != kExtra.end() ? it->second : std::unordered_map<std::string, std::string>{};
}
```

### 数据搬迁规则
- **BIOME_***（36 个 key × 16 种语言）：原样从 `biomeDict` 搬入（含 es）
- **TEXT_SCALE / MINIMAP_SCALE / MINIMAP_POS_SETTINGS / X_OFFSET / Y_OFFSET / SAVE_AND_EXIT / DONT_SAVE / EDIT_MINIMAP_POS / DEFAULT_POS / TOP_LEFT_POS**（10 个 key）：原 if-else 仅对 zh_CN/zh_TW 提供翻译，其他语言走 en fallback → 在 `kExtra` 中只填 zh_CN/zh_TW/en 三种语言的条目；其他语言调用 `BuildExtraTranslations` 返回不含这些 key 的 map，磁盘 JSON 文件中也不写入，`ll::i18n.get(key, lang)` 自动回退到 en
- **NATIVE_IME_TOOLTIP / ROTATE_MINIMAP / SETTINGS_TOOLTIP**（3 个 key）：原 if-else 对 15 种语言都有翻译（不含 es）→ 在 `kExtra` 中为 15 种语言填入条目（es 不填，回退 en）
- **LOG_VERSION_***（5 个 key）：**不搬入** `kExtra`。已在 `g_defaultJsonFiles` 中，且原 if-else 分支会覆盖 JSON 数据（这是 pre-existing bug：非 zh_CN/zh_TW 语言调用 LOG_VERSION_* 时从 if-else 拿到英文，绕过了 JSON 中已存在的本地化翻译）。删除 if-else 后所有语言将正确使用 JSON 中的 LOG_VERSION_* 翻译 — **顺带修复此 bug**

### es 语言处理
- `g_defaultJsonFiles` 不含 es → `Init()` 不会为 es 写 JSON 文件
- `BuildExtraTranslations("es")` 返回非空 map（仅 BIOME_*），但无处写入
- 用户切换到 es 时，`ll::i18n.get(key, "es")` 找不到 `es.json` → 回退到 en — **与现状一致**

---

## 改动 2：`Init()` 重写（替换 742–762 行）

### 新实现
```cpp
void Init() {
    auto& self   = chiyan_map::ChiyanMap::getInstance().getSelf();
    auto  langDir = self.getLangDir();
    std::filesystem::create_directories(langDir);

    // 写入/合并磁盘 JSON 文件：
    // - 文件不存在：从 g_defaultJsonFiles 模板创建，合并 BuildExtraTranslations
    // - 文件存在：读取已有内容，补充缺失 key（保留用户自定义修改）
    for (const auto& [langCode, jsonContent] : g_defaultJsonFiles) {
        auto filePath = langDir / (langCode + ".json");
        json j;
        if (std::filesystem::exists(filePath)) {
            std::ifstream in(filePath);
            if (in.is_open()) {
                try { in >> j; } catch (...) { j = json::parse(jsonContent); }
            }
        } else {
            j = json::parse(jsonContent);
        }
        for (const auto& [k, v] : BuildExtraTranslations(langCode)) {
            if (!j.contains(k)) j[k] = v;  // 仅补充缺失 key，不覆盖
        }
        std::ofstream out(filePath);
        if (out.is_open()) out << j.dump(4);
    }

    if (auto res = ll::i18n::getInstance().load(langDir); !res) {
        self.getLogger().error("i18n load failed: {}", res.error().message());
    }

    ScanLanguages();
    LoadConfig();
}
```

### 关键差异
| 项 | 旧实现 | 新实现 |
|----|--------|--------|
| 路径 | `"mods/ChiyanMap/lang"` 硬编码 | `self.getLangDir()` |
| JSON 写入 | 仅在文件不存在时写 `jsonContent` 原文 | 文件不存在时创建+合并 extras；存在时读+补充缺失 key |
| 日志 | `printf("[ChiyanMap] i18n load failed!\n")` | `self.getLogger().error("i18n load failed: {}", ...)` |
| 翻译源 | `g_defaultJsonFiles` + biomeDict + if-else 三轨 | `g_defaultJsonFiles` + `BuildExtraTranslations` 合并后落盘 |

### 为什么合并而非覆盖现有 JSON
- 现有用户的 `mods/ChiyanMap/lang/*.json` 文件可能含手动自定义（如改 `BIGMAP_TITLE` 文案）
- 覆盖会丢失用户修改，破坏体验
- 合并策略：仅在 key 缺失时补充，已存在 key 保留 — 兼顾向后兼容与功能补全

---

## 改动 3：`ScanLanguages()` 路径替换（787 行）

```cpp
// 旧
for (const auto& entry : std::filesystem::directory_iterator("mods/ChiyanMap/lang")) {
// 新
auto langDir = chiyan_map::ChiyanMap::getInstance().getSelf().getLangDir();
for (const auto& entry : std::filesystem::directory_iterator(langDir)) {
```

---

## 改动 4：`LoadConfig()` / `SaveConfig()` 路径替换（813、866 行）

```cpp
// 旧
std::string filePath = "mods/ChiyanMap/config.json";
// 新
auto filePath = chiyan_map::ChiyanMap::getInstance().getSelf().getConfigDir() / "config.json";
```

`LoadConfig` 与 `SaveConfig` 各替换一处。`std::ifstream`/`std::ofstream` 接受 `std::filesystem::path`，无需 `.string()`。

---

## 改动 5：`GetText()` 简化（替换 884–1051 行）

### 新实现
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

### 删除内容
- `biomeDict` 静态局部变量（886–911 行，16 种语言 × 36 BIOME_* key）
- 全部 14 个 `if (key == "XXX")` 分支（914–1042 行），含：
  - TEXT_SCALE / MINIMAP_SCALE / MINIMAP_POS_SETTINGS / X_OFFSET / Y_OFFSET
  - SAVE_AND_EXIT / DONT_SAVE / EDIT_MINIMAP_POS
  - NATIVE_IME_TOOLTIP / DEFAULT_POS / TOP_LEFT_POS
  - ROTATE_MINIMAP / SETTINGS_TOOLTIP
  - LOG_VERSION_MISMATCH / LOG_VERSION_STRICT / LOG_VERSION_ABORT / LOG_VERSION_PASS / LOG_VERSION_UNKNOWN

### 行为变化
| 场景 | 旧行为 | 新行为 |
|------|--------|--------|
| BIOME_PLAINS + zh_CN | 从 biomeDict 取 "平原" | 从 zh_CN.json 取 "平原"（Init 时合并） |
| TEXT_SCALE + ja | if-else 走 en fallback "Text Scale" | ja.json 中无此 key → ll::i18n 回退 en → "Text Scale" |
| LOG_VERSION_PASS + de | if-else 走 en "[CRITICAL]..." | 从 de.json 取 "[KRITISCH]..."（**bug fix**） |
| 未知 key | 走 ll::i18n.get() → 空 string_view → 缓存空串 | 返回 key 本身 |

### 兼容性
- `g_translationCache` 非锁访问保持不变（pre-existing 设计）
- `LoadLanguage()` 清空缓存的逻辑保持不变
- 缓存命中后返回 `c_str()` 指针的有效期：引用稳定（`std::unordered_map` 节点稳定），与旧实现一致

---

## 改动 6：头文件依赖

### 新增 include（LanguageManager.cpp 顶部）
```cpp
#include "mod/ChiyanMap.h"  // 为 getInstance().getSelf() 访问器
```

### 已有 include（无需改）
- `#include <ll/api/i18n/I18n.h>` ✓
- `#include <nlohmann/json.hpp>` ✓
- `#include <filesystem>` ✓
- `#include <fstream>` ✓
- `#include <windows.h>` ✓（LoadConfig 用 `GetUserDefaultUILanguage`）

### 循环包含风险
- `ChiyanMap.h` 已 include `state/LanguageManager.h`、`state/MapCacheManager.h`、`state/WaypointManager.h`
- `LanguageManager.cpp` include `mod/ChiyanMap.h` 是 .cpp 层引用，不会形成头文件循环
- 与 `MapCacheManager.cpp`、`WaypointManager.cpp` 已验证的 include 模式一致

---

## 验证方案

### 1. 编译验证
```powershell
xmake f -m release
xmake
```
预期：零警告（项目 `/W4`），生成 `ChiyanMap.dll`。

### 2. 功能验证（手动）

#### i18n 合并
- 删除 `mods/ChiyanMap/lang/*.json` 全部 15 个文件 → 重启 → 验证 15 个 JSON 重新生成
- 检查 `zh_CN.json` 包含 `BIOME_PLAINS: "平原"`、`TEXT_SCALE: "文本缩放比例"`、`LOG_VERSION_PASS: "游戏客户端版本验证通过"`
- 检查 `ja.json` 包含 `NATIVE_IME_TOOLTIP`、`ROTATE_MINIMAP` 的日文翻译
- 检查 `de.json` 的 `LOG_VERSION_*` 为德文（"[KRITISCH]..."），不再是英文
- 在 `zh_CN.json` 手动改 `BIGMAP_TITLE` 为 "测试" → 重启 → 验证保留（不被覆盖）
- 在 `zh_CN.json` 删除 `TEXT_SCALE` 行 → 重启 → 验证该 key 被重新补回 "文本缩放比例"

#### 路径迁移
- 保留旧 `mods/ChiyanMap/config.json`、`mods/ChiyanMap/lang/` → 启动游戏 → 检查 `mods/ChiyanMap/config/config.json` 生成、`mods/ChiyanMap/lang/` 保留（lang 路径未变）

#### GetText 兜底
- 临时调用 `LanguageManager::GetText("NON_EXISTENT_KEY")` → 验证返回 `"NON_EXISTENT_KEY"` 而非空串

#### 回归
- 切换语言至 zh_CN/zh_TW/ja/ko/fr/de/pt_BR/it/id/th/tr/uk/vi/ru → 小地图侧栏、生物群系标签、设置面板、版本日志全部正确本地化
- M/U/N/Y/J 热键、ESC 关闭 UI、小地图拖拽缩放、路径点增删改查、跨维度切换 → 全部正常

### 3. 日志核验
启动日志应包含：
```
[ChiyanMap] 游戏客户端版本验证通过: 1.26.20.04
```
不再出现：
```
[ChiyanMap] i18n load failed!   ← printf 旧日志
```

---

## Assumptions & Decisions

1. **保留 `g_defaultJsonFiles` 作为内存模板**：不删除此数据结构，因为它是新 JSON 文件创建的唯一来源。`BuildExtraTranslations` 作为补充数据源，在 Init 时合并落盘。
2. **合并而非覆盖现有 JSON**：保护用户自定义修改，仅在 key 缺失时补充。
3. **LOG_VERSION_* 不入 `BuildExtraTranslations`**：避免与 `g_defaultJsonFiles` 重复，顺带修复非 zh_CN/zh_TW 语言拿到英文的 pre-existing bug。
4. **es 语言不写入磁盘**：与现状一致，`ll::i18n` 自动回退到 en。
5. **`g_translationCache` 线程安全**：保持 pre-existing 非锁设计，本次不引入锁，避免改动 GetText 性能特征。
6. **未知 key 返回 key 本身**：便于运行时发现遗漏翻译，优于返回空串。
7. **不改造 LoadConfig/SaveConfig 为 `ll::config::saveConfig/loadConfig` + Reflection**：当前手写 JSON 读写工作正常，改造收益有限，留作后续。

---

## 不在范围

- **客户端事件系统**（原改进点 #4）：用户已确认跳过
- **`g_defaultJsonFiles` 数据瘦身**：可考虑后续将 JSON 模板移至资源文件，本次保留内存常量
- **`ll::config` 模板化配置**：留作后续
