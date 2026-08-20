# PERF17 – Shoreline Breakup Mask

Applies the user-provided grayscale breakup texture to shallow pooled-water edges so forming puddles stop exposing geometric triangle/quad boundaries.

## Changes
- Embedded the supplied grayscale breakup mask directly in `SurfacePresentationRenderer.cpp` through `Texture2DCache::acquireEmbedded`, so no external asset path is required.
- Added water-shader uniforms for shoreline breakup mask presence, scale, breakup strength, and fade softness.
- Samples the breakup mask in continuous world space using the reconstructed water surface coordinates.
- Restricts breakup mainly to shallow water (`~0.75–4.25 mm`) so deep puddle centers remain solid.
- Adds a light stable dither clip in the shallow-edge zone to avoid mushy transparent fringes.
- Multiplies the existing water alpha by the resulting shoreline coverage factor.

## Intent
The current connected-water reconstruction already gives much better pooling behavior. PERF17 specifically targets the remaining issue where puddle edges still reveal the underlying mesh topology while water is beginning to form.
