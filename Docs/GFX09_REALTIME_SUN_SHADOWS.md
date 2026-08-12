# GFX09 / SHADOW03 — Real-time Sun Shadows

## Purpose

Heritage Engine now renders the moving day/night sun with real-time directional shadows for module mesh entities. The implementation is camera-relative and follows the existing EnvironmentSystem sun direction, so shadow direction changes continuously with time of day without changing authored assets.

## Current implementation

- Four cascaded directional shadow maps (CSM).
- D32F depth texture array with live Low/Medium/High/Ultra presets at 1024/2048/3072/4096 per cascade. Ultra/4096 is the default and allocation is clamped to the GPU maximum texture dimension.
- Cascade view-depth ranges: 0.10–20 m, 20–70 m, 70–220 m, 220–800 m.
- Shadow matrices are built in the same camera-relative FP32 space used by the renderer. FP64 world/floating-origin state remains unchanged.
- Main view remains reversed-Z. Shadow maps deliberately use conventional GL_LESS depth internally, then restore Heritage's reversed-Z state before normal rendering resumes.
- 3x3 PCF filtering softens shadow-map texel edges. The shadow array is a hardware comparison sampler: Video Settings can select Nearest comparisons or Linear hardware-filtered comparisons, with Linear as the default.
- Normal/slope-aware receiver bias plus polygon offset reduce self-shadow acne.
- Mirrored GLB node winding is respected in the shadow pass, preserving the VA02D convention.
- Skinned GLB meshes use the same joint palette in the shadow pass.
- Authored collision/spawn nodes and active mesh-node prefix filters are respected.
- Static non-skinned draw ranges are frustum-culled against each light cascade before shadow submission.
- SHADOW02 renders the four cascades as one layered depth-array pass. A geometry shader fans each accepted source triangle only into the cascade layers selected by a per-draw bit mask, so the CPU no longer resubmits the same mesh range up to four times.
- Material-only mesh splits that are contiguous and share the same node transform/skin are coalesced into one shadow indexed draw because the depth-only pass does not need visible material state. Hidden/filtered authoring ranges still break a batch.
- Sun shadows automatically become inactive when EnvironmentSystem sun intensity is effectively zero.

## Performance diagnostics

The F8 overlay reports:

- sun shadow active/off state;
- cascade count, resolution and active Nearest/Poisson-PCF/PCSS+Poisson filter mode;
- shadow-pass CPU wall time;
- shadow draw-call count;
- shadow triangle count;
- shadow ranges culled by cascade light frusta.

GPU shadow cost is already included in the asynchronous whole-frame GPU timer.

## Current limitations / next steps

This first shadow pass treats submitted geometry as opaque depth casters. It intentionally does not yet reproduce glTF alphaMode/alphaCutoff inside the shadow pass. When material render correctness is upgraded, masked foliage/fences should use alpha-tested shadow casting, while BLEND materials such as glass should use an explicit shadow policy.

Future shadow work can add:

1. cascade transition blending;
2. stronger texel-grid stabilization if authored-world motion exposes shimmer;
3. extend the now-live shadow presets beyond resolution/filtering to cascade distance/PCF policy if profiling and visual tests justify it;
4. cached/static scene shadow layers combined with dynamic vehicle shadows;
5. alpha-tested octahedral vegetation-cluster shadows;
6. headlights/spot-light shadows and optional moonlight;
7. full GPU-built indirect shadow command lists for very large grids/vegetation after profiling the SHADOW02 layered/coalesced path;
8. a unified HeritageEditor lighting/shadow debug view.

## Design rule

The 100 km visual horizon does not imply 100 km dynamic shadows. Shadow detail is concentrated around the active camera/vehicle where it is perceptually useful. Distant landscape and vegetation should later use HLOD, baked/probe lighting, or other large-scale approximations rather than extending high-resolution real-time shadow maps to the horizon.


## SHADOW04 filtering

The depth array is shared between two OpenGL sampler objects: a raw nearest sampler for diagnostic Nearest mode and PCSS blocker search, and a GL_LINEAR comparison sampler for Poisson PCF/PCSS final filtering. The three user-facing filtering modes are Nearest, Poisson PCF, and PCSS + Poisson. PCSS uses a stable Poisson blocker search and a clamped, blocker-separation-driven penumbra before the final Poisson comparison filter. No temporal randomization is used, so the filter does not intentionally shimmer frame-to-frame.
