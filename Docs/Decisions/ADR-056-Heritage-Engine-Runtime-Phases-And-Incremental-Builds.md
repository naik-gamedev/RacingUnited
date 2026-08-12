# ADR-056 — Heritage Engine runtime phases and incremental development builds

Status: Accepted candidate (CLEAN09)

## Context

CLEAN07 reduced `main.cpp` to the executable entry point and created named runtime-phase destinations,
but `HeritageEngine.cpp` still directly implemented hotkeys, fixed/render-frame simulation coordination,
render-target/GPU-timing orchestration and frame timing/presentation. Long-lived settings and core engine
services also remained anonymous translation-unit globals.

The rolling developer helper additionally forced both the physics regression executable and the engine
through `/t:Rebuild` on every run. That discarded valid object files and made even small ownership edits
pay nearly full project compile cost.

## Decision

Promote the four CLEAN07 runtime destinations into compiled owners with explicit contracts:

- `Runtime/EngineFrame.*` owns frame begin/timing, framebuffer readiness, screenshot timing exclusion and
  final presentation/FPS limiting;
- `Runtime/EngineHotkeys.*` owns F5/F6/F7/F8/F11/F12/PrintScreen/ESC edge state and actions;
- `Runtime/EngineSimulation.*` owns fixed-world stepping, module/environment update, entity pose sync and
  per-frame chase/fallback camera update;
- `Runtime/EngineRendering.*` owns post-processing render targets, resize/AA/scale preparation, GPU timer
  queries and scene render orchestration.

`HeritageEngine` owns an internal `EngineRuntimeState` through a private `std::unique_ptr`. That state is
restricted to process-lifetime services/settings/fonts/paths. It is not a giant per-frame scratch context;
frame-local data remains in `EngineFrameData`, render-only resources remain in `EngineRenderingState`, and
hotkey edge state remains in `EngineHotkeyState`.

The default `Tools/00_BuildAndRunCurrent.cmd` path uses MSBuild `/t:Build` for both regression tests and
the engine. Passing `FULL` selects `/t:Rebuild` for explicit checkpoint/diagnostic clean rebuilds without
adding another step-specific helper script.

## Ordering invariant

The extraction preserves the established frame order:

1. vsync/frame limiter begin;
2. event/input/audio update;
3. hotkeys;
4. window/display-mode update and framebuffer readiness;
5. render-target/AA/scale/span-FBO preparation;
6. frame-time sampling and screenshot-pause exclusion;
7. fixed simulation/module/environment/camera update;
8. world rendering and GPU timing;
9. module/engine ImGui;
10. optional exact backbuffer clipboard capture;
11. GPU query close, swap/present, diagnostics end-frame and FPS limiting.

The render-target preparation intentionally remains before frame-time sampling, matching the prior runtime
so resize/span maintenance does not silently change gameplay delta-time semantics during the refactor.

## Consequences

- `HeritageEngine.cpp` remains the startup/shutdown and high-level frame coordinator instead of owning each
  mechanism inline.
- New frame/simulation/render/hotkey work has a clear owner and no reason to regrow the central file.
- Process services are explicit engine-owned state rather than hidden globals.
- Normal iterative builds can reuse unchanged object files; a full rebuild remains one explicit command-line
  mode of the same rolling helper.
- A later shared native simulation library can still remove duplicated compilation between the engine and
  regression project; CLEAN09 does not force that larger project-boundary change.

## Non-goals

CLEAN09 does not change tire/vehicle equations, fixed-step frequency, render order, camera tuning, input
bindings, display policy, post-processing behavior or Lua APIs. It also does not move startup/shutdown into
another giant class merely to reduce a line-count metric.
