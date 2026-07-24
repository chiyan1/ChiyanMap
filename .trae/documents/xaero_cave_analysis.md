# Xaero's Minimap 洞穴地图截图分析报告

分析目录: `D:\赤焰\桌面\临时截图\1`
截图文件数: 5

---

## 一、概览汇总

| 文件名 | 尺寸 (WxH) | 推断类型 | 暗区占比 |
| --- | --- | --- | --- |
| PixPin_2026-07-19_18-56-50.png | 1920x1080 | corner-minimap (0.706) | 0.188 |
| PixPin_2026-07-19_18-57-08.png | 1920x1080 | corner-minimap (0.886) | 0.111 |
| PixPin_2026-07-19_18-57-26.png | 1920x1080 | corner-minimap (0.946) | 0.482 |
| PixPin_2026-07-19_18-57-38.png | 1920x1080 | full-screen-cave-map (0.929) | 0.929 |
| PixPin_2026-07-19_18-58-14.png | 1175x804 | full-screen-cave-map (0.836) | 0.836 |

---

## 二、逐文件详细分析

### 2.1 `PixPin_2026-07-19_18-56-50.png`

- **尺寸**: 1920 x 1080 像素
- **图像模式**: RGBA
- **推断类型**: corner-minimap (置信度/分数: 0.7056)
- **暗区(背景)占比**: 0.1882 (18.8%)

#### 整图 Top 15 高频颜色

| 排名 | RGB | Hex | 类别 | 占比 |
| --- | --- | --- | --- | --- |
| 1 | (89, 92, 99) | #595C63 | dark-gray | 0.54% |
| 2 | (0, 0, 0) | #000000 | near-black | 0.52% |
| 3 | (72, 63, 49) | #483F31 | reddish | 0.44% |
| 4 | (29, 26, 22) | #1D1A16 | very-dark | 0.43% |
| 5 | (90, 82, 67) | #5A5243 | reddish | 0.39% |
| 6 | (91, 94, 102) | #5B5E66 | dark-gray | 0.37% |
| 7 | (97, 85, 63) | #61553F | reddish | 0.34% |
| 8 | (90, 93, 101) | #5A5D65 | dark-gray | 0.34% |
| 9 | (75, 69, 60) | #4B453C | reddish | 0.32% |
| 10 | (40, 36, 28) | #28241C | very-dark | 0.30% |
| 11 | (61, 56, 51) | #3D3833 | dark-gray | 0.29% |
| 12 | (41, 37, 29) | #29251D | very-dark | 0.29% |
| 13 | (30, 27, 23) | #1E1B17 | very-dark | 0.29% |
| 14 | (71, 62, 48) | #473E30 | reddish | 0.28% |
| 15 | (61, 56, 50) | #3D3832 | dark-gray | 0.28% |

#### 关键像素采样

| 位置 | 坐标 (x,y) | 颜色 |
| --- | --- | --- |
| center | (960,540) | RGB(56, 52, 44) #38342C [very-dark] |
| top-left-corner | (0,0) | RGB(21, 20, 16) #151410 [very-dark] |
| top-right-corner | (1919,0) | RGB(14, 16, 18) #0E1012 [near-black] |
| bottom-left-corner | (0,1079) | RGB(38, 34, 26) #26221A [very-dark] |
| bottom-right-corner | (1919,1079) | RGB(90, 81, 64) #5A5140 [reddish] |
| top-mid | (960,0) | RGB(50, 49, 46) #32312E [very-dark] |
| bottom-mid | (960,1079) | RGB(0, 0, 0) #000000 [near-black] |
| left-mid | (0,540) | RGB(107, 93, 66) #6B5D42 [reddish] |
| right-mid | (1919,540) | RGB(49, 43, 33) #312B21 [very-dark] |
| q1-center | (480,270) | RGB(30, 26, 22) #1E1A16 [very-dark] |
| q2-center | (1440,270) | RGB(33, 29, 25) #211D19 [very-dark] |
| q3-center | (480,810) | RGB(36, 33, 26) #24211A [very-dark] |
| q4-center | (1440,810) | RGB(75, 69, 59) #4B453B [reddish] |

#### 象限颜色分布 (各象限 Top 3)

- **top-left**:
  - (57, 57, 55) #393937 [very-dark] - count=1195
  - (224, 224, 224) #E0E0E0 [light-gray] - count=1116
  - (20, 19, 15) #14130F [very-dark] - count=961
- **top-right**:
  - (89, 92, 99) #595C63 [dark-gray] - count=2833
  - (91, 94, 102) #5B5E66 [dark-gray] - count=1941
  - (90, 93, 101) #5A5D65 [dark-gray] - count=1782
- **bottom-left**:
  - (29, 26, 22) #1D1A16 [very-dark] - count=1852
  - (40, 36, 28) #28241C [very-dark] - count=1381
  - (41, 37, 29) #29251D [very-dark] - count=1343
- **bottom-right**:
  - (90, 82, 67) #5A5243 [reddish] - count=2012
  - (75, 69, 60) #4B453C [reddish] - count=1675
  - (61, 56, 51) #3D3833 [dark-gray] - count=1500
- **center**:
  - (33, 30, 26) #211E1A [very-dark] - count=552
  - (28, 25, 22) #1C1916 [very-dark] - count=552
  - (39, 35, 28) #27231C [very-dark] - count=520

---

### 2.2 `PixPin_2026-07-19_18-57-08.png`

- **尺寸**: 1920 x 1080 像素
- **图像模式**: RGBA
- **推断类型**: corner-minimap (置信度/分数: 0.8858)
- **暗区(背景)占比**: 0.1106 (11.1%)

#### 整图 Top 15 高频颜色

| 排名 | RGB | Hex | 类别 | 占比 |
| --- | --- | --- | --- | --- |
| 1 | (0, 0, 0) | #000000 | near-black | 5.44% |
| 2 | (45, 46, 49) | #2D2E31 | very-dark | 3.48% |
| 3 | (25, 26, 29) | #191A1D | very-dark | 2.50% |
| 4 | (21, 31, 52) | #151F34 | very-dark | 2.47% |
| 5 | (24, 34, 55) | #182237 | very-dark | 2.33% |
| 6 | (23, 33, 54) | #172136 | very-dark | 2.09% |
| 7 | (18, 28, 49) | #121C31 | very-dark | 2.00% |
| 8 | (68, 65, 40) | #444128 | reddish | 1.73% |
| 9 | (20, 30, 51) | #141E33 | very-dark | 1.61% |
| 10 | (17, 27, 48) | #111B30 | very-dark | 1.54% |
| 11 | (22, 32, 53) | #162035 | very-dark | 1.52% |
| 12 | (22, 33, 54) | #162136 | very-dark | 1.37% |
| 13 | (16, 26, 47) | #101A2F | very-dark | 1.36% |
| 14 | (32, 33, 35) | #202123 | very-dark | 1.36% |
| 15 | (38, 37, 24) | #262518 | very-dark | 1.24% |

#### 关键像素采样

| 位置 | 坐标 (x,y) | 颜色 |
| --- | --- | --- |
| center | (960,540) | RGB(240, 25, 25) #F01919 [reddish] |
| top-left-corner | (0,0) | RGB(45, 46, 49) #2D2E31 [very-dark] |
| top-right-corner | (1919,0) | RGB(39, 40, 43) #27282B [very-dark] |
| bottom-left-corner | (0,1079) | RGB(16, 26, 47) #101A2F [very-dark] |
| bottom-right-corner | (1919,1079) | RGB(31, 17, 18) #1F1112 [very-dark] |
| top-mid | (960,0) | RGB(18, 19, 21) #121315 [very-dark] |
| bottom-mid | (960,1079) | RGB(70, 72, 76) #46484C [dark-gray] |
| left-mid | (0,540) | RGB(16, 26, 47) #101A2F [very-dark] |
| right-mid | (1919,540) | RGB(27, 33, 52) #1B2134 [very-dark] |
| q1-center | (480,270) | RGB(18, 28, 49) #121C31 [very-dark] |
| q2-center | (1440,270) | RGB(22, 47, 39) #162F27 [very-dark] |
| q3-center | (480,810) | RGB(19, 29, 51) #131D33 [very-dark] |
| q4-center | (1440,810) | RGB(60, 58, 36) #3C3A24 [reddish] |

#### 象限颜色分布 (各象限 Top 3)

- **top-left**:
  - (25, 26, 29) #191A1D [very-dark] - count=5265
  - (0, 0, 0) #000000 [near-black] - count=5147
  - (18, 28, 49) #121C31 [very-dark] - count=5011
- **top-right**:
  - (0, 0, 0) #000000 [near-black] - count=7340
  - (68, 65, 40) #444128 [reddish] - count=4896
  - (38, 37, 24) #262518 [very-dark] - count=3232
- **bottom-left**:
  - (21, 31, 52) #151F34 [very-dark] - count=8299
  - (24, 34, 55) #182237 [very-dark] - count=8283
  - (23, 33, 54) #172136 [very-dark] - count=6513
- **bottom-right**:
  - (0, 0, 0) #000000 [near-black] - count=11590
  - (45, 46, 49) #2D2E31 [very-dark] - count=9825
  - (68, 65, 40) #444128 [reddish] - count=3807
- **center**:
  - (45, 46, 49) #2D2E31 [very-dark] - count=4250
  - (21, 31, 52) #151F34 [very-dark] - count=3790
  - (18, 28, 49) #121C31 [very-dark] - count=3346

---

### 2.3 `PixPin_2026-07-19_18-57-26.png`

- **尺寸**: 1920 x 1080 像素
- **图像模式**: RGBA
- **推断类型**: corner-minimap (置信度/分数: 0.9460)
- **暗区(背景)占比**: 0.4818 (48.2%)

#### 整图 Top 15 高频颜色

| 排名 | RGB | Hex | 类别 | 占比 |
| --- | --- | --- | --- | --- |
| 1 | (0, 0, 0) | #000000 | near-black | 36.52% |
| 2 | (32, 33, 35) | #202123 | very-dark | 1.50% |
| 3 | (45, 46, 49) | #2D2E31 | very-dark | 1.01% |
| 4 | (21, 31, 52) | #151F34 | very-dark | 0.82% |
| 5 | (23, 33, 54) | #172136 | very-dark | 0.74% |
| 6 | (22, 32, 53) | #162035 | very-dark | 0.73% |
| 7 | (20, 30, 51) | #141E33 | very-dark | 0.73% |
| 8 | (19, 29, 50) | #131D32 | very-dark | 0.68% |
| 9 | (18, 28, 49) | #121C31 | very-dark | 0.63% |
| 10 | (24, 34, 55) | #182237 | very-dark | 0.49% |
| 11 | (18, 19, 21) | #121315 | very-dark | 0.46% |
| 12 | (252, 252, 252) | #FCFCFC | near-white | 0.43% |
| 13 | (17, 27, 48) | #111B30 | very-dark | 0.43% |
| 14 | (25, 26, 29) | #191A1D | very-dark | 0.41% |
| 15 | (31, 32, 34) | #1F2022 | very-dark | 0.41% |

#### 关键像素采样

| 位置 | 坐标 (x,y) | 颜色 |
| --- | --- | --- |
| center | (960,540) | RGB(240, 25, 25) #F01919 [reddish] |
| top-left-corner | (0,0) | RGB(0, 0, 0) #000000 [near-black] |
| top-right-corner | (1919,0) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-left-corner | (0,1079) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-right-corner | (1919,1079) | RGB(0, 0, 0) #000000 [near-black] |
| top-mid | (960,0) | RGB(16, 19, 25) #101319 [very-dark] |
| bottom-mid | (960,1079) | RGB(0, 0, 0) #000000 [near-black] |
| left-mid | (0,540) | RGB(0, 0, 0) #000000 [near-black] |
| right-mid | (1919,540) | RGB(28, 23, 22) #1C1716 [very-dark] |
| q1-center | (480,270) | RGB(63, 62, 66) #3F3E42 [dark-gray] |
| q2-center | (1440,270) | RGB(23, 33, 54) #172136 [very-dark] |
| q3-center | (480,810) | RGB(0, 0, 0) #000000 [near-black] |
| q4-center | (1440,810) | RGB(41, 42, 45) #292A2D [very-dark] |

#### 象限颜色分布 (各象限 Top 3)

- **top-left**:
  - (0, 0, 0) #000000 [near-black] - count=50830
  - (32, 33, 35) #202123 [very-dark] - count=1451
  - (21, 31, 52) #151F34 [very-dark] - count=1338
- **top-right**:
  - (0, 0, 0) #000000 [near-black] - count=21921
  - (45, 46, 49) #2D2E31 [very-dark] - count=2527
  - (32, 33, 35) #202123 [very-dark] - count=1944
- **bottom-left**:
  - (0, 0, 0) #000000 [near-black] - count=75284
  - (32, 33, 35) #202123 [very-dark] - count=1497
  - (21, 31, 52) #151F34 [very-dark] - count=1307
- **bottom-right**:
  - (0, 0, 0) #000000 [near-black] - count=41357
  - (32, 33, 35) #202123 [very-dark] - count=3024
  - (45, 46, 49) #2D2E31 [very-dark] - count=1429
- **center**:
  - (0, 0, 0) #000000 [near-black] - count=24043
  - (32, 33, 35) #202123 [very-dark] - count=2922
  - (21, 31, 52) #151F34 [very-dark] - count=1799

---

### 2.4 `PixPin_2026-07-19_18-57-38.png`

- **尺寸**: 1920 x 1080 像素
- **图像模式**: RGBA
- **推断类型**: full-screen-cave-map (置信度/分数: 0.9286)
- **暗区(背景)占比**: 0.9286 (92.9%)

#### 整图 Top 15 高频颜色

| 排名 | RGB | Hex | 类别 | 占比 |
| --- | --- | --- | --- | --- |
| 1 | (0, 0, 0) | #000000 | near-black | 90.68% |
| 2 | (252, 252, 252) | #FCFCFC | near-white | 0.43% |
| 3 | (62, 62, 62) | #3E3E3E | dark-gray | 0.21% |
| 4 | (255, 255, 255) | #FFFFFF | near-white | 0.18% |
| 5 | (221, 221, 221) | #DDDDDD | light-gray | 0.18% |
| 6 | (63, 63, 63) | #3F3F3F | dark-gray | 0.16% |
| 7 | (55, 55, 55) | #373737 | very-dark | 0.13% |
| 8 | (1, 1, 1) | #010101 | near-black | 0.09% |
| 9 | (21, 31, 52) | #151F34 | very-dark | 0.05% |
| 10 | (2, 2, 2) | #020202 | near-black | 0.05% |
| 11 | (19, 29, 50) | #131D32 | very-dark | 0.05% |
| 12 | (29, 30, 32) | #1D1E20 | very-dark | 0.04% |
| 13 | (27, 28, 30) | #1B1C1E | very-dark | 0.04% |
| 14 | (32, 33, 35) | #202123 | very-dark | 0.04% |
| 15 | (20, 30, 51) | #141E33 | very-dark | 0.04% |

#### 关键像素采样

| 位置 | 坐标 (x,y) | 颜色 |
| --- | --- | --- |
| center | (960,540) | RGB(240, 25, 25) #F01919 [reddish] |
| top-left-corner | (0,0) | RGB(0, 0, 0) #000000 [near-black] |
| top-right-corner | (1919,0) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-left-corner | (0,1079) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-right-corner | (1919,1079) | RGB(0, 0, 0) #000000 [near-black] |
| top-mid | (960,0) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-mid | (960,1079) | RGB(0, 0, 0) #000000 [near-black] |
| left-mid | (0,540) | RGB(0, 0, 0) #000000 [near-black] |
| right-mid | (1919,540) | RGB(0, 0, 0) #000000 [near-black] |
| q1-center | (480,270) | RGB(0, 0, 0) #000000 [near-black] |
| q2-center | (1440,270) | RGB(0, 0, 0) #000000 [near-black] |
| q3-center | (480,810) | RGB(0, 0, 0) #000000 [near-black] |
| q4-center | (1440,810) | RGB(0, 0, 0) #000000 [near-black] |

#### 象限颜色分布 (各象限 Top 3)

- **top-left**:
  - (0, 0, 0) #000000 [near-black] - count=117415
  - (252, 252, 252) #FCFCFC [near-white] - count=560
  - (255, 255, 255) #FFFFFF [near-white] - count=175
- **top-right**:
  - (0, 0, 0) #000000 [near-black] - count=110598
  - (255, 255, 255) #FFFFFF [near-white] - count=660
  - (63, 63, 63) #3F3F3F [dark-gray] - count=582
- **bottom-left**:
  - (0, 0, 0) #000000 [near-black] - count=123163
  - (252, 252, 252) #FCFCFC [near-white] - count=612
  - (221, 221, 221) #DDDDDD [light-gray] - count=444
- **bottom-right**:
  - (0, 0, 0) #000000 [near-black] - count=119010
  - (252, 252, 252) #FCFCFC [near-white] - count=1080
  - (62, 62, 62) #3E3E3E [dark-gray] - count=684
- **center**:
  - (0, 0, 0) #000000 [near-black] - count=88089
  - (1, 1, 1) #010101 [near-black] - count=416
  - (2, 2, 2) #020202 [near-black] - count=241

---

### 2.5 `PixPin_2026-07-19_18-58-14.png`

- **尺寸**: 1175 x 804 像素
- **图像模式**: RGBA
- **推断类型**: full-screen-cave-map (置信度/分数: 0.8358)
- **暗区(背景)占比**: 0.8358 (83.6%)

#### 整图 Top 15 高频颜色

| 排名 | RGB | Hex | 类别 | 占比 |
| --- | --- | --- | --- | --- |
| 1 | (0, 0, 0) | #000000 | near-black | 70.80% |
| 2 | (255, 255, 255) | #FFFFFF | near-white | 3.33% |
| 3 | (63, 63, 63) | #3F3F3F | dark-gray | 2.96% |
| 4 | (44, 44, 44) | #2C2C2C | very-dark | 1.99% |
| 5 | (117, 117, 117) | #757575 | mid-gray | 1.43% |
| 6 | (42, 42, 42) | #2A2A2A | very-dark | 0.55% |
| 7 | (5, 7, 11) | #05070B | near-black | 0.55% |
| 8 | (7, 7, 8) | #070708 | near-black | 0.48% |
| 9 | (6, 6, 7) | #060607 | near-black | 0.47% |
| 10 | (4, 6, 11) | #04060B | near-black | 0.42% |
| 11 | (6, 6, 6) | #060606 | near-black | 0.41% |
| 12 | (45, 45, 45) | #2D2D2D | very-dark | 0.37% |
| 13 | (4, 6, 10) | #04060A | near-black | 0.36% |
| 14 | (115, 115, 115) | #737373 | mid-gray | 0.34% |
| 15 | (118, 118, 118) | #767676 | mid-gray | 0.34% |

#### 关键像素采样

| 位置 | 坐标 (x,y) | 颜色 |
| --- | --- | --- |
| center | (587,402) | RGB(0, 0, 0) #000000 [near-black] |
| top-left-corner | (0,0) | RGB(0, 0, 0) #000000 [near-black] |
| top-right-corner | (1174,0) | RGB(35, 35, 37) #232325 [very-dark] |
| bottom-left-corner | (0,803) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-right-corner | (1174,803) | RGB(0, 0, 0) #000000 [near-black] |
| top-mid | (587,0) | RGB(0, 0, 0) #000000 [near-black] |
| bottom-mid | (587,803) | RGB(0, 0, 0) #000000 [near-black] |
| left-mid | (0,402) | RGB(0, 0, 0) #000000 [near-black] |
| right-mid | (1174,402) | RGB(0, 0, 0) #000000 [near-black] |
| q1-center | (293,201) | RGB(0, 0, 0) #000000 [near-black] |
| q2-center | (881,201) | RGB(0, 0, 0) #000000 [near-black] |
| q3-center | (293,603) | RGB(117, 117, 117) #757575 [mid-gray] |
| q4-center | (881,603) | RGB(0, 0, 0) #000000 [near-black] |

#### 象限颜色分布 (各象限 Top 3)

- **top-left**:
  - (0, 0, 0) #000000 [near-black] - count=56691
  - (255, 255, 255) #FFFFFF [near-white] - count=1228
  - (63, 63, 63) #3F3F3F [dark-gray] - count=1043
- **top-right**:
  - (0, 0, 0) #000000 [near-black] - count=21589
  - (255, 255, 255) #FFFFFF [near-white] - count=3138
  - (63, 63, 63) #3F3F3F [dark-gray] - count=2704
- **bottom-left**:
  - (0, 0, 0) #000000 [near-black] - count=37561
  - (44, 44, 44) #2C2C2C [very-dark] - count=3859
  - (117, 117, 117) #757575 [mid-gray] - count=2847
- **bottom-right**:
  - (0, 0, 0) #000000 [near-black] - count=52071
  - (255, 255, 255) #FFFFFF [near-white] - count=1791
  - (63, 63, 63) #3F3F3F [dark-gray] - count=1305
- **center**:
  - (0, 0, 0) #000000 [near-black] - count=45048
  - (255, 255, 255) #FFFFFF [near-white] - count=3941
  - (63, 63, 63) #3F3F3F [dark-gray] - count=2985

---

## 三、综合观察与推断

### 推断为洞穴地图的截图

- `PixPin_2026-07-19_18-56-50.png` — 类型: corner-minimap, 尺寸 1920x1080, 暗区占比 18.8%
- `PixPin_2026-07-19_18-57-08.png` — 类型: corner-minimap, 尺寸 1920x1080, 暗区占比 11.1%
- `PixPin_2026-07-19_18-57-26.png` — 类型: corner-minimap, 尺寸 1920x1080, 暗区占比 48.2%
- `PixPin_2026-07-19_18-57-38.png` — 类型: full-screen-cave-map, 尺寸 1920x1080, 暗区占比 92.9%
- `PixPin_2026-07-19_18-58-14.png` — 类型: full-screen-cave-map, 尺寸 1175x804, 暗区占比 83.6%

### 洞穴地图截图汇总 Top 颜色

| RGB | Hex | 类别 | 累计采样数 |
| --- | --- | --- | --- |
| (0, 0, 0) | #000000 | near-black | 214410 |
| (45, 46, 49) | #2D2E31 | very-dark | 5821 |
| (21, 31, 52) | #151F34 | very-dark | 4335 |
| (25, 26, 29) | #191A1D | very-dark | 3781 |
| (32, 33, 35) | #202123 | very-dark | 3750 |
| (23, 33, 54) | #172136 | very-dark | 3658 |
| (24, 34, 55) | #182237 | very-dark | 3645 |
| (18, 28, 49) | #121C31 | very-dark | 3402 |
| (20, 30, 51) | #141E33 | very-dark | 3078 |
| (22, 32, 53) | #162035 | very-dark | 2919 |
| (17, 27, 48) | #111B30 | very-dark | 2551 |
| (68, 65, 40) | #444128 | reddish | 2248 |
| (255, 255, 255) | #FFFFFF | near-white | 2203 |
| (63, 63, 63) | #3F3F3F | dark-gray | 1960 |
| (22, 33, 54) | #162136 | very-dark | 1779 |

### 颜色语义推断 (Xaero 洞穴地图典型表现)

- **黑色 (RGB 近 0,0,0)**: 未探索区域 / 洞穴地图背景
- **白色或浅灰 (RGB > 220)**: 已探索的洞穴通道/隧道
- **中灰 (~128)**: 过渡区域或半透明叠加
- **深灰 (<60)**: 洞穴墙壁或阴影
- **彩色 (蓝/红/绿)**: 玩家箭头、坐标文本、图标或生物群系着色
- **红色**: 玩家朝向箭头常用色 / 警告色
- **黄色/橙色**: 坐标文本 / 路径点

### 关于位置与 UI 控件

- Xaero's Minimap 默认小地图位于屏幕右上角，可配置
- 洞穴地图 (Cave Map) 与表面地图切换显示，按 ']' 键切换
- 全屏大地图按 'X' 打开，可同时显示洞穴层
- 坐标标签通常显示在地图下方: `X / Y / Z`
- 玩家箭头为指向当前朝向的三角形
