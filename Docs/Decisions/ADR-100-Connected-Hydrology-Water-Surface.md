# ADR-100 — Connected Hydrology Water Surface

## Decision

Heritage Engine will keep the authoritative hydrology simulation cell-based, but final explicit water presentation must not be cell-card-based. Adaptive hydrology presentation leaves are now treated as reconstruction inputs for a connected indexed surface.

## Rationale

Independent transparent quads exposed card edges, overlapping-alpha seams, mismatched contour supports and the characteristic "dragon-scale" appearance on sloped/curved terrain. More tessellation of independent cards does not solve disconnected topology.

A connected mesh allows adjacent triangles to share the exact same boundary position and continuously interpolated physical state. It also preserves adaptive source resolution: large planar areas can still be represented from 8/16 m leaves while complicated terrain falls back to finer leaves.

## Rules

- Hydrology cells remain authoritative for water mass, depth, flow and tire interaction.
- Presentation may interpolate and simplify but may not create/delete authoritative water.
- Shared X/Z vertices are only stitched within the same hydrology vertical layer.
- Adaptive T-junctions must be split into a conforming edge before drawing.
- Shared vertices interpolate ground support, water depth, flow and normal.
- Fine source leaves dominate coarse leaves at mixed-resolution seams.
- Explicit free-sheet geometry remains suppressed on >55 degree surfaces; hydrology continues there.
- Procedural material detail remains world-anchored and independent of mesh topology.

## Consequences

The water path now uses indexed vertex/index buffers rather than instanced four-vertex cards. CPU reconstruction cost is paid only when a cadence ring refreshes, not every rendered frame. F8 exposes this cost separately so later optimization can move reconstruction to jobs/GPU compute if profiling warrants it.
