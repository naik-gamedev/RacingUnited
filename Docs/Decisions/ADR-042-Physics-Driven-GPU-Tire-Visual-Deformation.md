# ADR-042 — Physics-Driven GPU Tire Visual Deformation

## Status

Accepted for TIRE09 / VIS01.

## Context

Heritage already owns the tire states needed to describe visible deformation: radial
compression and loaded/effective radius, finite contact-patch dimensions, SWIFT-like belt
translation/yaw/wind-up, inflation pressure and spatial flat-spot wear. The rendered Peugeot
206 RC asset also contains four separate tire nodes (`WH_FL_Tire`, `WH_FR_Tire`, `WH_RL_Tire`,
`WH_RR_Tire`). A visible tire should reflect those authoritative physics states without making
the rendered mesh itself part of the high-rate physics solver.

A node/beam or per-vertex CPU soft-body implementation would duplicate state, scale poorly to
large race grids, and make visual mesh topology a simulation dependency. Conversely, a generic
uniform squash would deform the rim/bead and would not preserve the physical distinction
between bead, sidewall, tread, contact patch and material-fixed wear.

## Decision

Tire physics remains authoritative. Rendering receives a compact per-wheel `TireVisualState`
through the generic Entity mesh-node override boundary and performs deformation in the GPU
vertex path.

For the first path, Heritage automatically recognizes tire/tyre-named GLB nodes and derives:

- authored tire centre;
- axle axis from the shortest mesh extent;
- half section width;
- inner/bead radius;
- outer/free visual radius.

This makes the current Peugeot tire usable without editing its GLB, adding bones, painting
vertex masks or changing creator pivots. Runtime deformation is converted from metres into the
authored mesh's local scale using the physics reference radius.

The vertex shader applies bounded deformation only to the tire node:

1. bead-to-tread radial masks keep the bead region nearly rigid;
2. radial compression creates a road-facing flat tread patch rather than scaling the whole wheel;
3. lower sidewalls bulge outward from physical radial deflection;
4. rigid-ring longitudinal/lateral/radial translation moves the outer carcass relative to the bead;
5. ring wind-up and a deliberately reduced visual yaw deform the belt/tread;
6. TIRE08's material-fixed deepest wear sector produces a local visual flat-spot dent;
7. the shadow pass uses the same position deformation so tire silhouettes and shadows agree.

The source GLB is unchanged. Rim, brake disc and caliper nodes remain rigid. The presentation
state updates at rendering/Lua cadence while the underlying tire/contact/ring physics continues
at its native high rate.

## Current Peugeot validation asset

The supplied Peugeot 206 RC GLB exposes four separate `WH_*_Tire` nodes. Inspection of the
current provisional mesh gives an envelope of approximately 205 mm section width and 595 mm
overall diameter, close enough to the authored 205/40 R17 technical envelope to exercise this
system. The detailed mesh remains non-authoritative; explicit tire metadata/physics dimensions
still outrank its detailed tread/sidewall shape.

## Consequences

- Visual vertex count does not multiply tire-physics cost.
- A high-detail player tire can visibly deform while distant vehicles can later use cheaper
  presentation LODs without changing physics.
- The same API can support future accurately authored tires and motorcycle tires.
- No tire-specific Blender rig is required for the current prototype.
- Flat-spot *visual* radius variation is now represented, but TIRE08's wear state still does not
  yet feed circumferential radius variation back into contact/suspension vibration. That remains
  a separate physical mechanism.
- The first contact flattening direction uses world gravity projected into tire-local space.
  A later refinement may supply the exact road-contact normal for highly banked/irregular surfaces.
- Tangent-space deformation is approximate in this first pass; authored normal-map tangents are
  not fully re-derived from the deformed surface.

## Rejected alternatives

### Full node/beam tire soft body

Rejected as the baseline because it spends high-rate CPU work on rendered topology and is poorly
matched to Heritage's large-grid target.

### CPU per-vertex deformation

Rejected because it repeats work the GPU can perform naturally and increases per-frame CPU/data
traffic.

### Uniform wheel squash

Rejected because it visually deforms the bead/rim relationship and cannot represent finite
contact patches, sidewall bulge, ring motion or material-fixed flat spots.
