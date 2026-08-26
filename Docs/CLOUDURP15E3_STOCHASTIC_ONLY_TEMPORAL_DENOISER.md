# CLOUDURP15E3 — Stochastic-only cloud temporal denoiser

CLOUDURP15E3 changes Heritage cloud temporal filtering from a uniform TAA blend
into a selective denoiser for the visibly dithered / undersampled part of the
volumetric raymarch.

## Why

The visible cloud body already contains useful native density, erosion and
lighting detail. Applying the same temporal history weight everywhere softens
that structure and can make the volume look painted or cartoon-like. The
problematic pixels are instead the high-frequency salt-and-pepper samples
created where the 32-step stochastic raymarch is undersampled, especially thin
or strongly lit cloud regions.

## Selective temporal confidence

The full-resolution temporal pass now examines the current raymarch sample and
its four native raymarch neighbours. It measures:

- opacity and luminance spatial outlier strength;
- local opacity/luminance variance;
- centre-pixel frame-to-frame flicker after reprojection; and
- whether that flicker is isolated or shared by the surrounding cloud structure.

Only a pixel that is both spatially noisy and temporally unstable receives a
non-zero denoising mask. Clean and stable cloud pixels receive **0% history**.
Detected stochastic noise ramps smoothly from a light temporal contribution to
a maximum of **50% history** for the worst undersampling.

The previous early-out for fully clear current pixels is removed. This is
important because stochastic raymarch holes can temporarily resolve to clear
inside a thin cloud edge; those holes can now converge with valid reprojected
history rather than flashing as white/black speckles.

## Motion / edge protection

Reactive rejection no longer treats every single-pixel opacity change as cloud
motion. It uses coherent five-sample neighbourhood opacity and luminance change.
A genuinely moving, appearing or disappearing cloud silhouette therefore still
rejects stale history, while isolated stochastic flicker is allowed to converge.

## Unchanged systems

- The volumetric density/raymarch/light model is unchanged.
- The one regional weather authority is unchanged.
- The dedicated CELESTIAL04 Sun/Moon ground-shadow receiver is unchanged.
- The 256×256 cloud optical-depth shadow cookie is unchanged.
- No full-scene TAA is introduced.
