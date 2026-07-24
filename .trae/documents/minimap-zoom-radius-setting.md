# 小地图比例尺（视野范围）设置实现计划

## Context（背景）

当前小地图的视野范围（显示玩家周围多少方块）是硬编码的 `ZOOM_RADIUS = 50.0f`（[DX11Hook.h:822](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L822)），用户无法调整。用户希望添加一个"比例尺"设置来控制小地图覆盖的方块范围——值越小越放大（看更少方块、更详细），值越大越缩小（看更多方块）。

**注意区分**：已存在 `miniMapScale`（[MapRenderState.h:116](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h#L116)，i18n="缩放比例"）控制小地图的**视觉像素大小**，并非本次要添加的"视野范围"。两者相互独立：视觉大小决定圆圈多大，视野范围决定圈里显示多少方块。

## 命名约定

| 概念 | 名称 | 说明 |
|---|---|---|
| 状态变量 | `miniMapZoomRadius` | 遵循 `miniMapScale`/`miniMapOffsetX` 驼峰命名 |
| 配置 JSON 键 | `"miniMapZoomRadius"` | 遵循 `j.value("miniMapScale", ...)` 模式 |
| i18n 键 | `"MINIMAP_ZOOM_RADIUS"` | 遵循 `MINIMAP_SCALE`/`X_OFFSET` 大写下划线 |
| 默认值 | `50.0f` | 保持当前行为不变 |
| 范围 | `[10.0f, 200.0f]` | 上限远低于 `MAP_DATA_RADIUS=256`，防止 UV 越界 |
| UI 步长/格式 | `5.0f` / `"%.0f"` | 与 X/Y 偏移行一致（方块数为整数） |
| i18n 标签 | zh_CN="视野范围" / en="View Range" | 避免与已有"缩放比例"混淆 |

## 实现步骤

### 步骤 1：添加状态变量

**文件**：[D:\Project\ChiyanMap\src\state\MapRenderState.h](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h)

在 [第 116 行](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h#L116) `miniMapScale` 之后插入：

```cpp
inline float miniMapZoomRadius = 50.0f; // 小地图可视范围（玩家周围方块半径，10-200）
```

### 步骤 2：渲染时消费状态变量

**文件**：[D:\Project\ChiyanMap\src\hooks\DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h)

[第 822 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L822) 替换硬编码常量：

```cpp
// 之前: float ZOOM_RADIUS = 50.0f;
// 运行时夹取为安全区间 [10, 200]，防御配置文件被外部工具篡改为越界值
float ZOOM_RADIUS = std::clamp(MapRenderState::miniMapZoomRadius, 10.0f, 200.0f);
```

**无需其他渲染改动**（已验证）：
- [第 825 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L825) `uvR = ZOOM_RADIUS / MAP_DATA_SIZE` → 自动适应（缩小时采样更大纹理区域）
- [第 928 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L928) `scale = IM_MAP_R / ZOOM_RADIUS` → 实体/路径点位置自动缩放
- [第 987 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L987) 路径点"屏内/边缘"阈值 `physicalDist <= ZOOM_RADIUS` → 自动跟踪

`<algorithm>` 已在 [DX11Hook.h:31](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L31) 包含，`std::clamp` 可直接使用。

### 步骤 3：扩展设置面板 UI

**文件**：[D:\Project\ChiyanMap\src\hooks\DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h)，函数 `RenderMiniMapPosSettings()`（[第 1086-1199 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1086-L1199)）

#### 3a. 添加 `origZoomRadius` 静态变量（回滚支持）
[第 1087-1090 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1087-L1090) 在 `origScale` 后添加：
```cpp
static float origZoomRadius = 50.0f;
```

#### 3b. 异常关闭时回滚
[第 1095-1097 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1095-L1097) 在 `miniMapScale = origScale;` 后添加：
```cpp
MapRenderState::miniMapZoomRadius = origZoomRadius;
```

#### 3c. 初始化时捕获原值
[第 1104-1106 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1104-L1106) 在 `origScale = ...` 后添加：
```cpp
origZoomRadius = MapRenderState::miniMapZoomRadius;
```

#### 3d. 扩展 `labelWidth` 计算
[第 1123 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1123) 替换为：
```cpp
float labelWidth = std::max({
    ImGui::CalcTextSize(LanguageManager::GetText("X_OFFSET")).x,
    ImGui::CalcTextSize(LanguageManager::GetText("MINIMAP_SCALE")).x,
    ImGui::CalcTextSize(LanguageManager::GetText("MINIMAP_ZOOM_RADIUS")).x
});
```

#### 3e. 添加第 4 行 drawRow
[第 1159 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1159)（MINIMAP_SCALE 行）之后插入：
```cpp
drawRow(LanguageManager::GetText("MINIMAP_ZOOM_RADIUS"), "##ZoomSlider", "##ZoomSub", "##ZoomInput", "##ZoomAdd", MapRenderState::miniMapZoomRadius, 10.0f, 200.0f, 5.0f, "%.0f");
```

#### 3f. 防御性夹取（镜像 [第 1161 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1161) miniMapScale 模式）
在新 drawRow 之后添加：
```cpp
if (MapRenderState::miniMapZoomRadius < 10.0f) MapRenderState::miniMapZoomRadius = 10.0f;
if (MapRenderState::miniMapZoomRadius > 200.0f) MapRenderState::miniMapZoomRadius = 200.0f;
```

#### 3g. DEFAULT_POS 按钮重置
[第 1166-1170 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1166-L1170) 在 `miniMapScale = 1.0f;` 后添加：
```cpp
MapRenderState::miniMapZoomRadius = 50.0f;
```

#### 3h. DONT_SAVE 按钮回滚
[第 1189-1196 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1189-L1196) 在 `miniMapScale = origScale;` 后添加：
```cpp
MapRenderState::miniMapZoomRadius = origZoomRadius;
```

SAVE_AND_EXIT（[第 1182-1187 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L1182-L1187)）无需改动——它调用 `LanguageManager::SaveConfig()` 会自动保存新状态。

### 步骤 4：添加 i18n 键（16 语言）

**文件**：[D:\Project\ChiyanMap\src\state\LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp)

**⚠️ BOM 保护**：此文件以 UTF-8 BOM (`EF BB BF`) 开头。**必须用 Python 脚本**（`rb` 模式读取→剥离 BOM→UTF-8 解码→替换→UTF-8 编码→前置单个 BOM→`wb` 写回）进行编辑，避免 Edit 工具累积 BOM 导致 MSVC C2014 错误。

**位置**：在 `MINIMAP_SCALE` 块（[第 1448-1452 行](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp#L1448-L1452)）之后、`MINIMAP_POS_SETTINGS` 块（[第 1453 行](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp#L1453)）之前插入：

```cpp
        if (key == "MINIMAP_ZOOM_RADIUS") {
            if (g_currentLanguage == "zh_CN") return "视野范围";
            if (g_currentLanguage == "zh_TW") return "視野範圍";
            if (g_currentLanguage == "ja") return "表示範囲";
            if (g_currentLanguage == "ko") return "표시 범위";
            if (g_currentLanguage == "ru") return "Радиус обзора";
            if (g_currentLanguage == "fr") return "Plage de vue";
            if (g_currentLanguage == "de") return "Sichtweite";
            if (g_currentLanguage == "pt_BR") return "Alcance de Visão";
            if (g_currentLanguage == "it") return "Raggio di visuale";
            if (g_currentLanguage == "es") return "Alcance de visión";
            if (g_currentLanguage == "id") return "Jangkauan Pandang";
            if (g_currentLanguage == "th") return "รัศมีมองเห็น";
            if (g_currentLanguage == "tr") return "Görüş Menzili";
            if (g_currentLanguage == "uk") return "Радіус огляду";
            if (g_currentLanguage == "vi") return "Phạm vi hiển thị";
            return "View Range";
        }
```

**翻译用词说明**：选用"视野范围/View Range"而非"比例尺"，避免与已有"缩放比例（Scale）"行混淆——两行都叫"比例"会让用户困惑。

### 步骤 5：配置持久化

**文件**：[D:\Project\ChiyanMap\src\state\LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp)（用同一 Python 脚本一并修改）

#### 5a. LoadConfig（[第 1364 行](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp#L1364) 之后）
```cpp
MapRenderState::miniMapScale = j.value("miniMapScale", 1.0f);
// 小地图可视范围（玩家周围方块半径），夹取到 [10, 200] 防御外部篡改
MapRenderState::miniMapZoomRadius = std::clamp(j.value("miniMapZoomRadius", 50.0f), 10.0f, 200.0f);
```

#### 5b. SaveConfig（[第 1392 行](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp#L1392) 之后）
```cpp
j["miniMapScale"] = MapRenderState::miniMapScale;
j["miniMapZoomRadius"] = MapRenderState::miniMapZoomRadius;
```

**注意**：若 `std::clamp` 在 LanguageManager.cpp 中因缺少 `<algorithm>` 头文件编译失败，需在文件头部 include 区添加 `#include <algorithm>`。验证方式：编译时若报 `std::clamp 未定义` 则补加。

## 边界情况与风险

1. **BOM 累积**（LanguageManager.cpp）— 用 Python 脚本编辑规避（步骤 4 已说明）
2. **方形+旋转+最大缩小的 UV 越界** — `ZOOM_RADIUS=200` + `isSquareMap && rotateMiniMap` 时 `uvDrawR ≈ 0.552 > 0.5`，可能在方形旋转地图边角出现采样瑕疵。这是极端组合（最大缩小+方形+旋转），PointSamplerCallback 会钳制寻址。若用户报告瑕疵，可将上限从 200 降至 150（`uvDrawR ≈ 0.414`，安全）
3. **旧配置文件兼容** — 旧 `config.json` 无 `miniMapZoomRadius` 键，`j.value(..., 50.0f)` 返回默认值 50.0f，保持原硬编码行为，无需迁移代码
4. **InputFloat 手动输入越界** — 用户可在输入框输入 `9999` 或 `1`，步骤 3f 的防御性夹取会在下一帧 snap 回 [10, 200]
5. **路径点边缘渲染** — [第 987 行](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h#L987) `physicalDist <= ZOOM_RADIUS` 决定路径点按实际位置绘制还是钉在边缘；ZOOM_RADIUS 增大时更多路径点按实际位置显示（符合预期）

## 验证步骤

### 1. 编译
```powershell
C:\Users\chiyan\scoop\shims\xmake.exe -r
```
预期：零警告零错误，生成 `D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll`

### 2. BOM 校验（编译前必做）
```powershell
powershell -NoProfile -Command "$b = [System.IO.File]::ReadAllBytes('D:\Project\ChiyanMap\src\state\LanguageManager.cpp'); ($b[0]).ToString('X2') + ' ' + ($b[1]).ToString('X2') + ' ' + ($b[2]).ToString('X2')"
```
预期输出：`EF BB BF`（恰好一个 BOM）

### 3. 部署
```powershell
[System.IO.File]::Copy("D:\Project\ChiyanMap\bin\ChiyanMap\ChiyanMap.dll", "D:\我的世界\基岩版游戏\levilauncher.exe\versions\1.26.20.04\mods\ChiyanMap\ChiyanMap.dll", $true)
```

### 4. 游戏内测试
进入世界 → 按 M 打开大地图 → 点击"调整小地图布局"打开设置面板：

| 测试项 | 操作 | 预期 |
|---|---|---|
| 默认值 | 打开面板 | 第 4 行显示"视野范围"，滑块在 50，输入框"50" |
| 滑块拖右 | 拖到最右 | 值=200，小地图显示更广区域 |
| 滑块拖左 | 拖到最左 | 值=10，小地图放大显示更少方块 |
| 箭头按钮 | 点右箭头 | 值+5 |
| 手动输入 | 输入 150 | 值应用，小地图更新 |
| 越界输入 | 输入 9999 | 下一帧 snap 回 200 |
| 恢复默认 | 点"恢复默认" | 4 行全部重置（X=0,Y=0,缩放=1.0,视野=50） |
| 不保存 | 修改后点"不保存" | 4 行全部回滚到打开前值 |
| 保存并退出 | 修改后点"保存并退出" | 面板关闭，config.json 更新 |
| 持久化 | 关游戏重开 | 视野范围值与保存时一致 |
| 篡改配置 | 手动改 config.json 为 9999 | 加载时夹取到 200 |
| 实体缩放 | 放路径点 80 格外，视野=50→边缘；视野=100→圈内 | 确认 `scale = IM_MAP_R / ZOOM_RADIUS` 自动适应 |

### 5. 配置文件校验
保存后检查 `mods/ChiyanMap/config/config.json`，应包含：
```json
{
    "miniMapScale": 1.0,
    "miniMapZoomRadius": 50.0,
    ...
}
```

## 关键文件清单

- [D:\Project\ChiyanMap\src\state\MapRenderState.h](file:///D:/Project/ChiyanMap/src/state/MapRenderState.h) — 添加 `miniMapZoomRadius` 状态变量（第 116 行后）
- [D:\Project\ChiyanMap\src\hooks\DX11Hook.h](file:///D:/Project/ChiyanMap/src/hooks/DX11Hook.h) — 第 822 行消费状态；扩展 `RenderMiniMapPosSettings()`（第 1086-1199 行）添加第 4 行、回滚、DEFAULT_POS、DONT_SAVE
- [D:\Project\ChiyanMap\src\state\LanguageManager.cpp](file:///D:/Project/ChiyanMap/src/state/LanguageManager.cpp) — 添加 `MINIMAP_ZOOM_RADIUS` i18n 块（第 1452 行后）、LoadConfig 行（第 1364 行后）、SaveConfig 行（第 1392 行后）。**必须用 Python 脚本编辑保护 BOM**
