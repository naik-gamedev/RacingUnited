# WATER14J — Balanced Feature Quadtree

> **Historical presentation experiment:** WATER15 retires WATER14J renderer-owned water topology. The adaptive hydrology hierarchy may remain as simulation authority, but no curb-line strip, seam stitcher, or water mesh is rendered. See `Docs/WATER15_DYNAMIC_TRACK_SURFACE_STATE.md`.

WATER14J turns the WATER14I curb-line refinement into a restricted, feature-driven quadtree-style hierarchy.

## Goal

Spend minimum detail only on genuine geometric features such as curb/sidewalk height breaks, then grow cell size aggressively but cleanly away from the feature. A flat road or parking lot must not inherit a carpet of tiny cells merely because a curb exists nearby.

## Hierarchy

The immutable terrain support raster remains 0.50 m. A detected curb/step keeps the WATER14I presentation-only ~0.10 m triangle strip on the exact marked edge. The authoritative solver cell beside that feature remains 0.50 m.

Away from the feature, permitted solver size grows in widening bands:

- feature support: 0.50 m
- 0.5–1 m away: up to 1 m
- 1–2 m away: up to 2 m
- 2–4 m away: up to 4 m
- 4–8 m away: up to 8 m
- 8–16 m away: up to 16 m
- beyond 16 m: up to the configured 20 m cap

The distance field uses four-connected support propagation so diagonal shortcuts do not collapse refinement bands around corners.

## 2:1 balancing

Unaligned greedy packing can leave an orphan fine leaf directly against a much larger patch even when the distance policy is correct. WATER14J therefore performs a post-pack shared-face balancing pass over the 0.50 m+ solver mesh. Any cell physically touching a neighbour more than 2x smaller is locally subdivided. The pass is repeated until no further shared-face violations remain or the bounded pass count is reached.

The 20 m top cap is not a power-of-two quadtree leaf. If it violates 2:1, it drops locally to the 4 m hierarchy tier rather than introducing 10/5/2.5 m levels that cannot descend cleanly to the 0.50 m support lattice.

True 0.10 m authoritative cells reserved for aggressive angular terrain are below the support lattice and are not expanded into a broad 2:1 solver halo. Their presentation seams remain handled by the adaptive stitcher.

## Preserved behaviour

WATER14J does not change rain accumulation, water volume conservation, virtual-pipe transport, explicit-water thresholds, shoreline breakup texture, 40 cm seam discovery, or the 3 cm-to-20 cm collider-normal presentation offset.
