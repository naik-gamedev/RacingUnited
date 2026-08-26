# OPT03 — Production Dynamic Surface GPU Runtime Split

OPT03 removes the word **Prototype** from the live Dynamic Surface GPU architecture and decomposes the former 1,637-line implementation by responsibility without changing the current `.hhyd v15` water model.

## Production ownership

`DynamicSurfaceGpuRuntime` is the single renderer-side owner of current near/far prebaked water presentation, optional snow/mud compute, localized tire dry-line events and its GPU timer. Its implementation is now divided into:

- `DynamicSurfaceGpuRuntime.cpp` — lifecycle and per-frame coordination
- `DynamicSurfaceGpuResources.cpp` — state textures and lazy optional-state resources
- `DynamicSurfaceGpuResidency.cpp` — near-tile residency and indirection
- `DynamicSurfaceGpuTopology.cpp` — far prebaked topology cache admission
- `DynamicSurfaceGpuGeometry.cpp` — optional exact geometry support for snow/mud
- `DynamicSurfaceGpuDispatch.cpp` — bounded optional-state compute dispatch
- `DynamicSurfaceGpuTireEvents.cpp` — localized tire event compute
- `DynamicSurfaceGpuTimers.cpp` — asynchronous compute timing
- `DynamicSurfaceGpuShaders.hpp` — compute shader sources

The coordinator is ~412 lines instead of 1,637. No split implementation exceeds 500 lines.

## Dead renderer mirror retired

The old `DynamicSurfaceGpuPagePool.cpp/.hpp` was compiled but had no live initialization/synchronization/upload call path. Its only remaining production interaction was a no-op shutdown. OPT03 removes the class, stale renderer methods, stale Track-upload scratch/maps and their permanently-zero F8 telemetry. `DynamicSurfacePagePool` on the **physics** side remains intact because it is still authoritative for persistent Track/rubber/thermal page residency.

## CPU Hydro audit

`DynamicSurfaceHydrology` is **not deleted in OPT03**. `SurfaceWorld` still deliberately calls it when `m_gpuDynamicSurfaceAuthorityEnabled == false`, and the native regression suite exercises it as a reference/failure path. The normal GPU-authoritative path already skips CPU Hydro advancement. Final retirement should wait until GPU startup is a hard supported-platform contract and tire physics has the intended live GPU water-sample bridge rather than its current weather-film fallback approximation.

This keeps one live water authority during normal supported operation without deleting a still-functional recovery/test path prematurely.
