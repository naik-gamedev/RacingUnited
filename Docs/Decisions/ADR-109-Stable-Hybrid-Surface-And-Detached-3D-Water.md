# ADR-109 — Stable Hybrid Surface And Detached 3D Water

## Status
Accepted by WATER11. Supersedes the WATER10 assumption that settled road water should be visually reconstructed from parcel coverage.

## Context
Live WATER10-D/E testing showed temporal holes, camera-sensitive coverage and complete disappearance even while authoritative hydrology still contained water. The failure was architectural: millimetre-deep settled water was allowed to exist visually only when enough transient GPU parcel splats survived compaction and screen-space reconstruction.

## Decision
Authoritative hydrology depth owns the existence and primary presentation of all surface-attached water. The renderer samples that field continuously and shades it directly on `SurfaceWetnessReceiver` pixels. Settled parcels may exist as hidden state, but they are never allowed to determine whether a puddle/film exists on screen.

True 3D representations are reserved for water that actually departs from the shallow free surface: tire spray, splashes, breaking sheets and similar detached detail.

## Consequences
- No surface-water flicker caused by parcel spawn/kill/compaction.
- No z-fighting surface mesh: primary water is a depth/stencil reconstructed receiver pass.
- The old explicit square puddle mesh remains retired.
- WATER09 virtual-pipe hydrology remains authoritative for water mass and tire interaction.
- Future restricted tall-cell / PBF detail can be added locally without replacing the stable broad water body.
