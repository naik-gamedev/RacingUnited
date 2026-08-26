# VCLOUD01 — HDRP-derived volumetric clouds on the PBSKY atmosphere

## Purpose

VCLOUD01 replaces the long-lived Heritage-specific CLOUDURP15 artistic cloud marcher with a clean Heritage-native OpenGL/GLSL translation of the active rendering model in **jiaozi158/UnityVolumetricCloudsURP**, itself a Unity HDRP volumetric-cloud port.

Upstream: `https://github.com/jiaozi158/UnityVolumetricCloudsURP`

License/provenance is retained in `Docs/ThirdParty/UnityVolumetricCloudsURP_NOTICE.txt` (MIT, Copyright (c) 2024 jiaozi158).

The objective is not to emulate Unity/URP APIs. Unity renderer features, command buffers, render graph handles and light cookies remain replaced by Heritage framebuffer/program/state ownership. The cloud **density, ray integration, lighting and shadow model** are the parts translated.

## Upstream-derived cloud model restored

The live cloud raymarch now preserves the important source behavior rather than the later Heritage artistic tuning chain:

- 32 primary ray steps.
- Maximum step derived from cloud-shell altitude range / 8.
- Fine sampling starts immediately.
- After 8 sequential empty samples, the ray switches to doubled cheap steps.
- Cheap density discovery backs up one fine step when cloud material is rediscovered.
- Early transmittance termination at 0.003.
- 128^3 Worley shape noise and 32^3 erosion noise with mip chains.
- Source-style erosion-distance mip bias.
- Density threshold 0.001.
- Coverage-squared density semantics.
- Rain-dependent extinction sigma: 0.04 .. 0.12.
- Two light samples with a maximum 1000 m light step.
- Two Wrenninge-style multiple-scattering octaves.
- Dual Henyey-Greenstein lobes with +/-0.7 eccentricity.
- Powder effect intensity 0.25.
- Source shader-side multi-scattering factor 0.525.
- Source-style energy-conserving analytical integration.

## Four upstream preset families

Heritage now builds one 4x64 RGB16F curve LUT containing the upstream Sparse, Cloudy, Overcast and Stormy density/erosion/AO curve families. Density endpoints are forced to zero and AO is stored as `1 - AO`, matching the upstream setup convention.

The preset parameter families include the original shader-side values:

| Preset | Bottom / range | Density | Shape | Erosion |
| --- | --- | --- | --- | --- |
| Sparse | 3000 / 1000 m | 0.32 | 0.95 | 0.80 |
| Cloudy | 1200 / 2000 m | 0.32 | 0.90 | 0.80 |
| Overcast | 1500 / 2500 m | 0.18 | 0.50 | 0.50 |
| Stormy | 1000 / 5000 m | 0.245 | 0.85 | 0.75 |

All four use the source 5x shape scale. The Heritage regional weather map interpolates cloud type spatially instead of introducing a second weather simulation.

## Heritage regional-weather mapping

Regional weather remains the authority for radar, rain, hydrology and cloud placement:

- R: coverage
- G: physical precipitation intensity
- B: relative humidity
- A: storm intensity

VCLOUD01 derives the upstream cloud-type coordinate and rain-cloud extinction value from those existing channels. Cloud density therefore follows the same spatial weather field that already drives precipitation.

The broad spherical intersection shell spans 900..6500 m altitude only as a cheap bounding volume. Actual density uses the selected/interpolated preset bottom and altitude range, so the renderer no longer forces every weather regime into one common handcrafted slab.

## PBSKY01 pairing

The cloud renderer now samples the PBSKY01 physical transmittance LUT when evaluating direct sunlight at the mean cloud position. This couples cloud solar colour/extinction to the same Earth atmosphere used by the visible sky rather than applying the retired hand-authored horizon tint stack.

Ambient cloud lighting still uses Heritage's environment cubemap, blended toward the current-frame PBSKY horizon/zenith authority so rapid astronomical time changes do not wait on an asynchronous reflection update.

The existing Moon uniforms remain available for the planned Moon-cloud illumination pass, but VCLOUD01 intentionally does not yet add Moon-cast cloud shadows to the ground. That feature remains a separately testable follow-up.

## Cloud shadows

The cloud-shadow pass is restored to the upstream 16-segment trace (15 interior samples, `i=1..15`) instead of the later 8-sample Heritage performance variant. The source-style 3x3 Gaussian filter with sigma 0.9 remains. Heritage continues to multiply this transmission texture with its existing cascaded geometry shadows instead of replacing the directional-light shadow system with a Unity cookie.

The working shadow texture is restored to 256x256 and the receiver half-range remains 8 km.

## Existing Heritage presentation retained

The following already-proven Heritage infrastructure is intentionally retained around the restored cloud model:

- post-opaque local-cloud ordering;
- reversed-Z single-sample/MSAA opaque-depth intersection;
- optional cloud depth output / depth merge;
- OPT05 low-resolution pass fusion and scene-staging bandwidth reductions;
- full-resolution temporal ping-pong without full-frame history copies;
- optional bilateral upscale before temporal denoising;
- final resolved camera-colour presentation with destination alpha preserved;
- PERF05 cached shader link status (no per-frame `GL_LINK_STATUS` query);
- asynchronous GPU timing and PERF01-PERF04 CPU attribution.

The temporal denoiser is now a direct Heritage/OpenGL translation of the upstream Pass 3: full-resolution camera-colour history, a 5-pixel current-frame AABB clamp, point-clamped history sampling, local-scene-depth/global-far-plane reprojection, and the upstream default 0.95 accumulation reduced by camera velocity. All later Heritage adaptive 20/60/50:1 classifier variants are retired.

## Validation

VCLOUD01 validation checks the four-preset LUT ownership, 32-step/8-empty-step integration policy, two-light-step dual-HG multiple scattering, PBSKY transmittance coupling, 0.95 temporal accumulation, cloud depth, 16-segment cloud shadows, 256x256 shadow target and retained MIT notice/assets.
