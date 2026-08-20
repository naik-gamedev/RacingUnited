# WATER14B — Shared-Boundary Adaptive Water Stitching

WATER14B keeps WATER14/WATER14A's authoritative adaptive hydrology cells, but changes how those differently sized cells become visible water geometry.

## Problem

Each adaptive control volume previously emitted its own four surface vertices and two triangles. Even though the simulation cells were adjacent, their duplicated boundary vertices could have different water heights, and a large cell next to several small cells created T-junctions. The result could look like detached chessboard plates.

## Solution

The renderer now builds one shared-boundary mesh per water cadence ring through `WaterSurfaceStitcher.hpp`.

- A **0.20 m neighbour-search radius** finds compatible adaptive-cell boundaries.
- The radius is discovery-only. It never blindly merges all vertices within 20 cm, because legitimate WATER14 cells can be only 0.10 m wide.
- Compatible boundaries on the same presentation layer share canonical vertices.
- Coarse edges are split at finer-neighbour corner positions, eliminating visible T-junction gaps.
- Shared boundary height, water depth and flow are averaged so adjacent control volumes meet at one surface position.
- A maximum 0.20 m water-surface mismatch prevents unrelated/vertically separated surfaces from being stitched together.
- The interior of each adaptive control volume is triangulated as a fan against its now-variable boundary polygon, so triangle shapes naturally vary around coarse/fine transitions.

This is presentation topology only. Water mass remains authoritative in WATER14's variable-area control volumes and unequal-face virtual pipes.
