# ADR-053 — Heritage Engine shell and domain-owned Lua registration

Status: Accepted candidate (CLEAN07)

## Context

The executable `main.cpp` had already shed most subsystem implementations, but it still contained
roughly two thousand lines of process startup, display-policy helpers, diagnostics UI, Windows
clipboard capture and the full long-running engine loop. That made the executable entry point the
place where unrelated process-level concerns naturally accumulated.

`LuaModuleRuntime::registerBindings()` had a similar scaling problem: hundreds of exact
`registerFunction(...)` calls lived in one master catalogue even though handler implementations were
already split into UI, input, physics, vehicle, entity and other domain translation units.

## Decision

The process-level coordinator is named **HeritageEngine**, not HeritageApplication. The executable
entry point constructs `heritage::engine::HeritageEngine` and delegates to `run(argc, argv)`.

Stable responsibilities are extracted immediately:

- project-root discovery and launch diagnostics -> `HeritageEngine/Runtime/EngineStartup.*`;
- global ImGui style policy -> `HeritageEngine/Runtime/EngineUiStyle.*`;
- display mode apply/change/confirm/revert -> `HeritageEngine/Display/DisplayModeController.*`;
- F8 performance UI -> `Core/Diagnostics/PerformanceOverlay.*`;
- Windows F12 backbuffer clipboard capture -> `Platform/Windows/BackbufferClipboard.*`.

`HeritageEngine.cpp` remains the frame/process orchestrator. `EngineFrame.cpp`,
`EngineSimulation.cpp`, `EngineRendering.cpp` and `EngineHotkeys.cpp` are created as deliberately
non-compiled future destinations. We do not create a giant mutable context object merely to make a
line-count metric look smaller; those phases move only when their inputs/outputs can be stated
cleanly.

Lua API registration becomes domain-owned. `LuaModuleRuntime::registerBindings()` keeps only the
runtime-owned global `print` replacement and calls ordered `register*Bindings()` domain methods.
Small domains register beside their handlers. Entity, Physics and Vehicle use dedicated registration
translation units because each table spans several handler files. The manifest generator scans the
whole runtime/binding registration source set and records each binding's actual registration source.

## Consequences

- `main.cpp` becomes an intentionally boring executable boundary.
- Process/display/platform/diagnostic code has obvious long-term owners.
- The central engine coordinator remains visible without being treated as a general dumping ground.
- Future frame-phase extraction has named destinations but is not forced through premature shared
  mutable state.
- Adding a Lua API function normally touches the domain that owns it rather than the runtime master
  catalogue.
- Exact Lua API names remain generated/validated from source; distributed registration does not
  weaken manifest verification.

## Non-goals

CLEAN07 does not change simulation equations, render order, input semantics, module lifecycle,
window-mode behavior or Lua API names. It also does not attempt to make HeritageEngine itself a
reusable library boundary yet; it is the executable runtime coordinator.
