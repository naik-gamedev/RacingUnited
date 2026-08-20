# PERF18 – Organic Sub-Cell Water Shoreline

PERF18 removes the last whole-cell visual activation from the connected-water presentation.

## Problem
PERF16/PERF17 connected the water interior and added a world-space breakup mask, but a newly eligible 0.5 m hydrology cell could still reveal its square/triangular footprint. The mask only modified fragments after geometry already existed.

## Solution
- Authoritative hydrology remains a 0.5 m cell field.
- Fine presentation cells now carry reconstructed water depths at their four exact corners.
- A corner samples the four hydrology centres surrounding that corner. The two strongest support-compatible depths are averaged so adjacent wet cells remain connected while isolated forming water contracts toward the cell centre.
- Collision/support disagreement at a shared corner rejects that neighbour, preserving curb/bridge/tunnel discontinuities.
- The connected mesh interpolates those corner depths through its shared vertices.
- The water fragment shader clips against the real per-ring explicit-water threshold (1.5 mm near, 2 mm mid, 3 mm far) through that continuous depth field.
- The user-supplied grayscale breakup mask perturbs the threshold in continuous world space.
- A fine world-anchored stochastic coverage test removes residual geometric regularity without screen-space crawling.
- Deep water bypasses shoreline breakup and remains solid.

## Performance intent
The expensive sub-cell treatment is naturally confined to fine fallback leaves around active puddle boundaries. Flat/deep interiors remain eligible for the existing adaptive 1/2/4/8/16 m connected mesh.
