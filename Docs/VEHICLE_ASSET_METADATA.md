# Vehicle GLB Metadata and Modular Parts

VA01 established the first bridge between Blender-authored vehicle semantics and
Heritage Engine runtime data. Later VA02 milestones now consume selected semantic
nodes for live presentation while keeping metadata inspection/validation separate
from simulation authoring.

It does **not** make the renderer guess what a mesh means from geometry. The
creator marks objects with Blender Custom Properties, exports them through glTF
`extras`, and Heritage preserves and validates those fields.

## Authoring flow

```text
Blender .blend
  -> Object Custom Properties
  -> Export glTF Binary (.glb) with Custom Properties enabled
  -> glTF node `extras`
  -> Heritage GLB metadata inspector
  -> VehicleAssetMetadata
  -> Racing United Asset Data panel / compatibility checks
```

The complete stock car may remain a convenient assembled authoring asset, for
example:

```text
Vehicle_Peugeot_206_RC.glb
```

while replacement parts may later live as independent GLB assets in a parts
library.

## Stable slot names vs installed part IDs

A stable slot describes **where / what the thing is**:

```text
WH_FL
WH_FL_Tire
WH_FR
WH_FR_Tire
WH_RL
WH_RL_Tire
WH_RR
WH_RR_Tire
```

The part ID describes **what is installed there**:

```text
WH_Peugeot_Atlantis_17x7J_108x4_1P
Pirelli_PZero_Nero_205_40_ZR17_84W
```

This separation is important because the slot remains stable when a modder
installs another wheel or tire.

## Wheel hierarchy contract

The current Peugeot template uses:

```text
WH_FL_Root
|- WH_FL_BrakeCaliper
`- WH_FL_Pivot
   |- WH_FL
   |  `- WH_FL_Tire
   `- WH_FL_BrakeDisc
```

Equivalent nodes may exist for `FR`, `RL` and `RR`.

Semantics:

- `*_Root`: follows suspension/upright/steering pose, but not wheel spin.
- `*_Pivot`: wheel-spin transform.
- brake caliper: follows upright, does not spin.
- wheel/rim: spins and is replaceable.
- tire: child of its wheel, spins and is independently replaceable.
- brake disc: spins with the wheel.

VA01 originally stopped at discovery. VA02 now binds complete embedded GLB wheel
subtrees to native wheel-center/upright/spin telemetry while preserving the Blender
bind pose. FITMENT01 consumes validated wheel/tire technical metadata for installed
ET/spacer/tire-radius setup while preserving suspension geometry. FITMENT02 adds
explicit hub-face/centerline/spin-axis datum semantics and live scrub/trail
diagnostics. Replacement-part visual instantiation remains a later layer. See `VEHICLE_ASSET_NODE_BINDING.md` and `WHEEL_FITMENT_AND_ALIGNMENT.md`.

## Scalar Custom Properties

The first metadata contract preserves scalar Blender Custom Properties:

- strings
- numbers
- booleans

Nested JSON objects are flattened with `.` separators. Arrays are not promoted
into the VA01 scalar metadata contract.

Important semantic fields include:

```text
heritage.metadata_version
heritage.part_type
heritage.part_id
heritage.role
heritage.slot
heritage.corner
heritage.replaceable
heritage.rotates_with_wheel
```

Example wheel properties:

```text
wheel.manufacturer
wheel.model
wheel.diameter_in
wheel.width_in
wheel.flange_profile
wheel.bolt_count
wheel.pcd_mm
wheel.offset_et_mm
wheel.center_bore_mm
wheel.hub_face_diameter_mm
wheel.construction
wheel.hump_profile
wheel.hump_outboard
wheel.hump_inboard
wheel.fastener_thread
wheel.fastener_seat
wheel.mass_kg_estimate
wheel.mass_status
```

Example tire properties:

```text
tire.manufacturer
tire.model
tire.width_mm
tire.aspect_ratio
tire.rim_diameter_in
tire.construction
tire.marking
tire.load_index
tire.max_load_kg
tire.speed_rating
tire.max_speed_kmh
tire.extra_load
```

Optional tire compatibility fields:

```text
tire.rim_width_min_in
tire.rim_width_max_in
```

If these are absent Heritage reports compatibility as incomplete rather than
inventing a fitment range.

## Native metadata service

`VehicleAssetMetadata` is deliberately separate from rendering and from the
native tire solver.

```text
GLB extras
   -> VehicleAssetMetadata
      -> validation / compatibility
      -> UI / modding tools
      -> explicit authoring bridges (for example suspension hardpoints)
      -> validated wheel/tire fitment authoring bridge (FITMENT01/FITMENT02)
```

This prevents a random visual GLB field from silently rewriting physics.

The service currently:

- discovers semantic parts on GLB nodes;
- retains all scalar authored properties;
- detects duplicate slots;
- checks required wheel/tire fields;
- warns if a tire is not parented to a Heritage wheel part;
- derives nominal outside tire diameter from width/aspect/rim size;
- checks tire-to-wheel diameter compatibility;
- checks authored tire rim-width ranges when available.

## Suspension hardpoint nodes (SUS03A)

Vehicle GLBs may also carry suspension hardpoint node origins. These are not
renderable parts; they are engineering reference points in chassis-local space.

The simplest authoring contract is a named Blender empty/node:

```text
SUS_FL_strut_top_mount
SUS_FL_lower_ball_joint
SUS_FL_tie_rod_outer
SUS_FR_strut_top_mount
...
```

Corner prefixes are `FL`, `FR`, `RL`, and `RR`. A node may alternatively use
explicit glTF extras:

```text
heritage.part_type = "suspension_hardpoint"
heritage.corner = "front_left"
heritage.hardpoint_id = "strut_top_mount"
heritage.provenance = "asset_authored"
heritage.confidence = 0.75
```

`VehicleAssetMetadata` computes the node origin through the glTF hierarchy and
returns its position in vehicle/chassis-local coordinates. Duplicate
`corner:id` hardpoints are rejected. Asset-authored points default to
`provenance=asset_authored` and confidence `0.75` unless the asset states a
valid explicit source/confidence.

Racing United's suspension authoring layer can explicitly import these points.
A higher-quality source replaces a lower-quality estimated point, while an
estimate never overwrites an asset-authored or measured point. This is an
explicit authoring bridge rather than arbitrary visual metadata silently
rewriting simulation.

`Vehicle.InspectAssetMetadata()` now additionally returns
`metadata.suspension_hardpoints` and `metadata.suspension_hardpoint_count`.
Each hardpoint entry includes its node name/index, corner, stable hardpoint ID,
local XYZ position, provenance, and confidence.

## Lua API

### `Vehicle.InspectAssetMetadata(assetPath)`

Accepts a module-asset-relative `.glb` path and returns:

```lua
local metadata, errorMessage = Vehicle.InspectAssetMetadata(
    "Vehicles/Player/Vehicle_Peugeot_206_RC.glb")
```

`metadata.parts` is keyed by stable Heritage slot, for example:

```lua
local rearRightWheel = metadata.parts["WH_RR"]
local rearRightTire = metadata.parts["WH_RR_Tire"]
```

Each part includes its semantic fields plus a `properties` table containing the
original scalar GLB extras.

### `Vehicle.CheckTireWheelCompatibility(assetPath, wheelSlot, tireSlot)`

Returns:

```text
compatible, complete, summary
```

`compatible=false` means an authored rule is violated.
`complete=false` means some useful compatibility metadata has not yet been
authored.

## Racing United UI

Vehicle -> Visual now has an **ASSET DATA** tab. When the active body visual is
a GLB, Racing United automatically inspects its semantic metadata and presents
FL/FR/RL/RR wheel/tire information in a human-readable form.

The Body tab also has **SELECT OBJ / GLB FROM ASSETS...** so a newly exported
vehicle GLB can be selected without editing Lua by hand.

## What the current metadata/part layer intentionally does not do yet

The current metadata/part layer does not yet:

- hide / replace individual GLB nodes at runtime;
- instantiate replacement wheel/tire GLBs into slots;
- treat arbitrary visual-part metadata or ambiguous node origins as suspension
  hardpoints or wheel-centerline datums; suspension hardpoints use the explicit
  SUS03A authoring/import contract and FITMENT01 only consumes validated fitment fields;
- calculate wheel-to-hub PCD/center-bore compatibility against a vehicle hub
  definition;
- calculate tire failure from load/speed/temperature.

Those now have a clean metadata source instead of requiring hard-coded guesses.
Embedded default wheel-node presentation binding already exists through VA02 and
FITMENT01 now provides explicit validated fitment/alignment simulation consumption.
The next modular-vehicle milestones can add replacement-part loading, explicit
hub-face/centerline/spin-axis origin semantics, clearance and component mass/inertia.

## Wheel fitment datum nodes (FITMENT02)

Do not assume the origin of a `WH_*` render node simultaneously means hub face,
wheel centerline and spin axis. A vehicle GLB may explicitly author engineering
datums instead.

Supported semantic extras are:

```text
heritage.part_type = "wheel_fitment_datum"
heritage.corner = "front_left" | "front_right" | "rear_left" | "rear_right"
heritage.datum_role = "hub_face_center" | "wheel_centerline" | "wheel_spin_axis"
heritage.provenance = "asset_authored"       # optional
heritage.confidence = 0.75                    # optional
```

Stable node-name aliases are also recognized:

```text
FIT_FL_HubFace          FIT_FR_HubFace
FIT_FL_WheelCenterline  FIT_FR_WheelCenterline
FIT_FL_SpinAxis         FIT_FR_SpinAxis
FIT_RL_...              FIT_RR_...
```

`Vehicle.InspectAssetMetadata` reports these in
`metadata.wheel_fitment_datums` and `metadata.wheel_fitment_datum_count`. Every
datum includes node name/index, corner, role, chassis-local position, provenance
and confidence. `wheel_spin_axis` additionally exposes the node's normalized local
+X direction after the glTF basis conversion.

The current Peugeot GLB is not required to contain these nodes. Its explicit
wheel/tire technical metadata and established reference wheel transforms remain
the stronger evidence for the current provisional asset until a higher-accuracy
replacement model is authored.
