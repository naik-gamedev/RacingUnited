# ADR-127: Dynamic Sponge-Brush Water Canvas

**Status:** Accepted for WATER18

## Decision

Visible puddle coverage is painted into engine-owned world-space clipmaps with deterministic,
rotated, overlapping sponge-brush dabs. `Water_ShorelineBreakup_A8` is used as brush alpha.
Hydrology cells provide physical state only and may never define rectangular visible coverage.

## Reason

WATER14–WATER17 repeatedly allowed simulation/support discretization to leak into the image as
squares, steps or repeated tiles. Increasing resolution, radial bases and procedural distortion
could reduce but not eliminate that ownership relationship. Texture-painting semantics remove it:
we know the world-space location of the physical water, but the brush chooses the mark shape.

## Constraints

* Brush pattern must be deterministic/world anchored.
* Brush growth may not create authoritative water mass.
* Exact receiver height and vertical-layer matching remain authoritative for curb/bridge isolation.
* Thin weather film remains a smooth material state, not discrete paint stamps.
* No visible water geometry is reintroduced.
