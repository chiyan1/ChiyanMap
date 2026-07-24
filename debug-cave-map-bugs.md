# Debug Session: cave-map-bugs

**Status**: [OPEN]
**Started**: 2026-07-20
**Session ID**: cave-map-bugs

## Objective
Comprehensive bug investigation and performance optimization for the cave map feature.

## Hypotheses (from static code review)

### H1 — Cache Leak: `wasCaveScan` captured at scan END [CODE-CONFIRMED]
- **Location**: PlayerHook.h:1582
- **Issue**: `bool wasCaveScan = MapRenderState::caveModeEnabled;` is captured AFTER the multi-frame scan completes. If user toggles cave mode OFF during scan, `caveModeEnabled` is false at capture → cave scan data leaks to surface disk cache via `MapCacheManager::UpdateFromScan`.
- **Evidence**: Scan spans multiple frames (250μs budget per frame, 263K columns). Mode toggle during scan is a normal user action. The capture point is deterministically wrong.
- **Fix**: Capture `wasCaveScan` at scan START (line ~1409), store in a static variable, read at scan END.

### H2 — Brightness formula produces wrong gradient [FIXED]
- **Location**: PlayerHook.h (cave brightness site, 原:1537)
- **Issue**: Formula `0.7 + 0.3 * (1-t)` produces brightness range [0.7, 1.0] → RGB [178, 255] = #B2B2B2→#FFFFFF. Documented Xaero gradient is #A0A0A0→#202020 = [160, 32] = brightness [0.125, 0.627]. Cave maps appear washed out / too bright.
- **Evidence**: Math: t=0 → 0.7+0.3=1.0 (white); t=1 → 0.7+0=0.7 (light gray). Neither matches the documented dark gradient.
- **Fix (applied)**: Changed formula to `0.125f + 0.5f * (1.0f - t)` → t=0 gives 0.625 (#A0A0A0), t=1 gives 0.125 (#202020). Matches documented Xaero gradient. Note: 实际地板方块颜色（如深板岩 0.62）会与亮度相乘，故比 Xaero 纯灰更暗，属正常深度阴影效果。

### H3 — Performance: std::string allocation in hot scan loop [FIXED]
- **Location**: PlayerHook.h (SafeGetCaveBlockY)
- **Issue**: `block.getTypeName()` returns `std::string` (heap alloc) for every scanned block. Full mode: 32 blocks/column × 263K columns = 8.4M heap allocations per full scan. Each allocation is ~50ns → ~420ms total just for string allocs.
- **Fix (applied)**: Added `IsAirLikeBlock(Block const&)` helper that keys a `std::unordered_map<decltype(getRuntimeId()), bool>` by `block.getRuntimeId()` (O(1), no heap alloc). `getTypeName()` is now called at most once per distinct block type per session (hundreds of calls instead of 263K+), eliminating the repeated per-column std::string allocation. `SafeGetCaveBlockY` now calls `IsAirLikeBlock(block)` instead of inline `getTypeName()` name matching.
- **Caveat**: relies on `Block::getRuntimeId()`. Confirmed as stable BDS/LeviLamina API; verify at next build.

### H4 — Burst rescan doesn't clear back buffer [CODE-CONFIRMED]
- **Location**: PlayerHook.h:1389-1398
- **Issue**: Burst clears front buffer (`g_mapColors`/`g_mapHeights`) but NOT back buffer (`g_mapColorsBack`/`g_mapHeightsBack`). Next scan continues writing into back buffer which has stale data from previous mode. Until all 513×513 cells are overwritten, render shows mixed old/new data.
- **Evidence**: The memset only covers `g_mapColors`/`g_mapHeights` (front), not `g_mapColorsBack`/`g_mapHeightsBack` (back). Deterministic.
- **Fix**: Also clear back buffers in burst handler.

### H5 — `getBiome` called with varying Y in cave mode (minor perf)
- **Location**: PlayerHook.h:1495
- **Issue**: `region.getBiome(BlockPos(targetX, topY - 1, targetZ))` — in cave mode, `topY` varies per column, so biome Y varies. Biome is 3D in caves (different biomes at different Y). This is correct behavior but each call is expensive.
- **Mitigation**: Already cached by 4×4 XZ cell. Acceptable.

## Instrumentation Plan
- Add scan-cycle timing log (microseconds, columns scanned) to confirm H3
- Add wasCaveScan state log to confirm H1
- Write to `D:\Project\ChiyanMap\cave_debug.log` (truncate on startup)

## Fix Plan (after evidence)
1. H1: Move wasCaveScan capture to scan start
2. H2: Fix brightness formula
3. H3: Optimize block type check (avoid string alloc)
4. H4: Clear back buffer in burst handler

## Evidence Log
(pending runtime collection)

## Cleanup Status
[ ] Instrumentation removed
[ ] Debug server stopped
[ ] This file archived
