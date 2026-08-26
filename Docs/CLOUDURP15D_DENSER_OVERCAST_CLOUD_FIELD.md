# CLOUDURP15D — True authored overcast coverage

## User-observed problem

With Scene → Weather → Cloud Coverage at 100%, the translated volumetric cloud bodies looked good but large clear-sky gaps remained. The cause was semantic: the volumetric renderer received the camera-local `RegionalWeatherSample.cloudCover`, and the regional FBM intentionally remains spatially broken even when the scene-authored cloud-cover control is 1.0.

## Fix

CLOUDURP15D separates two quantities that were previously conflated:

- `cloudCover`: camera-local regional coverage, still used by atmospheric colour/lighting.
- `authoredCloudCover`: the actual scene 0..1 Cloud Coverage control, used by the volumetric cloud authoring transfer.

The cloud-map path keeps the existing regional variation through ordinary settings. Between authored 0.82 and 1.0, a smooth overcast transfer progressively fills regional clear holes. At authored 1.0, regional cloud coverage is therefore 1.0 everywhere sampled by the cloud renderer.

To make 100% cover visually reach an 8/8 overcast sky instead of merely setting every weather-map texel to coverage=1, the same high-end transfer expands the low-frequency Worley **formation threshold** up to an effective 2.35x coverage. It does **not** multiply the final extinction/density by 1.65. This grows and joins the existing sculpted cloud bodies while retaining erosion, lighting and internal volume detail instead of producing a flat grey slab. The cloud-shadow density path mirrors the same transfer.

## Dithering note

The cloud renderer already has its own temporal reprojection/accumulation (0.95 history), so full-scene TAA is not required for the clouds to function. Remaining fine grain can come from the upstream-style stochastic 32-step raymarch at half resolution plus bilinear upscale. It can be addressed separately with improved jitter/blue-noise sequencing or the optional bilateral upscale without changing cloud coverage semantics.

## Scope

No rain, radar, hydrology, tire, or regional precipitation physics are changed. The regional weather texture itself is untouched; only the volumetric cloud interpretation of its coverage channel changes at the extreme high end of the authored coverage slider.
