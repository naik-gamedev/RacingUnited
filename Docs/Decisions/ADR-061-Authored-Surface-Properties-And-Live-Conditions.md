# ADR-061 — Authored Surface Properties And Live Conditions

**Status:** Accepted for TIRE15B1 candidate (2026-08-10)

## Context

TIRE15 proved persistent mud/sand/soft-soil/deep-snow terramechanics, but the material mechanics were still synthetic tire-side presets and winter temperature used a compatibility constant. CLEAN10 established world-owned, global-coordinate-safe `SurfaceWorld`, so scene and weather ownership can now become authoritative without putting world state back inside the vehicle.

## Decision

1. Physical deformable-material parameters belong to `Physics/Surfaces/SurfaceMaterialProperties`, not the tire solver. The legacy TIRE15 values become validated material-family defaults.
2. Static collision GLB nodes may override these values through `heritage.surface.*` numeric metadata. Parent metadata is inherited and child metadata may refine it.
3. `SurfaceWorldEnvironment` owns bounded live global wetness, ambient temperature and an optional road-temperature override. Effective wetness combines authored/local wetness with global wetness. Road-temperature precedence is runtime override, then scene-authored local temperature, then material-family fallback.
4. Wet, winter, deformable-terrain and tire-thermal mechanisms consume this shared resolved state. The named wheel telemetry table may grow; the legacy positional 169-value ABI remains frozen.
5. Lua exposes the environment seam for later weather/day-night systems, but those systems do not own tire physics.
6. TIRE15B presentation is staged separately as TIRE15B2. Visual ruts, spray, dust, snow/mud displacement and audio consume authoritative surface state rather than creating another simulation state.
7. Tire rubber/marbles remain the dedicated TIRE15C rubber subsystem; they are not generalized into deformable terrain.

## Consequences

Existing scenes remain behaviorally compatible when they do not author the new metadata. Creator worlds can progressively replace estimated defaults with measured/fitted surface data, and weather can affect every vehicle through one shared world state. The staged split keeps the physics/data integration independently testable before presentation work.
