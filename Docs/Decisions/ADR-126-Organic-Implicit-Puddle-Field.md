# ADR-126 — Organic Implicit Puddle Field

## Status
Accepted for WATER17.

## Context
Earlier water presentation paths repeatedly exposed square or stepped shapes even after moving water into textures.  The remaining cause was structural: rectangular adaptive/support footprints and hardware interpolation across invalid atlas samples were still capable of becoming the visible wet/dry boundary.

## Decision
Visible standing water is an implicit scalar field rather than a set of owned cells.

1. Adaptive simulation leaves never parameterize graphics.
2. Immutable collision-support samples generate overlapping compact Wendland C2 radial bases.
3. Stable world-space domain warping and the authored shoreline mask deform only the implicit presentation boundary.
4. Basin head discontinuities on continuous surfaces are regularized with a spill-gated graph-Laplacian solve inside a strict trust region.
5. Atlas reconstruction explicitly rejects invalid/vertically unrelated samples before interpolation.
6. Exact authored fragment height remains the final geometric test for local water depth.
7. Curbs and stacked surfaces remain hard vertical/topological barriers.

## Consequences
- No visible puddle boundary is allowed to be the edge of an adaptive solver cell or rectangular atlas tile.
- Nearby wet regions join through overlapping implicit support and hydraulic saddle connectivity rather than explicit shape merging.
- The shoreline texture is used as micro-topography/contour deformation, not as authoritative water mass.
- The compact 512/256/128 cache and one-refresh-per-frame budget are retained.
