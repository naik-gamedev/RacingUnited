# OPT06 — Optimization Freeze and Stabilization Audit

OPT06 closes the OPT00–OPT05 optimization campaign. It is intentionally a stabilization milestone rather than another renderer, weather, physics, or hydrology redesign.

## Production invariants frozen by this milestone

- Dynamic Surface spatial water has one production authority: `DynamicSurfaceGpuRuntime`.
- The historical CPU `DynamicSurfaceHydrology` implementation remains test/reference-only.
- Tire water sampling remains the OPT03B nonblocking three-slot SSBO bridge; no full atlas readback and no GPU wait are permitted.
- `.hhyd v15` prebaked hydrology topology/capacity/flow remains the production cache format and is not modified by OPT06.
- OPT04A renderer ownership split remains in place.
- OPT04B renderer hot paths do not query state already owned by frame orchestration and do not blanket-zero texture units.
- OPT04C shares frame-local mesh/animation/node preparation between shadow and material passes without reducing shadow quality.
- OPT05 cloud temporal history ping-pongs instead of copying a full RGBA16F image, and default upscale remains fused into temporal reconstruction.
- Native runtime stdout/stderr, Windows fault capture, and minidump support remain enabled.

## Final live hot-path cleanup

`EntityDebugRenderer` was the last renderer found resolving uniform names from the OpenGL driver during its live draw path. OPT06 caches all nine uniform locations once during initialization and reuses them. Consecutive debug instances that use the same primitive VAO also avoid redundant VAO binds.

This does not alter debug primitive shading, transforms, camera-relative coordinates, geometry, or draw ordering.

## Build-time freeze inventory

`CodeHealthAudit.ps1` now records an `OPT06 OPTIMIZATION-FREEZE INVENTORY` in `Build/Reports/CodeHealthSnapshot.txt`. The validation safety net requires the following production invariants to remain zero:

- debug-render draw-time `glGetUniformLocation` calls;
- known synchronous renderer state queries removed by OPT04B;
- OPT05 full-image cloud temporal copies;
- blocking graphics `glFinish`, `glGetTextureSubImage`, or `glReadPixels` calls;
- production `DynamicSurfaceHydrology` source/header files.

The audit is static and intentionally does not add runtime GPU synchronization or disk writes to measured render frames.

## What OPT06 does not do

OPT06 does not reduce cloud resolution, raymarch steps, shadow resolution/cascade count/filter quality, hydrology resolution, tire sampling fidelity, material quality, or draw distance. It does not change weather tuning or water appearance.

After this milestone, further performance work should be evidence-driven from the existing F8 CPU/GPU timers and should target a measured bottleneck rather than continue broad architectural refactoring.
