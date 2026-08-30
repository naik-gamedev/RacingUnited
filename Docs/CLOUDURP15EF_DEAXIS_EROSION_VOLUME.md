# CLOUDURP15EF — De-axis Erosion Volume

The persistent horizontal "slices" survived CLOUDURP15EE's four occupied microsteps, which proves the dominant artifact is not the primary raymarch interval spacing. The remaining bands align with the world axes of the 32x32x32 erosion volume.

CLOUDURP15EF samples that periodic erosion volume through two fixed orthonormal coordinate frames and blends them. The same de-aliased field is used by visible cloud density and the cloud-shadow density path. This removes coherent horizontal voxel planes without introducing a second cloud simulation or replacing the upstream volume assets.

Micro-erosion amplitude is reduced modestly from 0.40 to 0.34 now that the low-resolution volume is being sampled isotropically. CLOUDURP15EE's full-resolution 64-step raymarch, four occupied substeps, 7x7 transmittance-aware reconstruction, RGB+coverage history and current-frame AABB anti-ghosting clamp remain authoritative.
