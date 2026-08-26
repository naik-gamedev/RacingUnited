# OPT01 — Proven Dead-Code Retirement

OPT01 is the first destructive cleanup stage in the code-health roadmap. It removes only files for which OPT00 established strong structural evidence of non-use.

## Removed

- 54 `.cpp` files that existed in the Heritage Engine tree but were compiled by neither active Visual Studio project.
- The orphan `TireCarcass3D.hpp` API, whose implementation/header pair had no external caller.
- The unreachable Racing United Cloud Lab Lua panel.
- Four empty/unreachable Lua topology modules.
- Completed migration/signpost translation units for glTF, SurfaceField and the retired VehicleConfiguration umbrella.
- Fifty 2–4 line future-vehicle mechanism `.cpp` placeholders.

## Replaced with architecture, not pretend code

Future vehicle mechanism intent now lives in `VEHICLE_SUBSYSTEM_ARCHITECTURE_MANIFEST.md`. The rule is simple: **create source only when implementation exists**. A future subsystem may have a documented seam, but it does not receive a `.cpp` file until it owns real compiled behavior and tests.

The Visual Studio project group was renamed from `VehicleArchitectureScaffolds` to `VehicleArchitecture`; it now tracks real implementation units only. Empty project filters and empty source directories left behind by the retired placeholders were pruned.

## Safety-net changes

- Code-health validation now requires zero project-tree `.cpp` files outside the active engine/test compile graphs.
- Racing United Lua reachability now requires zero unreachable scripts.
- CLEAN03A/ARCH04 validators guard documented topology/mechanism intent instead of requiring empty source files.
- The legacy `Physics/SurfaceField.hpp` compatibility include remains, but its dead `.cpp` signpost is gone.
- The real glTF facade under `Graphics/Gltf/` remains the only implementation owner.

## Runtime effect

No gameplay, physics, rendering, hydrology, weather or Lua runtime behavior is intentionally changed. OPT00 asynchronous GPU profiling remains active for the structural stages that follow.
