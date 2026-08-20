# WATER11 - Stable Hybrid Water Surface

## Decision
Settled road water is no longer reconstructed from GPU parcel coverage. Authoritative hydrology depth is the primary continuous surface representation. 3D parcels are a secondary detail representation for detached water only.

## Why
WATER10-D/E tried to represent millimetre-deep settled water as 3D parcel splats. This created holes and temporal disappearance because screen-space parcel coverage was incorrectly allowed to decide whether the puddle existed. For shallow road water, the stable and cheapest representation is the continuous hydrology/free-surface field; true 3D work is reserved for events that actually leave that surface.

## Presentation
The existing two-layer depth/stencil receiver path now stores physical water depth, reconstructs it bicubically, derives smooth normals from support geometry + depth gradients, and shades the continuous layer with the environment map. It does not write scene depth and cannot z-fight with the road.

## 3D detail
WaterParcelRenderer remains GPU resident at 20 Hz with render interpolation, but settled parcels are hidden. Only detached water is allowed into the screen-space particle-fluid renderer. Tire splash/breaking-wave impulses can activate that tier later without making a flat puddle depend on particle coverage.
