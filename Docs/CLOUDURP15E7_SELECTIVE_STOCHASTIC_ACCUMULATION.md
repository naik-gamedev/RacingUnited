# CLOUDURP15E7 — Selective stochastic accumulation

CLOUDURP15E7 keeps the CLOUDURP15E6 upstream UnityVolumetricCloudsURP/HDRP-derived temporal denoiser as the **only** cloud temporal system. It does not restore any retired Heritage adaptive TAA branch.

## Why this exists

CLOUDURP15E6 made temporal accumulation visibly effective, but sparse stochastic samples remained visible at partially covered cloud edges and thin illuminated regions. Two details were responsible for most of that residual grain:

1. A single current pixel with transmittance `1.0` bypassed temporal history even when its four neighbours contained cloud. A stochastic ray miss could therefore survive as a clear salt-and-pepper hole.
2. All cloud pixels used the same 0.95 accumulation even though sparse/high-frequency samples need a longer temporal integration window than coherent dense interiors.

## Authoritative path

The upstream path remains unchanged in structure: full-resolution current scene+cloud, 5-pixel current neighbourhood, point-clamped reprojected history, current-neighbourhood AABB clamp, camera-motion reduction, resolved history ping-pong.

CLOUDURP15E7 only modulates the accumulation factor inside that single resolve:

- coherent cloud structure: upstream `0.95`;
- detected mild stochastic grain: ramps toward `0.985`;
- detected severe stochastic grain: ramps toward `0.9975`;
- dense/opaque cloud interiors strongly suppress the extra accumulation so their internal lighting does not become waxy/cartoonish.

The stochastic signal is derived from high-frequency luminance and transmittance disagreement within the same center-plus-four-cardinal neighbourhood already used by the upstream clamp. There is no second history texture, no second temporal pass, no legacy clean/mild/strong classifier path, and no GL_LINEAR history override.

A center pixel that is clear no longer bypasses TAA by itself. History is bypassed only when the entire 5-pixel neighbourhood is clear, allowing stochastic edge holes to converge while empty sky remains untouched.

CELESTIAL04 cloud-shadow receiver behavior remains outside cloud temporal history.
