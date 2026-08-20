# ADR-116 - Planarity-Driven Adaptive Hydrology Coarsening

## Status
Accepted for WATER14F.

## Decision
Use a best-fit support plane and maximum elevation residual as the primary large-cell eligibility test for WATER14 adaptive hydrology. Compare support normals against the fitted plane with a modest tolerance, and allow greedy coarse patches to start at any unused support-cell origin rather than only at globally span-aligned coordinates.

## Rationale
Absolute slope does not imply geometric complexity: a long downhill road can be represented by one plane. Likewise, a curb should invalidate only candidates that cross the curb, not force unrelated asphalt beside it to inherit a fine world-grid partition. Plane-fit error measures the actual approximation error that matters to water support, while unaligned packing localizes refinement around real features.

## Consequences
Broad parking lots, ramps and planar road sections can use much larger authoritative control volumes. Curbs, steps, creases and true curvature remain local refinement boundaries. The existing aggressive-angle 0.10 m gate, material boundaries, conserved volume and unequal-cell virtual pipes remain unchanged.
