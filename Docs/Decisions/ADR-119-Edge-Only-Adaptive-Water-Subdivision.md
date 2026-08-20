# ADR-119 — Edge-only adaptive water subdivision

Status: Accepted  
Date: 2026-08-17

## Decision

Treat sharp curb/sidewalk height discontinuities as directional feature edges rather than as a request to refine an entire 0.50 m support cell to 0.10 m.

The authoritative solver remains at 0.50 m immediately beside a curb unless independent angular/curvature criteria require finer simulation. Presentation spends ~0.10 m detail only in one strip along the detected boundary and then transitions immediately to coarser geometry. Solver coarsening grows exponentially away from the feature (0.50, 1, 2, 4, 8, 16, then up to 20 m) while retaining existing plane-fit and material constraints.

A marked height discontinuity is also a hard topology boundary for adaptive packing and must not be swallowed by a coarse candidate or welded to the opposite elevation by presentation seam repair.

## Rationale

The WATER14H whole-support refinement solved continuity along curbs but created unnecessary 5x5 clusters of 0.10 m cells. This decision concentrates detail on the geometric line that needs it and preserves large planar cells immediately away from that line.
