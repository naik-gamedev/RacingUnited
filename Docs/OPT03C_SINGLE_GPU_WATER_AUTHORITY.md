# OPT03C — Single GPU Water Authority

## Goal

Remove the dormant production CPU Dynamic Surface water solver after OPT03B gave tire physics a nonblocking path to the same live GPU water field used by rendering.

## Production authority

`DynamicSurfaceGpuRuntime` is now the only spatial live-water authority in a normal Heritage Engine runtime. It reconstructs standing water and kinematic runoff from the immutable `.hhyd v15` topology and owns localized tire dry-line mutation.

`SurfaceWorld` no longer advances, samples, resets, or tire-mutates a CPU `DynamicSurfaceHydrology` lattice. When GPU spatial samples are unavailable or stale, physics falls back only to the scalar weather-film depth. That fallback does not create a second spatial water simulation.

## Tire physics

OPT03B's three-slot SSBO bridge remains the physics handoff. Active tire/footprint sample positions are batched, sampled after GPU tire clearing, and consumed only when a fenced slot is ready. There is no full-atlas readback, `glFinish`, or CPU wait for the GPU.

## Retired production solver

The former `Physics/Surfaces/DynamicSurface/DynamicSurfaceHydrology.cpp/.hpp` production implementation is removed from the engine project and from the production source tree.

A renamed `Tests/Reference/DynamicSurfaceHydrologyReference` copy is compiled only by `HeritagePhysicsTests`. It preserves historical conservation, drainage, interest-union, and tire-clearing regressions as an oracle without being reachable by the shipping runtime.

ZIP overlays cannot delete old files, so `Tools/Diagnostics/ApplyOPT03Retirement.ps1` converges existing checkouts by removing the retired production CPU Hydro files before validation.

## Other ownership cleanup

- Track thermal environmental cooling consumes weather wetness rather than a hidden CPU Hydro simulation.
- The CPU tire-fleet benchmark no longer creates a synthetic spatial hydrology world; it benchmarks the tire stack with explicit wet inputs. Live GPU-water cost belongs to renderer/runtime profiling.
- Lua/Scene hydrology diagnostics expose immutable `.hhyd` support/cache information and direct live-water inspection to the F8 GPU runtime telemetry.

## Compatibility guarantees

- `.hhyd v15` topology/cache implementation is unchanged.
- Standing-depth ladder, MFD contributing runoff area, and baked flow direction are unchanged.
- OPT03B GPU tire-water sampling behavior is retained.
- No new synchronous GPU readback is introduced.
