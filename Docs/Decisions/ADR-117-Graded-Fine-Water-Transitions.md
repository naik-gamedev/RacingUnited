# ADR-117 — Grade adaptive water topology around aggressive fine detail

**Status:** Accepted

## Decision

Do not allow very large adaptive water control volumes to terminate directly against 0.10 m aggressive-angle detail. Precompute the aggressive support mask and a bounded support-grid distance field, then cap merge span by distance from that detail. Keep large planar cells unrestricted outside the local halo.

Treat the renderer's 0.40 m seam radius as discovery only. Select the nearest coherent opposing seam and apply a smaller scale-aware topology snap limit based on the smaller cell.

## Rationale

WATER14F correctly produced large cells on planar parking lots and sloped roads, but the abrupt size ratio and gather-all seam logic generated excessive coarse-edge insertions and malformed-looking fine triangles. Local grading reduces the ratio before presentation stitching, while nearest-line seam selection prevents unrelated parallel fine boundaries from contaminating a cell.
