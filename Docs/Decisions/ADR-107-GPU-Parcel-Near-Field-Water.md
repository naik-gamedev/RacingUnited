# ADR-107 — GPU parcel near-field water over global shallow-water authority

## Decision

Keep WATER09's virtual-pipe shallow-water field as the authoritative global water-mass representation and use a bounded GPU 3D parcel simulation for visible near-field liquid. Do not make triangle/quad hydrology patches the final near-field water representation.

## Performance policy

The first parcel solver uses a fixed occupancy grid rather than a sorted neighbour list and multi-iteration PBF solve. Simulation runs at 20 Hz and presentation interpolates at render rate. Hydrology/support data is refreshed at 10 Hz. Fluid reconstruction runs at half resolution with two separable bilateral passes. Active parcels are compacted and indirect-drawn entirely on the GPU with no active-count readback.

## Rationale

This gives genuine XYZ water motion and screen-space surface merging while bounding both CPU and GPU work. Full PBF/XPBD remains an optional quality tier if later profiling proves affordable and visibly beneficial.
