# ADR-113: Adaptive Control-Volume Water Authority

## Status
Accepted for WATER14.

## Decision
Replace fixed-size authoritative hydrology cells with variable-area control volumes ranging from 0.10 m to 20.0 m. Retain the historical 0.5 m raster only as immutable terrain-bake support.

Each control volume owns conserved water volume and flow velocity. Adjacent volumes are connected by one persistent signed virtual pipe whose hydraulic width is the shared boundary length and whose gradient length is the distance between volume centres.

The visible settled-water mesh is generated directly from these authoritative adaptive volumes. The old contour-mesher and settled GPU-parcel experiments are removed from the build and repository overlay cleanup.

## Why
The fixed 0.5 m simulation field made cost scale with terrain area even where water/terrain were essentially planar. Adding separate adaptive presentation on top duplicated work and accumulated legacy code paths. Adaptive authority removes that duplication and makes both simulation and rendering cost follow local geometric complexity.

## Consequences
- Flat planar areas can collapse to very few large simulation cells.
- Sharp terrain/material boundaries can refine down to 0.10 m.
- Water mass remains conservative because state is stored as volume.
- Pipe topology is more complex because neighbouring cells can differ in size.
- Topology is bake-time/static in WATER14; dynamic split/coarsen from changing wet fronts is intentionally deferred until this implementation is profiled live.
