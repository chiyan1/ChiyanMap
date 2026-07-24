# 快捷键单行重置/清除/撤销 功能实现文档

> **创建日期**: 2026-07-18  
> **变更类型**: UI 功能增强  
> **影响范围**: 快捷键设置面板、国际化系统、WndProc 输入链  
> **构建验证**: ✅ 零警告，生成 ChiyanMap.dll（1,217,536 字节）

---

## 一、需求背景

在已有的"快捷键设置模块"基础上（参见此前 `levilamina_modernization_build_verify.md` 与 `language_manager_i18n_finalization.md`），用户要求增强单行操作能力：

| 需求 | 说明 |
|------|------|
| **单行重置默认值** | 每个快捷键项目独立添加"重置为默认值"按钮，点击后仅恢复该行至系统预设按键 |
| **单行清除设置** | 每个快捷键项目独立添加"清除设置"按钮，点击后将该行按键设为空值（禁用） |
| **视觉反馈** | 两类操作均需明显视觉反馈 |
| **撤销支持** | 两类操作均需支持撤销 |

---

## 二、技术设计

### 2.1 核心数据结构

#### `HotkeyBindings::Defaults()` 单一来源

在 [src/state/MapRenderState.h](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h) 中新增静态方法，集中维护系统预设默认值，供"单行重置"和"全部重置"共用，避免值重复：

```cpp
struct HotkeyBindings {
    int openBigMap        = 0x4D; // M
    int openWaypointMgr   = 0x55; // U
    int toggleMinimap     = 0x4E; // N
    int toggleMinimapShape= 0x59; // Y
    int toggleMinimapRot  = 0x4A; // J

    // 系统预设默认值 (单一来源)
    static constexpr HotkeyBindings Defaults() {
        return { 0x4D, 0x55, 0x4E, 0x59, 0x4A };
    }
};
```

#### 跨线程撤销标志

由于 WndProc 与渲染线程分离，使用原子布尔传递 Ctrl+Z 撤销请求：

```cpp
// [快捷键增强] Ctrl+Z 撤销请求标志 (由 WndProc 设置，渲染线程消费)
inline std::atomic<bool> g_hotkeyUndoRequested{false};
```

### 2.2 撤销系统设计

**单级撤销**（符合快捷键场景的轻量需求）：

- 每次重置/清除/重绑操作前，记录 `{target指针, prevValue}` 到撤销栈
- "全部重置"作为一次批量操作，压入 `vector<UndoChange>` 而非 5 次单独操作
- 撤销栈仅保留最近一次操作（后入覆盖前入）
- 撤销后再撤销 = 无操作（按钮变灰）

```cpp
struct UndoChange { int* target; int prevValue; };
struct UndoEntry  { std::vector<UndoChange> changes; };
static std::optional<UndoEntry> s_undo;  // 单级
```

### 2.3 闪烁反馈系统

通过颜色插值（color lerp）实现 1.5 秒渐隐动画：

```cpp
constexpr float kFlashDuration = 1.5f;
struct FlashEntry { int* target; float timeLeft; ImVec4 color; };
static std::vector<FlashEntry> s_flashes;

// 渲染时计算当前颜色
float t = std::clamp(entry.timeLeft / kFlashDuration, 0.0f, 1.0f);
ImVec4 curCol = ImLerp(normalColor, entry.color, t);
```

- **重置成功**：绿色闪烁
- **清除成功**：红色闪烁  
- **撤销成功**：蓝色闪烁（应用于所有被还原的行）

### 2.4 状态栏反馈

底部 3 秒文本提示，最后 0.5 秒淡出：

| 操作 | 文本（zh_CN） | 颜色 |
|------|---------------|------|
| 重置 | 已重置为默认值 | 绿色 |
| 清除 | 已清除按键 (已禁用) | 红色 |
| 撤销 | 已撤销操作 | 蓝色 |

---

## 三、UI 布局

### 3.1 窗口规格

- **窗口大小**：600 × 360（原 440 × 280）
- **标题**：本地化的 `HOTKEY_SETTINGS_TITLE`
- **关闭键**：`Esc` 或 `U`（沿用原快捷键面板入口键）

### 3.2 四列表格布局

使用 ImGui Columns API 实现：

| 列 | 标题 | 宽度占比 | 内容 |
|----|------|----------|------|
| 1 | 动作 | 40% | 本地化的动作名（如"切换大地图"） |
| 2 | 按键 | 22% | 4 状态按键按钮（监听/闪烁/禁用/正常） |
| 3 | 重置 | 19% | "重置"按钮（已为默认值时禁用） |
| 4 | 清除 | 19% | "清除"按钮（已为 0 时禁用） |

### 3.3 按键按钮四状态

```
┌──────────────────────────────────┐
│  [监听态]  橙色 + "按下任意键..."  │  g_listeningHotkey == ptr
│  [闪烁态]  插值色 + 按键名        │  在 s_flashes 中
│  [禁用态]  暗红底 + 红字"已禁用"  │  *ptr == 0
│  [正常态]  默认底色 + 按键名      │  其他
└──────────────────────────────────┘
```

### 3.4 工具栏

- **全部重置**：批量操作，仅当 `anyChanged` 为 true 时启用，绿色闪烁全部行
- **撤销**：蓝色按钮，仅当 `s_undo.has_value()` 时启用
- 鼠标悬停显示 Tooltip 说明

---

## 四、Ctrl+Z 撤销链路

### 4.1 WndProc 端（输入线程）

在 [src/hooks/DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h) 的 WndProcHook 中，于 ImGui 处理之后、热键触发之前插入：

```cpp
// [快捷键增强] Ctrl+Z 撤销 (仅在快捷键设置面板打开且非监听态时)
if (!isTyping && uMsg == WM_KEYDOWN && 
    MapRenderState::showHotkeySettings && 
    MapRenderState::g_listeningHotkey == nullptr) {
    bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    if (ctrlDown && wParam == 'Z') {
        MapRenderState::g_hotkeyUndoRequested.store(true);
        return 1;  // 阻止 Z 键触发已绑定的快捷键
    }
}
```

**关键点**：
- 必须在监听模式（重绑）下禁用 Ctrl+Z，避免与按键捕获冲突
- `return 1` 阻止 Z 键冒泡到热键触发逻辑
- ImGui 已在前面收到此事件（如焦点在输入框则 `isTyping=true` 会被跳过）

### 4.2 渲染端（UI 线程）

`RenderHotkeySettingsWindow()` 开头消费标志：

```cpp
if (MapRenderState::g_hotkeyUndoRequested.exchange(false)) {
    performUndo();  // 复用与撤销按钮相同的逻辑
}
```

---

## 五、国际化支持

### 5.1 新增 7 个 i18n 键

在 [src/state/LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp) 的 `kHotkeyActionExtras` 映射中，覆盖全部 15 种语言：

| 键 | 用途 | zh_CN 示例 |
|----|------|-----------|
| `HOTKEY_RESET` | 重置按钮文本 | 重置 |
| `HOTKEY_CLEAR` | 清除按钮文本 | 清除 |
| `HOTKEY_DISABLED` | 禁用态显示文本 | 已禁用 |
| `HOTKEY_UNDO` | 撤销按钮文本 | 撤销 |
| `HOTKEY_STATUS_RESET` | 重置状态文本 | 已重置为默认值 |
| `HOTKEY_STATUS_CLEARED` | 清除状态文本 | 已清除按键 (已禁用) |
| `HOTKEY_STATUS_UNDONE` | 撤销状态文本 | 已撤销操作 |

### 5.2 合并策略（非破坏性）

沿用既有 `kUiExtras` 的合并逻辑，**不覆盖用户已有自定义**：

```cpp
auto hkExtrasIt = kHotkeyActionExtras.find(langCode);
if (hkExtrasIt != kHotkeyActionExtras.end()) {
    for (const auto& [k, v] : hkExtrasIt->second) {
        if (!j.contains(k)) j[k] = v;  // 仅在缺失时写入
    }
}
```

---

## 六、关键代码位置

### 6.1 MapRenderState.h

- **第 53-56 行**：`Defaults()` 静态方法
- **第 62 行**：`g_hotkeyUndoRequested` 原子标志

### 6.2 LanguageManager.cpp

- **`kHotkeyActionExtras` 映射**：7 键 × 15 语言
- **合并逻辑**：紧随 `kUiExtras` 合并块之后

### 6.3 DX11Hook.h

- **WndProc Ctrl+Z 守卫**：位于监听模式块之后、热键触发之前
- **`RenderHotkeySettingsWindow()` 完整重写**：
  - 静态 UI 状态：`s_undo`、`s_flashes`、`s_status`
  - 辅助 Lambda：`addFlash`、`setStatus`、`pushUndo`、`performUndo`
  - `renderRow` Lambda：4 列渲染单行
  - Ctrl+Z 标志消费
  - 5 行快捷键渲染调用
  - 工具栏（全部重置 / 撤销）
  - 状态栏（底部 3 秒淡出文本）

---

## 七、禁用语义与现有触发逻辑的兼容

### 7.1 值 0 的天然语义

`HotkeyBindings` 中所有字段为 `int`，0 自然作为合法值存储。

### 7.2 触发逻辑无需修改

现有热键触发代码形如：

```cpp
if (wParam == g_hotkeys.openBigMap) { /* ... */ }
```

由于 `WM_KEYDOWN` 的 `wParam` 永远不会是 0（虚拟键码 0 不存在），**值 0 的热键天然永远不会被触发**，等同于禁用。无需修改任何触发代码即可支持"清除设置"。

### 7.3 持久化兼容

`LanguageManager::SaveConfig()` / `LoadConfig()` 使用 nlohmann json 存储整数，0 作为合法 int 值自然读写，无需特殊处理。

---

## 八、构建验证

### 8.1 构建命令

```powershell
& "C:\Users\chiyan\scoop\shims\xmake.exe" -r
```

### 8.2 构建结果（首次构建）

```
[100%]: build ok, spent 20.391s
```

- **耗时**：20.391s
- **警告**：0（零警告）
- **产物**：`D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`
- **大小**：1,217,536 字节（1.16 MB）

### 8.3 构建结果（ID 冲突修复后重建）

```
[100%]: build ok, spent 26.984s
```

- **耗时**：26.984s
- **警告**：0（零警告）
- **产物**：`ChiyanMap.dll`，1,219,072 字节
- **构建时间**：2026-07-19 00:14:31

---

## 九、ImGui ID 冲突修复（截图验证后追加）

### 9.1 问题发现

用户提供的两张运行时截图（`PixPin_2026-07-18_23-25-34.png` 与 `PixPin_2026-07-19_00-08-03.png`）通过 Windows.Media.Ocr 识别后，发现 ImGui 报错：

```
MESSAGE FROM DEAR IMGUI
Programmer error: 5 visible items with conflicting ID!
Code should use PushID/PopID loops, or append "##" to same-label identifiers.
Empty string (e.g. Button("") = same ID as parent widget/node.
Use Button("##xx") instead!
```

### 9.2 根因分析

`renderRow` lambda 被 5 行快捷键复用，每行都有：

| 控件 | 标签来源 | 问题 |
|------|----------|------|
| 按键按钮（禁用态） | `HOTKEY_DISABLED` = "已禁用" | 5 行禁用时同标签 |
| 重置按钮 | `HOTKEY_RESET` = "重置" | 5 行始终同标签 |
| 清除按钮 | `HOTKEY_CLEAR` = "清除" | 5 行始终同标签 |

ImGui 使用标签字符串作为 ID 哈希的种子，相同标签在同一 ID 栈下产生相同 ID，导致 5 个按钮 ID 冲突。

### 9.3 修复方案

采用 **`PushID/PopID` + `##suffix` 双重保障**：

```cpp
auto renderRow = [&](const char* actionName, int* bindPtr, int defaultVk) {
    ImGui::PushID(bindPtr);          // 用指针值作为唯一 ID 前缀
    // ... 行内所有控件自动获得唯一 ID ...
    ImGui::PopID();
};
```

同时为按钮标签添加 `##` 后缀（即使标签文本相同，`##` 后的部分不显示但参与 ID 计算）：

```cpp
std::string keyBtnId   = keyName + "##Key";
std::string resetBtnId = std::string(LanguageManager::GetText("HOTKEY_RESET")) + "##Reset";
std::string clearBtnId = std::string(LanguageManager::GetText("HOTKEY_CLEAR")) + "##Clear";
```

### 9.4 附带修复：BOM 累积问题

重建时发现 `LanguageManager.cpp` 开头累积了 **8 个 UTF-8 BOM**（`EF BB BF` × 8），这是历次 Edit 操作的副作用（项目记忆中已记录此问题）。MSVC 报 `C2014: 预处理器命令必须作为第一个非空白空间启动`。

修复脚本：剥离所有前导 BOM，仅保留单个 BOM：

```powershell
$bytes = [System.IO.File]::ReadAllBytes("...LanguageManager.cpp")
# 剥离所有前导 BOM
while ($offset + 2 -lt $bytes.Length -and $bytes[$offset] -eq 0xEF ...) { $offset += 3 }
# 写回单 BOM + 内容
```

### 9.5 验证

修复后重新构建：零警告零错误，`ChiyanMap.dll` 1,219,072 字节。部署后 ImGui ID 冲突警告将消失。

---

## 十、用户验证清单

部署 DLL 后，请在游戏中验证以下场景：

### 10.1 单行重置

1. 打开快捷键设置面板（按 `M` 之外的入口键，或设置按钮）
2. 将"切换大地图"重绑为 `P`
3. 点击该行"重置"按钮
4. **预期**：按键恢复为 `M`，按钮变绿闪一下，底部显示"已重置为默认值"
5. 再次点击"重置"：按钮应为禁用态（已是默认值）

### 10.2 单行清除

1. 点击"切换小地图显示"行的"清除"按钮
2. **预期**：按键显示为红色"已禁用"，按钮红闪，底部显示"已清除按键 (已禁用)"
3. 关闭面板，按 `N` 键
4. **预期**：小地图不再切换（热键已禁用）
5. 重新打开面板，该行"清除"按钮应为禁用态

### 10.3 撤销（按钮）

1. 执行任意重置或清除操作
2. 点击工具栏"撤销"按钮
3. **预期**：被改的行恢复原值，蓝闪一下，底部显示"已撤销操作"
4. 再次点击"撤销"：按钮应为禁用态（无可撤销）

### 10.4 撤销（Ctrl+Z）

1. 执行任意重置或清除操作
2. 按 `Ctrl+Z`
3. **预期**：同 10.3，撤销生效
4. 在监听模式（点击按键按钮等待输入）下按 `Ctrl+Z`
5. **预期**：撤销不触发，Z 键被当作重绑输入捕获

### 10.5 全部重置批量撤销

1. 修改多个快捷键（如 3 个）
2. 点击"全部重置"
3. **预期**：5 行全部绿色闪烁，全部恢复默认
4. 点击"撤销"
5. **预期**：5 行全部恢复为步骤 1 后的值（批量撤销）

### 10.6 持久化

1. 清除某个快捷键，关闭面板，退出游戏
2. 重新进入游戏
3. **预期**：该快捷键仍为禁用状态
4. 打开面板，重置该快捷键，关闭面板，退出游戏
5. **预期**：重置后的值被保存

### 10.7 ImGui ID 冲突已消除

1. 部署修复后的 DLL，打开快捷键设置面板
2. **预期**：不再出现 "5 visible items with conflicting ID!" 警告
3. 5 行重置按钮、5 行清除按钮均能独立点击，无串扰

---

## 十一、设计权衡

### 11.1 为何使用单级撤销而非多级？

- 快捷键场景下，用户通常只需"撤回刚才那一下"
- 多级撤销需要维护操作栈，增加状态复杂度与 UI 认知负担
- 单级 + 批量（全部重置视为一次操作）已覆盖 99% 场景

### 11.2 为何用静态局部而非全局状态？

`RenderHotkeySettingsWindow()` 内的 `static` 局部变量：
- 生命周期与进程一致，自动跨帧保持
- 作用域限制在渲染函数内，不污染全局命名空间
- 无需在 MapRenderState.h 中暴露 UI 实现细节

### 11.3 为何用原子布尔而非事件队列？

- Ctrl+Z 是单比特信息（请求/无请求），无需携带数据
- 原子布尔的 `exchange(false)` 天然实现"消费即清除"
- 避免队列的内存分配与同步开销

### 11.4 为何 `Defaults()` 用 `constexpr` 而非 `const`？

- `constexpr` 允许编译期求值，零运行时开销
- 可用于 `static_assert` 等编译期断言
- 与 C++20 风格一致

---

## 十二、已知限制

1. **撤销不跨会话**：撤销栈为进程内静态状态，游戏重启后清空（合理行为，避免跨会话语义混乱）
2. **闪烁不跨会话**：同上
3. **Ctrl+Z 不与全局撤销系统集成**：本模块独立维护撤销，不影响其他 UI（如路径点管理器）
4. **重置/清除按钮的禁用判断基于当前值**：若用户重绑为恰好等于默认值的键，重置按钮会显示为禁用（合理，因已是默认值）

---

## 十三、相关文件

| 文件 | 变更类型 | 说明 |
|------|----------|------|
| [src/state/MapRenderState.h](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h) | 修改 | 新增 `Defaults()` 与 `g_hotkeyUndoRequested` |
| [src/state/LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp) | 修改 | 新增 `kHotkeyActionExtras` 映射与合并逻辑 |
| [src/hooks/DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h) | 修改 | WndProc Ctrl+Z 守卫 + `RenderHotkeySettingsWindow()` 完整重写 |
| `.trae/documents/hotkey_reset_clear_undo.md` | 新建 | 本文档 |

---

## 十四、部署说明

由于沙箱限制，无法自动写入游戏目录。请手动执行：

```powershell
Copy-Item "D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll" `
          "D:\我的世界\基岩版游戏\levilauncher.exe\versions\1.26.20.04\mods\ChiyanMap\ChiyanMap.dll" `
          -Force
```

部署后重启游戏，按 `M` 打开大地图 → 设置 → 快捷键设置，验证上述功能。
