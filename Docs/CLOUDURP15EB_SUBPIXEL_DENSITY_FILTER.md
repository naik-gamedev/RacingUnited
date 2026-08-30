# CLOUDURP15EB — Sub-pixel volumetric density and coverage filtering

The remaining cloud "spray-paint" / salt-and-pepper silhouette was not solved by extreme RGB temporal accumulation because two independent aliasing paths remained.

1. The raymarch sampled mip-0 erosion and micro-erosion detail even when one cloud pixel or one primary march interval represented many source-volume voxels. Small/distant cloud bodies therefore became binary high-frequency hit/miss patterns.
2. The temporal resolve accumulated cloud RGB but wrote current-frame transmittance directly every frame. The cloud silhouette/coverage could therefore remain noisy even while color was nearly frozen by 0.9995–0.99995 history.

CLOUDURP15EB keeps the existing single cloud pipeline and fixes both at source:

- derives a world-space ray-cone footprint from `dFdx/dFdy(reconstructRay())` before raymarch divergence;
- includes a fraction of the primary march interval in the represented volume footprint;
- selects mipmapped 128^3 shape, 32^3 erosion, and 32^3 micro-erosion density using their actual world-space mip-0 voxel widths;
- keeps distance erosion LOD as a lower bound, never sharpening detail that earlier milestones intentionally filtered;
- makes the existing 11x11 Gaussian current-frame cleanup transmittance-range aware so dense cloud is not dragged into clear sky as a fuzzy halo;
- point-samples the same single temporal history and clamps previous RGB and transmittance to the current five-pixel support;
- accumulates clamped transmittance with the same motion-aware temporal authority, capped at 0.9995 for coverage so silhouettes converge without becoming permanently stuck.

No second TAA, second cloud field, extra weather authority, or additional cloud texture is introduced. CLOUDURP15EA's intentionally extreme color denoising remains active.
