# VA02 — Embedded GLB wheel-node binding

VA02 turns Heritage semantic wheel nodes inside a complete vehicle GLB into live
presentation nodes driven by the native vehicle simulation.

This milestone builds on:

- GLB hierarchy/material/texture support
- Blender Custom Properties exported as glTF `extras`
- VA01 semantic part discovery
- existing per-wheel native upright/center/spin telemetry

## Authoring contract

For each corner (`FL`, `FR`, `RL`, `RR`) the complete vehicle GLB should contain:

```text
WH_FL_Root
|- WH_FL_BrakeCaliper
`- WH_FL_Pivot
   |- WH_FL
   |  `- WH_FL_Tire
   `- WH_FL_BrakeDisc
```

Equivalent names are used for FR/RL/RR.

The Blender helper used for the Peugeot template adds semantic Custom Properties
to Root/Pivot/Wheel/Tire/BrakeDisc/BrakeCaliper. VA02 requires the semantic
`WH_*_Root` and `WH_*_Pivot` nodes for all four corners before automatic embedded
wheel binding activates.

## Runtime behavior

For each wheel corner:

- `WH_*_Root` receives the authoritative native wheel center and upright world
  orientation.
- `WH_*_Pivot` receives a local wheel-spin rotation offset.
- The brake caliper remains parented to Root and therefore follows suspension,
  steering, camber and toe, but does not spin.
- The rim, tire and brake disc inherit Pivot and therefore spin together.
- Legacy separate articulated wheel entities are hidden while VA02 is active,
  avoiding duplicate visual wheels.

The renderer remains vehicle-agnostic. It exposes generic Entity mesh-node pose
overrides; Racing United Lua decides which semantic nodes are driven by vehicle
telemetry.

## Lua API added

```text
Entity.SetMeshNodeWorldPose(entity, nodeName, px, py, pz, rx, ry, rz)
Entity.SetMeshNodeLocalRotationOffset(entity, nodeName, rx, ry, rz)
Entity.ClearMeshNodeOverrides(entity)
```

These APIs are generic and may later be used for other GLB attachment/authoring
systems besides vehicles.

## Current boundary

VA02 binds the default parts already contained in the complete GLB. It does not
yet load a replacement wheel/tire/spoiler GLB into a slot. Runtime part swapping
is the next modular-parts layer.

Likewise, wheel/tire metadata is not yet allowed to directly rewrite physics.
That remains a separate validated simulation bridge.


## VA02A runtime pivot centering

`WH_*_Root` and `WH_*_Pivot` are both driven to the native wheel center/upright pose.
The pivot then receives wheel spin as a local rotation. This means the Blender pivot
does not need a perfectly zero Root-to-Pivot translation; Heritage establishes the
authoritative rotation axis at runtime. The authored wheel/rim/tire geometry still
keeps its local offsets beneath the pivot.

For authoring, the pivot only needs to lie on the intended axle axis. Its exact
inboard/outboard position along that axis is not important for rotation.

## VA02C authored-subtree anchoring

VA02C removes the last reusable-OBJ presentation assumptions from complete GLB wheel corners.

For each corner, Heritage now treats the authored Blender hierarchy as one rigid subtree:

```text
WH_FL_Root
|- WH_FL_BrakeCaliper
`- WH_FL_Pivot
   |- WH_FL
   |  `- WH_FL_Tire
   `- WH_FL_BrakeDisc
```

`WH_*_Pivot` is used as the anchor. Heritage moves the complete `WH_*_Root` subtree so the authored pivot lands on the native wheel center, then composes the native **vehicle-local** upright rotation around that anchor. The wheel-spin rotation is applied only to `WH_*_Pivot` afterward.

This has three important consequences:

- the caliper remains exactly where Blender authored it relative to the disc while still following suspension / steering / camber / toe;
- left/right wheel facing and any authored per-side transform are preserved instead of being replaced by the old mirrored reusable-wheel assumptions;
- the Root object's origin no longer has to coincide with the hub center. Only the Pivot needs to represent the intended axle/rotation anchor.

Complete GLB vehicle assets should therefore author all four pivots with the same logical local axle convention (local +X is the wheel axle). Side-specific visible-facing transforms belong in the authored wheel/rim/tire objects, not in Racing United's runtime mirror flags.


## VA02E — Blender bind pose is authoritative

For precision-authored vehicle assets, Heritage does not reinterpret the
static node placement from Blender.

The exported GLB bind pose is the source of truth for body orientation,
wheelbase, front/rear track, wheel facing, brake placement and every static
child transform.

At binding activation, Heritage captures the current native wheel state only as
a motion reference. Runtime updates are expressed as deltas from that native
reference and composed on top of the untouched GLB bind pose:

- wheel-center translation delta
- upright steer/camber/toe rotation delta
- wheel-spin delta

Therefore a zero runtime delta produces the exact authored Blender pose.

VA03 can then make the native simulation itself derive wheel reference geometry
from the same authored pivots, eliminating the remaining duplicate geometry
source on the physics side.

## TIRE09 / VIS01 — tire-node deformation without asset rigging

The four `WH_*_Tire` child nodes now have an optional physics-driven GPU deformation path.
Heritage identifies a tire/tyre node by name at mesh upload and infers its centre, shortest-axis
axle, section half-width, inner/bead radius and outer radius from the node's indexed vertices.
Racing United then calls `Entity.SetMeshNodeTireDeformation(...)` with the matching live wheel
state every presentation update.

The current Peugeot asset therefore needs no tire bones, blend shapes or painted deformation
weights. `WH_*` rim nodes and brake nodes are separate and remain rigid. A future higher-accuracy
tire asset may add explicit authored region metadata/masks, but automatic inference remains the
fallback so ordinary creator assets do not require a special rig merely to show load deformation.

The source GLB remains the bind-pose/reference visual asset. Deformation is transient GPU
presentation state and is cleared with the other mesh-node overrides.
