# OPT05 — Volumetric Cloud Bandwidth / Pass Fusion

OPT05 optimizes the existing CLOUDURP15 volumetric-cloud pipeline without
reducing the authored raymarch resolution, 32-step integration budget, cloud
lighting, cloud-shadow resolution/filtering, weather authority, or temporal
accumulation strength.

The optimization removes redundant full-screen memory traffic around the cloud
model rather than simplifying the cloud model itself.

## Temporal history ping-pong

Before OPT05, every cloud frame rendered the full-resolution temporal result to
`m_cloudTemporalTexture` and then copied that complete RGBA16F image into
`m_cloudHistoryTexture` with `glCopyImageSubData`.

OPT05 gives both temporal textures their own framebuffer and swaps texture/FBO
ownership after presentation. The just-completed temporal result becomes next
frame's history without any image copy.

The old history texture used nearest filtering while the temporal/present
texture used linear filtering. Because the two textures now alternate roles,
OPT05 preserves that behavior with a dedicated nearest sampler bound only while
history is sampled; both texture objects can therefore safely ping-pong.

At 1920x1080, one RGBA16F frame is about 15.82 MiB. Removing the copy eliminates
that image-copy payload every cloud frame (roughly 31.64 MiB of underlying
read+write traffic before driver/cache effects).

## Default upscale fused into TAA

The default configuration has `bilateralUpscale == false`. In that mode the old
combine pass did only this operation at every full-resolution pixel:

`texture(uCloudTexture, uv)`

It wrote that linearly filtered low-resolution sample into a full-resolution
RGBA16F intermediate, after which the temporal pass immediately read it again.

OPT05 lets the temporal shader sample the low-resolution raymarch texture at the
same full-resolution pixel-center UVs. The explicit full-screen combine pass and
its full-resolution RGBA16F write are skipped in the normal path.

The authored bilateral 7x7 upscale remains available. When
`bilateralUpscale == true`, the existing combine shader and full-resolution
intermediate are still used.

## Scene-colour staging

Cloud raymarching uses the already-rendered scene colour only for distant
horizon matching. The source scene render target is RGB8, while the old staging
texture was RGBA16F.

OPT05 stages scene colour as RGBA8, which preserves all precision available in
the RGB8 source while halving bytes per staged texel versus RGBA16F.

For a single-sample target, scene colour is also copied directly to the 4/5
raymarch resolution because no later full-resolution cloud pass needs that
staging texture anymore. At 1920x1080 this changes scene-colour staging from
about 15.82 MiB to about 5.06 MiB, a 68% reduction in intermediate storage/write
payload.

A multisampled source deliberately keeps a full-size resolve. OpenGL does not
permit resolving and scaling an MSAA framebuffer in one blit, so OPT05 avoids an
illegal optimization there. The MSAA resolve uses `GL_NEAREST`.

## Direct final composition

Cloud RGB is already premultiplied scattering and cloud A is transmittance. The
old final shader sampled the copied scene texture and evaluated:

`cloud.rgb + scene.rgb * cloud.a`

OPT05 leaves the scene in the destination framebuffer and performs the same RGB
equation with fixed-function blending:

- source RGB factor: `GL_ONE`
- destination RGB factor: `GL_SRC_ALPHA`
- blend equation: `GL_FUNC_ADD`

The Heritage scene framebuffer is RGB-only, so alpha-channel preservation is
not part of this handoff. This removes one full-resolution scene-texture fetch
from every cloud presentation pixel.

## Preserved cloud behavior

The following source blocks are unchanged from OPT04C1:

- spherical Earth/cloud-shell intersection;
- 32-step primary cloud integration;
- density/noise formation math;
- Sun/Moon/ambient cloud lighting;
- powder/multiple-scattering approximations;
- regional weather/wind advection;
- cloud shadow raymarch;
- cloud shadow filter;
- temporal accumulation strength and reactive rejection thresholds;
- cloud raymarch resolution (4/5 viewport dimensions);
- optional bilateral upscale algorithm;
- optional cloud-depth output/merge semantics.

OPT04C1 shadow/frame preparation, OPT04B renderer-state cleanup, OPT04A renderer
ownership split, OPT00 asynchronous GPU profiling, OPT03C4 single GPU-water
authority, OPT03B tire-water sampling, and OPT02 `.hhyd` v15 remain intact.
