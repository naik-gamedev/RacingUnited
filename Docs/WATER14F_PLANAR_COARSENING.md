# WATER14F - Planarity-Driven Adaptive Hydrology Coarsening

WATER14F keeps WATER14E's 40 cm topology-aware seam discovery, triangle-only stitched surface, full current-map presentation budgets, and WATER14C/D distance-adaptive collider-normal offset. It changes how the authoritative adaptive hydrology decides whether broad terrain can become a large control volume.

## Problem

The previous merge test compared every support sample against one anchor sample and required a very small normal difference. A sidewalk/curb inside a globally aligned candidate could also prevent a large patch from forming over otherwise flat asphalt beside it. In practice this produced too many 0.5 m / small cells on parking lots and uniformly sloped roads.

## Changes

- Coarse candidates now fit one least-squares plane to their 0.5 m support samples.
- The primary geometric error is maximum elevation residual from that fitted plane. Uniform slopes are therefore treated as planar rather than "steep = detailed".
- Candidate support normals are compared against the fitted plane normal with a 10 degree tolerance. This tolerates modest collision-triangle normal noise while still rejecting real creases/angular changes.
- The surface residual allowance remains centimetre-scale: 20 mm base plus 1.5 mm per metre of candidate size (50 mm at the 20 m maximum). A normal curb/step remains far outside the allowed residual and blocks only candidates that cross it.
- Large-to-small greedy packing is no longer forced to start on global span-aligned coordinates. After a curb invalidates one candidate, a large planar cell may begin immediately beside the curb instead of waiting for an arbitrary world-grid alignment.
- Accepted adaptive cells use the fitted plane for their authoritative support elevation/normal.

## Unchanged

- 0.10 m cells are still reserved for aggressive angular geometry: >=55 degree source slope or >=30 degree local normal break.
- Material/exposure/hydraulic-property boundaries remain authoritative merge boundaries.
- Adaptive cell range remains 0.10 m to 20 m.
- Unequal-cell virtual pipes, water volume conservation, rain input, tire-water physics and distance cadence are unchanged.
- Explicit-water depth thresholds are unchanged.
- WATER14E visible-water stitching remains 40 cm topology-aware discovery and triangle-only shared skin.
- WATER14C/D normal offset remains 3 cm near the camera, smoothly reaching 20 cm at 500 m.

## Regression coverage

The native SurfaceWorld regression now includes:

1. an 8 x 8 m perfectly planar downhill surface whose source normals intentionally alternate by about 11 degrees; it must still coarsen to an ~8 m adaptive cell, and
2. a 15 cm step at an unaligned X coordinate; candidates crossing the step must fail, while the broad planar region beside it must still form a 4 m control volume.

This protects the intended rule: **refine because one plane cannot represent the collider, not merely because the road is sloped or a local feature exists somewhere in a larger grid block.**
