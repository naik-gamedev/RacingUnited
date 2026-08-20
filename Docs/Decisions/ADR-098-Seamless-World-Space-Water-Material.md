# ADR-098 - Seamless World-Space Water Material

**Status:** Accepted for PERF13.

## Context

PERF12 made water presentation adaptive across the full 0-200 m rendered range. However, an adaptive mesh is only useful if the material does not reveal the patch partition. The previous vertex path expanded a tangent-space square by 1% (`0.505 * patchSize`) and the procedural shader used an offset that was local to that tangent square. On slopes, the projected X/Z footprint no longer matched the axis-aligned hydrology block, and transparent oversize caused neighbouring patches to overlap. Both can reveal LOD/card boundaries.

The existing water material also made very thin authoritative rain film too easy to miss in live scenes.

## Decision

1. Construct each water patch in the same world-X/Z footprint used by the adaptive hydrology collector. Solve the corner Y offset analytically from the fitted support-plane normal.
2. Use exactly half the patch size for each X/Z extent. Do not hide cracks by overlapping alpha-blended water cards.
3. Generate procedural material coordinates from bounded global X/Z phase plus the true X/Z vertex offset. Ripple, flow and roughness fields are therefore continuous across patch/LOD boundaries.
4. Treat thin rain film as a neutral dark/glossy transparent overlay and standing water as progressively smoother, more reflective water. Saturate the thin-film response quickly enough that small cell-depth differences do not expose the hydrology grid.
5. Keep rain ripple animation procedural and world anchored. Do not require texture assets for the recovery milestone.
6. Preserve PERF12 adaptive caching, hydrology authority/cadence and the 200 m explicit-water limit.

## Consequences

- Planar slopes render as planar sloped water rather than horizontal or footprint-distorted cards.
- Equal neighbouring adaptive patches no longer accumulate alpha at an intentional overlap seam.
- A 16 m patch and four 8 m patches sample the same optical field at the same world pixel.
- Thin film becomes visible earlier without painting an opaque blue surface over the road.
- Connected puddle free-surface reconstruction remains future work; PERF13 improves optical behaviour but does not invent new water mass or alter hydrology.
