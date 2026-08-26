# OPT04B — Renderer State / CPU Overhead Cleanup

OPT04B removes measurable CPU/driver overhead from renderer hot paths without
changing shader math, draw order, render resolution, shadow quality, cloud
quality, weather authority, water authority, physics, or `.hhyd` v15.

## Entity mesh handoff

`EntityMeshRenderer` no longer calls `glIsEnabled(GL_BLEND)` every draw and no
longer performs a blanket texture-cleanliness sweep after material rendering.
The old cleanup dynamically issued 54 texture/sampler/active-unit calls after a
normal mesh draw; the replacement performs only the three state handoff calls
that matter (release shadow sampler overrides on units 10/11 and normalize the
active texture unit). VAO and canonical blend/cull state handling remain.

Texture bindings themselves are intentionally allowed to remain resident.
Every following renderer explicitly binds the texture unit/target it consumes.
Shadow sampler objects are still unbound because sampler objects override
texture sampling state across targets and units 10/11 are reused later.

## Volumetric sky/cloud handoff

`SkyRenderer` no longer queries `GL_SAMPLES` from the driver every cloud frame.
The framebuffer sample count is already known by `EngineRendering`; it now
travels through `EntityMeshRenderTargetState` and `SkyRenderTargetState`.
Spanning mode explicitly supplies one sample because the span FBO is
single-sampled.

Sky background rendering no longer zero-binds its three sampled textures after
the pass, and volumetric-cloud presentation no longer clears four possible
texture targets across units 0-7. The next passes bind their inputs explicitly,
so only the active texture unit is normalized.

## Rain presentation

The active-rain path previously captured six pieces of OpenGL state every draw:
three enable flags, depth write mask, active texture unit, and viewport. The
viewport dimensions are now supplied directly by `EngineRendering`; the pass
owns a deterministic output state (depth test on, depth writes on, culling on,
blending off, active texture unit zero).

This removes the six synchronous state queries without changing the intentional
WEATHER09A rule that fixed-function depth remains disabled during the actual
transparent rain draw.

## Uniform lookup cleanup

`PostProcessor` and the small `SceneRenderer` now resolve uniform locations once
at initialization. They no longer call `glGetUniformLocation` in their draw
functions. This removes two name lookups from the normal post-processing frame
and ten lookups from each logo-scene draw.

## Static hot-path call reduction

For a normal single-viewport Racing United frame with entity rendering,
volumetric clouds, rain, and final post-processing active, the changed paths
remove roughly 100 redundant/query/name-resolution OpenGL calls per frame
before counting any driver-internal savings. This is a static API-call count,
not a claimed frame-time result; the existing CPU/GPU timers remain the runtime
authority.

OPT04A ownership splits, OPT00 asynchronous profiling, OPT03C4 single GPU-water
authority, OPT03B tire-water sampling and OPT02 `.hhyd` v15 remain intact.
