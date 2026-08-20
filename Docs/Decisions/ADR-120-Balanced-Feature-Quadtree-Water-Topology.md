# ADR-120: Balanced Feature-Quadtree Water Topology

## Decision

Use a restricted 2:1 balanced quadtree-style hierarchy for 0.50 m+ adaptive hydrology cells around local geometric detail.

A curb or sharp support-height break is treated as a feature line. The renderer may spend ~0.10 m detail on that line, while the solver remains 0.50 m directly beside it and grows through 1/2/4/8/16 m tiers before reaching the 20 m cap.

After coarse packing, inspect actual shared faces and subdivide only cells whose neighbour is more than 2x smaller. This removes isolated fine-to-giant transitions caused by unaligned packing without spreading minimum detail across a broad area.

## Why

Distance-only span caps improved coarse-cell recovery but did not guarantee clean topology. A single leftover 0.50 m leaf could still sit beside a 4 m or larger patch and force ugly coarse/fine stitching. Explicit 2:1 balancing makes the topology itself responsible for a clean transition rather than asking the renderer to hide a pathological mesh.

## Consequences

- Curbs keep a single narrow minimum-detail presentation line.
- Transition bands widen geometrically away from the feature.
- Flat regions recover large cells rapidly.
- Shared-face size jumps are bounded to 2:1 for the 0.50 m+ solver mesh.
- The 20 m cap remains available in uniform far-field regions.
