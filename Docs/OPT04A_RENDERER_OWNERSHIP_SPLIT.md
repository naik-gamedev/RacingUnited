# OPT04A — Renderer Ownership Split

OPT04A is a structural renderer cleanup only. It deliberately does not change
surface presentation, sky/cloud rendering algorithms, water authority, weather
authority, physics, shader math, shader constants, draw order, or OpenGL state
policy.

## Surface presentation

`SurfacePresentationRenderer.cpp` remains the lifecycle/draw orchestrator.
Persistent tire-mark GPU cache work is owned by
`SurfacePresentationTireMarks.cpp`; resting/moving rubber presentation is owned
by `SurfacePresentationRubber.cpp`; embedded GLSL is owned by
`SurfacePresentationShaders.cpp`; compact private GPU record vocabulary lives
in `SurfacePresentationRendererInternal.hpp`.

The root implementation is reduced from 2434 lines to 693 lines.

## Sky and volumetric clouds

`SkyRenderer.cpp` remains the runtime resource/pass orchestrator. Atmospheric
sky GLSL is owned by `SkyRendererAtmosphereShaders.cpp`; volumetric cloud,
temporal, depth, upscale/present, and cloud-shadow GLSL is owned by
`SkyRendererCloudShaders.cpp`.

The root implementation is reduced from 1476 lines to 359 lines.

## Compatibility

All moved renderer method bodies were compared against the pre-OPT04A source
and are text-identical. All moved GLSL raw-string bodies were compared against
the pre-OPT04A source and are byte-for-byte identical.

OPT00 asynchronous GPU timers, OPT03C4 single GPU-water production authority,
OPT03B nonblocking tire-water sampling, and OPT02 `.hhyd` v15 remain unchanged.
