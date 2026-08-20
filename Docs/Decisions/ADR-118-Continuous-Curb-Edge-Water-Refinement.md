# ADR-118 — Continuous curb-edge water refinement

## Status
Accepted — WATER14H, 2026-08-17.

## Decision
Treat a sharp support-to-support height discontinuity as genuine fine hydrology geometry even when both adjacent surface normals are nearly parallel. Detect the discontinuity from the residual between the actual neighbour elevation delta and the delta predicted by the neighbours' average tangent plane.

Only N/E/S/W support neighbours participate in this height-break test because they share a real support edge. Diagonal neighbours do not create a feature by corner contact alone.

## Why
A sidewalk curb is commonly represented by two horizontal top surfaces separated vertically. The vertical curb face is not a suitable hydrology receiver, so normal-angle tests alone cannot trace the curb. Height-residual detection follows the full edge while still allowing uniformly sloped roads to coarsen.

## Consequences
The 0.10 m tier forms continuous local strips along curb/step boundaries. The existing distance-field grading limits the fine topology to the immediate feature area, and planar parking lots and roads recover large cells farther away.
