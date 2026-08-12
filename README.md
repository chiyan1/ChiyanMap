# ChiyanMap（炽焰地图）

> 一款基于 **LeviLamina** 原生 Mod 框架、在客户端（渲染线程）运行的 Minecraft基岩版 地图 / 雷达一体化增强模组。
> 通过 DirectX 11 渲染管线注入与游戏内部接口 Hook，提供**实时小地图、雷达探测、洞穴地图、生物群系悬停识别、大地图、路径点管理**等 HUD 叠加层。

---

## 目录

- [项目简介](#项目简介)
- [功能特性](#功能特性)
- [运行环境](#运行环境)
- [构建与安装](#构建与安装)
- [使用说明](#使用说明)
  - [快捷键](#快捷键)
  - [小地图](#小地图)
  - [雷达 / 洞穴地图](#雷达--洞穴地图)
  - [生物群系悬停识别](#生物群系悬停识别)
  - [大地图](#大地图)
  - [路径点（Waypoint）](#路径点waypoint)
  - [设置面板](#设置面板)
- [配置说明](#配置说明)
- [架构与源码结构](#架构与源码结构)
- [渲染与 Hook 机制](#渲染与-hook-机制)
- [已知限制与容错](#已知限制与容错)
- [开发约定](#开发约定)
- [许可证](#许可证)

---

## 项目简介

ChiyanMap 是一个以「**让地图信息一目了然**」为目标的客户端模组。它不依赖服务器插件，而是在本地客户端内部：

- 利用 **DirectX 11 Present / WndProc Hook** 在安全时机把 ImGui 叠加层绘制到游戏画面之上；
- 利用 **Player / 区块 / 维度接口 Hook** 读取玩家坐标、朝向、所在区块、生物群系与高度图；
- 通过缓存（MapCacheManager）与节流（Radar 扫描帧间隔、悬停查询分帧）降低性能开销。

模组的运行完全处于游戏渲染线程，因此所有读图操作都不会阻塞逻辑线程或影响服务端。

---

## 功能特性

| 模块 | 说明 |
| --- | --- |
| **小地图（MiniMap）** | 圆形 / 方形可切换，支持按玩家朝向旋转；可拖拽位置、缩放、调整显示半径 |
| **雷达（Radar）** | 以玩家为中心按距离分层探测实体，支持上界（半径 32）与下界（半径 24）不同扫描半径 |
| **洞穴地图（CaveMap）** | 在玩家当前 Y 层上下各搜索若干方块，标出空气层与地板，呈现地下结构 |
| **生物群系悬停识别（HoverBiome）** | 鼠标悬停地面时，按分帧节流查询并翻译当前生物群系名称，HUD 淡入显示 |
| **大地图（BigMap）** | 全屏缩放式地图浏览，支持以玩家为中心切换「居中显示」 |
| **路径点（Waypoint）** | 增删改查路径点，可设置颜色；小地图 / 大地图上以三角标记显示 |
| **设置面板** | 快捷键重绑定、单行 / 全部重置、Ctrl+Z 撤销；各项显示开关与参数调节 |

主要设计亮点：

- 基于 **ImGui** 的叠加渲染，UI 与游戏画面同一坐标空间，鼠标交互自然；
- 所有 `WM_KEYDOWN` 快捷键在 `WndProc` 中处理，避免与游戏输入冲突；
- 生物群系名称映射覆盖主世界、下界、末地、洞穴及几乎所有现代群系（含 `pale_garden`、`cherry_grove` 等）；
- 对「部分加载区块」「NULL subchunk 指针」「复杂玩家状态」等边界情形做了大量容错，最大限度避免 `0xC0000005` 崩溃。

---

## 运行环境

| 项目 | 要求 |
| --- | --- |
| 游戏版本 | Minecraft Bedrock Edition（基岩版），与所链接的 LeviLamina / BDS 版本匹配 |
| 加载器 | [LeviLamina](https://github.com/LiteLDev/LeviLamina) |
| 操作系统 | Windows（依赖 Win32 `d3d11` / `dwmapi` / `winuser`、DirectX 11 运行时） |
| 渲染后端 | DirectX 11（支持 DXGI 1.2+ / D3D_FEATURE_LEVEL_11_0） |
| 编译工具链 | XMake 3.0.0+、MSVC 2022+、C++20 |

> 本模组为**客户端渲染向** Mod，仅在带图形界面的 Windows 客户端进程内生效。

---

## 构建与安装

### 1. 环境准备

- 安装 [Visual Studio 2022](https://visualstudio.microsoft.com/)（勾选「使用 C++ 的桌面开发」）。
- 安装 [XMake](https://xmake.io/)（`3.0.0` 及以上）。
- 准备与游戏版本对应的 LeviLamina 开发包（`xmake.lua` 通过 `add_requires("levilamina")` 引入）。

### 2. 克隆与构建

```powershell
git clone <本仓库地址> ChiyanMap
cd ChiyanMap

# 配置并构建（Release）
xmake f -m release
xmake

# 如需调试符号
xmake f -m debug
xmake
```

构建产物为 `.dll`（及配套 `manifest.json` / `tooth.json`）。

### 3. 安装到 LeviLamina

将构建产物按 LeviLamina 的 Mod 目录规范放置（通常位于 LeviLamina 的 `mods/<modName>/` 下，包含 `manifest.json`、`ChiyanMap.dll` 以及 `lang/` 资源目录）。具体目录约定以你所使用的 LeviLamina 版本为准。

启动游戏后，模组会在 `onLoad` 阶段加载语言包与配置，`onEnable` 阶段注册所有 Hook。

---

## 使用说明

### 快捷键

| 按键 | 功能 |
| --- | --- |
| `M` | 切换**大地图**显示 |
| `U` | 切换**路径点管理器** |
| `N` | 切换**小地图**显示开关 |
| `Y` | 切换小地图形状（圆形 / 方形） |
| `J` | 切换小地图旋转（随玩家朝向 / 固定北） |
| `Ctrl+Z` | 撤销上一次快捷键 / 设置修改 |

> 所有快捷键均为 Win32 虚拟键码，可在设置面板中**重绑定**。将某键重绑定为 `0`（禁用）即清除该快捷键。重绑过程中有「单击监听」机制，按下目标键即完成绑定。

### 小地图

- 默认显示在屏幕右上角附近，可拖拽移动、缩放（`0.2` ~ `2.5`）、调节显示半径（`10` ~ `200` 格）。
- 支持**圆形 / 方形**两种形状，以及**随玩家旋转 / 锁定北**两种朝向模式。
- 可叠加显示：实体雷达、洞穴层、路径点、生物群系悬停信息。

### 雷达 / 洞穴地图

- **雷达**以玩家为中心分层探测，颜色按距离由近及远变化；上界扫描半径 `32`，下界扫描半径 `24`（见 `kRadarRangeMax` / `kRadarRangeMin` / `kRadarInterval`）。
- **洞穴地图**在玩家当前层向上 `3` 格起、上下各搜索 `64` 格（`kCaveLayerTopOffset` / `kCaveLayerAirSearchDepth` / `kCaveLayerFloorSearchDepth`），标出空气层与地板。

### 生物群系悬停识别

- 鼠标悬停地面时，模块以**分帧节流**（约每 6 帧、或移动足够距离时每 2 帧）查询该坐标生物群系，并翻译为本地化名称。
- 结果以 HUD 淡入显示（`hoverBiomeAlpha` 目标 `1.0`）；查询失败或离开地面则淡出。
- 内置 `kBiomeNameToId` 映射覆盖主世界、下界、末地、洞穴等绝大多数群系，未知群系回退为 `BIOME_UNKNOWN`。

### 大地图

- 全屏可缩放地图，可切换「以玩家为中心」居中模式。
- 支持在小地图 / 大地图间共享缓存，降低重复扫描开销。

### 路径点（Waypoint）

- 通过 `U` 打开管理器，支持新增 / 编辑 / 删除 / 颜色设置。
- 路径点数据由 `WaypointManager` 持久化（跟随模组配置目录），并在小地图、大地图上以三角标记呈现（方向随玩家视角旋转）。

### 设置面板

- 快捷键重绑定（单行重置 / 全部重置）；
- 小地图位置、缩放、半径、形状、旋转等参数实时调节；
- 各叠加层（雷达、洞穴、路径点、生物群系悬停）显示开关；
- 配置通过 `LanguageManager::SaveConfig()` / `loadConfig` 持久化到 JSON（聚合结构体 + 反射自动序列化）。

---

## 配置说明

模组配置与语言文件由 LeviLamina 提供的资源目录接口管理：

- `getConfigDir()`：配置目录（持久化快捷键、显示参数、路径点等）。
- `getLangDir()`：语言包目录，由 `LanguageManager` 在 `onLoad` 阶段 `load()`。
- 配置结构体为**聚合类型**，包含 `int version` 字段；首次加载或版本不符时 `loadConfig` 返回 `false`，此时需 `saveConfig` 写入默认值（见 `ChiyanMap::onLoad` 中的版本处理：`LOG_VERSION_MISMATCH / STRICT / ABORT / PASS / UNKNOWN`）。

语言文本通过 `LanguageManager::GetText("KEY")` 获取，`KEY` 形如 `MINIMAP_POS_SETTINGS`、`MINIMAP_SCALE`、`X_OFFSET` 等。

---

## 架构与源码结构

```
ChiyanMap/
├── manifest.json              # 模组清单（入口、版本、依赖）
├── tooth.json                 # LeviLamina / Tooth 打包元数据
├── xmake.lua                  # 构建脚本（XMake 3.0+，C++20，依赖 levilamina）
├── src/
│   ├── mod/
│   │   ├── ChiyanMap.h/.cpp    # 模组入口：NativeMod 生命周期（onLoad/onEnable/onDisable/onUnload）
│   ├── hooks/
│   │   ├── HookRegistry.h/.cpp # Hook 注册中心（统一注册 / 反注册）
│   │   ├── DX11Hook.h          # D3D11 Present + WndProc Hook，ImGui 叠加渲染与输入
│   │   ├── UIRenderHook.h      # UI 渲染编排（小地图 / 雷达 / 洞穴 / 悬停 / 大地图 / 路径点）
│   │   └── PlayerHook.h        # 玩家 / 区块 / 维度接口 Hook，生物群系与高度图读取
│   └── state/
│       ├── MapRenderState.h    # 全局渲染状态 + 快捷键绑定（HotkeyBindings）
│       ├── MapCacheManager.h   # 区块 / 地图数据缓存管理
│       ├── WaypointManager.h   # 路径点增删改查与持久化
│       └── LanguageManager.h   # 多语言加载、配置读写、文本查询
└── LICENSE
```

### 职责划分

- **ChiyanMap**：模组生命周期管理，加载语言与配置，在 `onEnable` 注册全部 Hook，在 `onDisable` 反注册。
- **HookRegistry**：集中管理所有 Hook 的注册与卸载，保证生命周期一致。
- **DX11Hook**：接管 D3D11 交换链 `Present` 与窗口过程 `WndProc`，在安全的渲染时机初始化 ImGui、处理键盘快捷、驱动每帧叠加渲染。
- **UIRenderHook**：编排各个叠加层（小地图、雷达、洞穴、悬停识别、大地图、路径点、设置面板）的绘制顺序与显隐。
- **PlayerHook**：通过游戏内部接口读取玩家坐标 / 朝向、区块高度图、生物群系 ID，并维护雷达扫描与洞穴层探测逻辑。
- **state/**：所有可变状态与持久化数据（渲染参数、缓存、路径点、语言）。

---

## 渲染与 Hook 机制

1. **D3D11 注入**
   - Hook `IDXGISwapChain::Present` 与窗口 `WndProc`，在每帧渲染后、安全的 D3D 上下文内调用 ImGui 绘制。
   - 使用 `d3d11` / `dxgi` / `dwmapi` 接口，兼容 DXGI 1.2+。

2. **输入处理**
   - 所有快捷键在 `WndProc` 的 `WM_KEYDOWN` 分支处理，写入 `MapRenderState::g_hotkeys` 与各显示开关；
   - `Ctrl+Z` 撤销请求以 `std::atomic<bool>` 跨线程传递，由渲染线程消费。

3. **数据读取（PlayerHook）**
   - 通过游戏内部接口（Fake Header 中声明的符号）获取玩家位置、区块、维度与生物群系；
   - 雷达扫描按帧间隔（`kRadarInterval`）节流，悬停查询分帧（`hoverBiomeFrameCounter`）节流，避免高频读图。

4. **缓存**
   - `MapCacheManager` 缓存已扫描区块数据，小地图与大地图共享，减少重复计算。

---

## 已知限制与容错

- 模组**仅在 Windows + DirectX 11 客户端**生效，不支持其他图形后端或平台。
- 读取未完全加载的区块 / 空 subchunk 指针时，模块做了容错（避免 `0xC0000005` 与 `std::terminate` → `0xC0000409 FAST_FAIL`），但极端边界下仍建议通过缓存与节流规避。
- 复杂玩家状态（如切换维度、死亡重生瞬间）下会跳过区块 / 缓存逻辑，待状态稳定后恢复。
- 雷达与洞穴探测范围受游戏接口可达性限制，过远区域可能无法实时刷新。

---

## 开发约定

本项目遵循 LeviLamina 原生 Mod 开发规范（摘要）：

- **构建**：XMake 3.0.0+ 与 MSVC 2022+（C++20）；入口在 `xmake.lua`，通过 `add_requires("levilamina")` 引入依赖。
- **生命周期**：继承 `ll::mod::NativeMod`，实现 `onLoad / onEnable / onDisable / onUnload`，返回 `bool`（`false` 中止）；日志用 `mod.getLogger()`。
- **Hook**：使用 `ll/api/memory/Hook.h`（`LL_AUTO_INSTANCE_HOOK` / `LL_TYPE_INSTANCE_HOOK`），优先 `Normal` 优先级；查函数签名使用 LeviLamina Fake Header（`src/mc/...`），不猜测偏移。
- **配置**：`ll/api/Config.h` 的 `saveConfig/loadConfig`，配置结构体须为聚合类型并含 `int version`。
- **国际化**：`ll::i18n` 加载 `getLangDir()`，文本用 `"key"_tr()`。
- **渲染**：所有游戏状态读取发生在渲染线程 / 服务器 tick 内，避免跨线程访问内部对象。

更多细节参见 LeviLamina 开发者文档（`docs/main/contents/developer_guides/` 与 `docs/main/contents/api_reference/`）。

---

## 许可证

本项目基于 [`LICENSE`](./LICENSE) 中的条款发布。
