# ADR-115: Shared-Boundary Adaptive Water Stitching

## Status
Accepted

## Decision
Render WATER14 adaptive hydrology cells as one stitched shared-boundary surface per cadence ring rather than isolated four-vertex cell plates.

Neighbour discovery uses a maximum 0.20 m search radius, but vertex merging is topology-aware. Only compatible adjacent boundaries on the same vertical presentation layer are stitched. Coarse edges are subdivided at fine-neighbour boundary vertices, and the shared water-surface state is averaged at canonical seam vertices.

## Rationale
A blind 20 cm proximity weld would destroy valid 10 cm simulation cells. Topology-aware seam discovery provides the requested visual merging while preserving adaptive simulation detail and removing coarse/fine T-junction cracks.
