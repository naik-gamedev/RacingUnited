# OPT00 — Profiling Baseline

OPT00 is the measurement checkpoint that precedes structural deletion/refactoring in the 2026-08-25 code-health roadmap. It intentionally changes no gameplay, physics, hydrology, sky, cloud, material or vehicle behavior.

## Runtime GPU timing

Heritage already had a frame-wide asynchronous `GL_TIME_ELAPSED` timer plus specialized shadow/Dynamic Surface timers. OPT00 adds a reusable `AsyncGpuTimer` based on non-blocking `GL_TIMESTAMP` pairs. Timestamp pairs can be nested inside the existing frame timer and inside renderer owners without illegal nested `GL_TIME_ELAPSED` queries.

The F8 overlay now reports completed GPU samples for:

- module render;
- complete entity/mesh renderer;
- surface presentation;
- weather/rain presentation;
- debug rendering;
- MSAA resolve;
- final post-process/presentation;
- sky background/celestial pass;
- volumetric cloud shadow pass;
- cloud scene color/depth copy;
- cloud raymarch;
- cloud upscale/combine;
- cloud temporal reprojection/history copy;
- final cloud presentation/depth merge.

No `glFinish()` or blocking current-frame query read is introduced. A timestamp result is retrieved only after `GL_QUERY_RESULT_AVAILABLE` reports that the older end timestamp has completed. The four-slot ring skips a sample rather than waiting if the GPU falls more than the ring depth behind.

Per-pass top-level GPU timings are intentionally omitted in multi-monitor spanning mode because the render passes interleave once per selected monitor. The existing frame-wide GPU timer remains authoritative there.

## Repeatable code-health inventory

`Tools/Diagnostics/CodeHealthAudit.ps1` generates `Build/Reports/CodeHealthSnapshot.txt` before validation on every `00_BuildAndRunCurrent.cmd` run. It records:

- C/C++/INL and Lua file/line totals;
- largest C++ and Lua files;
- number of 1000+ line C/C++ files and 500+ line Lua files;
- compiled translation units from the engine + physics-test projects;
- `.cpp` files on disk but absent from those projects;
- Racing United Lua reachability from `Scripts/Main.lua`;
- exact unreachable Lua inventory.

This turns OPT01 dead-code deletion into an evidence-based operation instead of a naming/age guess.

## Baseline captured in this archive

The static OPT00 snapshot currently reports approximately:

- 499 C/C++/INL files / 136,160 lines;
- 93 Lua files / 11,595 lines;
- 21 C/C++ files at or above 1,000 lines;
- 54 `.cpp` files not compiled by the two active engine/test projects;
- 88/93 Racing United Lua files reachable from `Scripts/Main.lua`;
- five unreachable Lua files.

The next checkpoint, OPT01, may delete only sources whose dead/unreachable status remains proven by build/project references, symbol search and the runtime/regression gates.
