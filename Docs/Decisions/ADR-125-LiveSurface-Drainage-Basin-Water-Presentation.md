# ADR-125 — LiveSurface drainage-basin water presentation

## Status
Accepted — 2026-08-17

## Context

WATER14 exposed adaptive water topology directly. WATER15 moved water into material textures, but multiple attempts still rasterized solver-cell ownership (rectangles, filtered rectangles, or circular splats). Live testing repeatedly showed the discretization through the final wet surface.

Project CARS 2 / Madness LiveTrack publicly separates dense dynamic track state from stochastic wet/puddle appearance. Heritage should follow that architectural principle without claiming access to proprietary implementation details.

## Decision

Keep `SurfaceHydrology` as the sole conserved water authority, but build immutable presentation drainage catchments from the 0.50 m collision-support raster. Reduce only excess standing-water volume into one free-surface elevation per catchment. Render that basin state through the normal authored surface material and use the existing shoreline texture as deterministic sub-cell stochastic relief.

Catchment connectivity is based on unexplained height discontinuity rather than absolute slope. Therefore a sloped road remains continuous while a curb/step is a hard presentation drainage boundary.

Do not render adaptive control-volume footprints, circular splats, a second water mesh, or a full-screen water geometry pass. Do not blur basin state across curbs with a compute filter.

## Consequences

- Puddle shape is determined by basin free-surface height intersecting exact scene geometry plus stochastic micro-relief, not simulation-cell topology.
- Separate nearby puddle islands within one catchment merge naturally as basin level rises.
- Raised sidewalks remain dry until their own basin has water or authoritative flow reaches them.
- Presentation reduction runs at a lower cadence than the water solver and does not move authoritative water mass.
- Future drain/saddle/overtopping metadata can extend the same basin graph without changing the visible-material ownership model.
