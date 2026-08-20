# ADR-133 — Continuous Scalar Dynamic Surface Water Presentation

## Status

Accepted for DSURF04D2. Supersedes ADR-132 for live water presentation. ADR-131 / DSURF04C physical authority remains unchanged.

## Context

DSURF04D attempted to hide the coarse 64 × 64 authority by reconstructing a coarse free-surface elevation and subtracting exact rendered receiver height. Live testing showed large bands and triangle-shaped artifacts on sloped terrain because coarse support height and exact mesh height are different representations of the surface. Their difference is not water depth.

## Decision

1. Hydro.R is the only source of visible standing-water depth after vertical sheet resolution.
2. Support-height data is used to choose the correct stacked surface sheet and is never subtracted from rendered triangle height to manufacture visible depth.
3. Hydro.R is linearly interpolated from the authoritative 64 × 64 mip0 texture.
4. A bounded world-space domain warp deforms sampling coordinates by less than half a texel near ordinary interiors to remove axis-aligned grid signatures.
5. Warp fades to zero at tile borders; the existing same-sheet neighbour blend owns 100 m continuity.
6. Millimetre optical micro-relief and derivative anti-aliasing shape only the shallow contour and fade out in deeper water.
7. No second simulation grid, visible water mesh, clipmap, canvas, splat field or CPU mip ladder is introduced.

## Consequences

- Render triangles cannot imprint their topology into water coverage through height subtraction.
- Physics remains cheap at 100 m / 64 × 64 / 2 Hz.
- The visual zero contour is continuous and world-stable but can differ slightly (sub-texel) from the coarse authority boundary, by design.
- If 64 × 64 remains visibly insufficient, escalation should add presentation resolution rather than increasing hydrology authority resolution.
