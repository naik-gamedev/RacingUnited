# ADR-067 — Layered cascaded shadow submission

## Status
Accepted candidate (SHADOW02)

## Context
The first Heritage directional CSM implementation rendered four texture-array layers with four CPU submission passes. In a simple Racing United scene the F8 overlay showed hundreds of shadow draws and shadow CPU wall time becoming a dominant portion of render submission even though whole-frame GPU time still had headroom. Repeating node/range work, uniforms, skin palettes and indexed draws once per cascade does not scale to large race grids or vegetation.

## Decision
Keep four conventional-depth CSM layers and all existing visual shadow policy, but attach the complete depth-array texture as one layered framebuffer. The shadow vertex stage performs skinning/tire deformation once and produces camera-relative world position. A geometry stage applies the four light-view-projection matrices and emits each source triangle only to the cascade layers selected by a CPU-generated bit mask via `gl_Layer`. Static bounds still determine exact cascade participation.

Because the shadow pass is depth-only and currently treats casters as opaque, contiguous mesh draw ranges that share the same node transform and skin may be coalesced across material boundaries. Hidden authoring ranges and node-prefix filtering remain hard batch boundaries.

## Consequences
- A shadow range is submitted once instead of once per intersecting cascade.
- Vertex skinning and tire visual deformation execute once per source draw rather than once per cascade.
- The geometry shader adds cascade fan-out work, but emitted shadow triangle work remains comparable to the old pass and can be lower where cascade culling removes layers.
- Material-only GLB primitive splits can collapse into fewer shadow draw calls without altering visible material rendering.
- `shadowDrawCalls` now tracks physical OpenGL submissions while `shadowTriangles` tracks cascade-emitted triangle work.
- The implementation remains OpenGL 3.3-compatible in shader feature level (geometry shaders/layered framebuffer are core) and uses Heritage's 4.6 Windows context.
- Full GPU-generated indirect command lists are deliberately deferred until profiling shows they are necessary after this substantially cheaper submission path.
