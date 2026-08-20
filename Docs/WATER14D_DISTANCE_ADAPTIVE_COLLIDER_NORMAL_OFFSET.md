# WATER14D — Distance-Adaptive Collider-Normal Water Offset

WATER14D keeps WATER14C's collider-normal water separation, but makes the visible offset increase smoothly with camera distance instead of remaining fixed at 3 cm everywhere.

## Presentation curve

The already-visible explicit-water mesh uses these absolute collider-normal offsets:

- 0–25 m: 0.03 m
- 50 m: 0.06 m
- 100 m: 0.09 m
- 200 m: 0.13 m
- 500 m and beyond: 0.20 m

Between those anchors the vertex shader uses `smoothstep`, so there are no hard geometric jumps at the cadence-ring boundaries. The derivative also falls to zero at each anchor, avoiding a visible kink in the transition.

## Ring compatibility

The offset is evaluated from the vertex's radial camera distance, not from a discrete ring index. This is intentional: during the existing ring overlap/crossfade, two caches rendering the same world-space water point receive the same requested offset. The LOD handoff therefore does not create two differently elevated water sheets.

WATER14C's 0.03 m near-field offset remains baked into the stitched mesh. WATER14D adds only the extra amount needed to reach the distance-dependent absolute target in the vertex shader.

## Direction

All displacement continues to follow the normalized collider-derived surface normal carried by each stitched water vertex. Shared seam vertices use WATER14C's normalized average of contributing collider normals.

## Explicitly unchanged

- No water visibility/depth threshold is changed.
- No hydrology depth, volume, transport, rainfall input or adaptive-cell sizing is changed.
- Tire-water physics stays on the authoritative collider-derived hydrology surface.
- WATER14B's 0.20 m topology-aware neighbour search and shared-boundary stitching are unchanged.
- WATER14A's aggressive-angle 0.10 m minimum-cell gate is unchanged.
- Reversed-Z and the existing polygon depth bias remain enabled.
