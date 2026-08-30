# CLOUDURP15EC — High-Quality Raymarch Reconstruction

## Why this milestone exists

CLOUDURP15EA/EB were diagnostic attempts to suppress visible cloud grain by making the spatial and temporal denoisers extremely strong. User captures showed that this failed in the important way: fine stochastic grain remained visible while the large cloud body was flattened into broad, cartoon-like slabs.

CLOUDURP15EC moves quality back to the source instead of hiding an undersampled image.

## Changes

- Primary cloud integration increases from 32 to 48 samples.
- Native cloud raymarch resolution increases from 0.80x to 0.90x linear resolution.
- First-step stochastic jitter is narrowed around the center of the interval rather than spanning the full 0..1 range.
- Pixel-footprint 3D noise mip selection remains, but is capped and weakened. The previous EB implementation folded the full longitudinal ray step into the footprint, selecting coarse mips that erased natural density structure.
- Micro erosion is broader and lower amplitude (0.40 strength, 140 scale instead of 0.65 / 300) so it reads as cloud structure rather than sand-grain texture.
- Current-frame cleanup is reduced from an almost-total 11x11 blend to a small 5x5 edge-aware reconstruction.
- Temporal accumulation is reduced to 0.965 coherent / 0.985 mild-grain / 0.995 strong-grain, with transmittance capped at 0.985 history.

## Invariants retained

- One HDRP/Unity-URP-derived cloud raymarch authority.
- One point-sampled, current-frame-AABB-clamped temporal history path.
- Cloud RGB and coverage/transmittance share the same temporal authority.
- Physical Sun/Moon lighting, PBSKY transmission, cloud depth, ground cloud shadow receiver and regional weather ownership are unchanged.
- No second blur/TAA pipeline was added.

## Expected result

Compared with CLOUDURP15EA/EB, silhouettes and small cloud bodies should contain fewer isolated spray-paint speckles while retaining substantially more natural volumetric structure. The image should be less smeared and less cartoon-like because more of the final result comes from real ray samples instead of long-lived history and a wide Gaussian.

The raymarch is intentionally more expensive than the earlier 0.80x / 32-step path. This is a quality-first production correction; profiling can later determine whether adaptive sampling can recover part of the cost without reintroducing the artifact.
