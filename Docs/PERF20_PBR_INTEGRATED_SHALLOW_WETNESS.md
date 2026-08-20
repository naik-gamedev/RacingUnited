# PERF20 – PBR-Integrated Shallow Wetness

## Problem
PERF19's isolated second wet-film draw followed the authored triangles, but it lit that second draw from geometric vertex normals only. When rain activated, the smooth overlay reflection visually replaced/wash-out the underlying material normal-map detail, producing broad streaky terrain shading.

## Decision
Shallow hydrology wetness is now a tagged material response inside the existing entity PBR fragment shader. There is no second authored-geometry wet-film draw.

For `SurfaceWetnessReceiver` entities only, the material shader samples the existing 400x400, two-layer hydrology atlas and the user-authored shoreline breakup mask. The normal/base/roughness texture maps are sampled exactly through the ordinary material path first. Wetness then:

- darkens the already-sampled base colour modestly;
- lowers the already-sampled roughness toward a wet target;
- leaves the reconstructed authored normal map unchanged;
- therefore makes the same material micro-normal detail drive dry and wet lighting/reflections.

The breakup mask shapes only the advancing wet/dry front. Once sufficiently wet, the material response becomes uniform rather than retaining a noise pattern.

## Safety
The old WATER04-style risk is addressed by making the wetness branch explicit and opt-in (`SurfaceWetnessReceiver`), by fully resolving/binding its uniforms and samplers during normal material-program initialization, and by avoiding all shader switches, duplicate geometry submission, framebuffer changes, and blend/depth-state changes. Non-receiver entities never sample the hydrology atlas.

Explicit connected puddle geometry remains separate and retains PERF19's 6/8/12 mm distance thresholds and <=25 degree basin-like support rule.
