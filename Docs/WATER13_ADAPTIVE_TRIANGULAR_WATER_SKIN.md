# WATER13 – Adaptive Triangular Water Skin

WATER13 removes the fixed 0.10/0.50/1.0/2.0 metre presentation lattices introduced for the WATER12A experiment. Settled water remains a real indexed 3D triangle surface, but tessellation is now driven by local representation error rather than distance alone.

## Adaptive range

- Minimum allowed triangle edge: 0.001 m
- Maximum allowed triangle edge: 20.0 m
- Flat/uniform water may remain near the 20 m macro scale.
- Shorelines, curb/support transitions, water-surface curvature and scalar-field error selectively split shared edges.
- Shared edge marks force both adjacent triangles to split the same edge, preventing cracks/T-junction surface gaps.
- Selective one/two/three-edge splits create mixed triangle sizes, aspect ratios and orientations rather than a repeated chessboard or single triangular lattice.

## Performance policy

The mesher begins from sparse 20 m wet-region macro tiles. It only refines where error requires it and uses hard per-ring triangle budgets. Source hydrology stays full-resolution to 100 m; 100–500 m uses adaptive source collection before presentation tessellation. This avoids paying full 0.5 m source and 0.1 m presentation density over hundreds of metres.

## Depth stability

The 3D skin now keeps at least 3 mm geometric clearance from the authored support surface and uses a small reversed-Z polygon offset during the water draw. This is presentation-only and does not alter authoritative water depth or tire physics.
