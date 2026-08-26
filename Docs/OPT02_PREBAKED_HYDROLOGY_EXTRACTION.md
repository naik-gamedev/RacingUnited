# OPT02 — Prebaked Hydrology Extraction

OPT02 removes the retired WATER14–WATER17 adaptive CPU hydrology generation from `SurfaceHydrology` and leaves one clear responsibility: immutable scene-derived `.hhyd v15` topology used by the current Dynamic Surface/GPU water runtime and precipitation-cover queries.

## Production ownership after OPT02

`SurfaceWorld` no longer advances, resets, schedules, or applies tire contacts to `SurfaceHydrology`. Runtime water/moisture/flow remains owned by Dynamic Surface, with the renderer GPU authority used in production and the Dynamic Surface CPU implementation retained only as the current fallback/regression path for OPT03.

`SurfaceHydrology` now owns only static scene topology and cache data:

- `SurfaceHydrology.cpp` — small facade, support-raster compatibility metadata, bake orchestration and fingerprinting.
- `SurfaceHydrologyTopology.cpp` — welded collision vertices, priority-flood spill head, MFD downhill routing, contributing runoff area and triangle/tile lookup.
- `SurfaceHydrologyTiles.cpp` — near-tile reconstruction and compressed 32×32 far-tile payload generation.
- `SurfaceHydrologyCover.cpp` — exact same-column triangle-space precipitation shelter query, with compatibility support-raster fallback.
- `SurfaceHydrologyCache.cpp` — byte-compatible `.hhyd v15` read/write and validation.

## Retired implementation

The following adaptive/live CPU hydrology mechanisms were removed from `SurfaceHydrology`:

- `AdaptiveCell` and `AdaptivePipe` state;
- virtual-pipe flow and `VirtualPipeFlow.hpp`;
- adaptive spatial buckets and topology rebuild;
- cadence bands and per-cell due scheduling;
- `advance()` / `simulateStep()` water stepping;
- adaptive tire-contact mutation;
- presentation-basin state and visual-cell gathering;
- test-only APIs that existed solely to keep the retired solver alive.

The tire-fleet benchmark was migrated to `DynamicSurfaceSystem`, so benchmarking no longer resurrects the retired solver as a hidden dependency.

## Compatibility gates

OPT02 intentionally preserves:

- `.hhyd` cache version **15** and its serialized structures;
- the exact 16-level standing-depth ladder used by the current presentation path;
- welded-mesh priority-flood spill topology;
- MFD contributing-runoff accumulation and downhill flow vectors;
- 10 m near-tile reconstruction and compressed 32×32 far payloads;
- exact triangle-space precipitation-cover behavior;
- existing `SurfaceWorld` public tire-contact compatibility types while the implementation is routed through Dynamic Surface.

The native regression now protects the current prebaked authority rather than the deleted adaptive solver.

## Overlay-safe deletion

ZIP extraction cannot delete stale files from an existing checkout. `Tools/Diagnostics/ApplyOPT02Retirement.ps1` therefore removes `VirtualPipeFlow.hpp` before freshness audit and repository validation.
