# ADR-112: Error-Driven Adaptive Water Tessellation

## Status
Accepted

## Context
Fixed water triangle spacing solved the visible square-cell problem but WATER12A oversampled large regions and dropped to roughly 1.6 FPS in the user's live scene. Fixed distance bands also waste triangles on flat water while still lacking enough detail at difficult local boundaries.

## Decision
Use a conforming adaptive triangular surface with an allowed edge range of 0.001–20 m. Begin from 20 m macro triangles around wet regions, then split shared edges only when shoreline classification, support topology, scalar interpolation error or water-surface interpolation error requires more detail. Use conservative per-ring triangle budgets and retain the existing hydrology cadence system.

## Consequences
Flat water can be represented by very large triangles while complex boundaries become progressively finer. Adjacent triangles may have different shapes and sizes, but shared-edge split propagation keeps topology continuous. The 1 mm floor is a capability/safety bound, not a target density.
