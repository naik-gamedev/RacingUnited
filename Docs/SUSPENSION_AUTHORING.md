# Suspension Authoring

## SUS03B purpose

SUS01 created the hardpoint authoring boundary, SUS02 implemented reusable
`macpherson_strut_v1` front kinematics, and SUS03A made estimated front authoring
practical. SUS03B applies the same evidence-aware workflow to the rear and adds
`trailing_arm_torsion_bar_v1`, so the prototype can run mechanism-specific
front and rear suspension without pretending estimated coordinates are factory
measurements.

The governing rule is not "never estimate." It is **never confuse an estimate
with a measurement.** Better evidence can replace estimated points later without
changing the provider or vehicle topology.

## Hardpoint source contract

Each VehicleDefinitionV2 hardpoint may carry:

```text
id
position = { x, y, z }       -- chassis-local metres
provenance                   -- e.g. measured / asset_authored / estimated
confidence                   -- 0.0 .. 1.0
```

Hardpoint IDs are stable, unique per suspension component and finite. Source
metadata does not select a different physics formula; it records how trustworthy
the input data is.

Racing United currently prioritizes sources as:

1. `measured`
2. `asset_authored`
3. `legacy_authored`
4. `estimated`

This permits partial upgrades. A GLB may replace only the points its creator has
located accurately while the remaining points continue using estimates.

## Assisted MacPherson profile

`estimated_macpherson_road_v1` is a deterministic compact-road-car package.
Inputs are deliberately limited to information a small team can normally obtain:

- reference wheel centre;
- immutable chassis suspension-package scale;
- approximate caster; and
- approximate steering-axis inclination.

The package scale is deliberately **not** the currently installed tire radius.
Wheel/rim/tire changes are downstream fitment changes and must never regenerate
chassis suspension pickup points (ADR-025).

The output is the complete eight-point MacPherson contract required by
`macpherson_strut_v1`. The current profile confidence is deliberately low
(0.35). It is a coherent starting linkage, **not Peugeot factory CAD**.

For the current Peugeot-oriented prototype, the front-left and front-right
packages are generated independently from their own wheel centres. When both
front sets are complete, Racing United activates native MacPherson kinematics on
the front wheels.

## Assisted trailing-arm/torsion-bar profile

`estimated_trailing_arm_torsion_bar_road_v1` supplies the five-point contract for
`trailing_arm_torsion_bar_v1` when stronger rear geometry is unavailable:

- `arm_pivot_inner`
- `arm_pivot_outer`
- `wheel_center`
- `damper_upper_mount`
- `damper_lower_mount`

It uses the rear wheel centre and immutable rear suspension-package scale, carries
low confidence (0.30), and is a packaging estimate rather than factory geometry.
The native provider rotates the arm about the authored pivot axis, derives actual
wheel/damper arcs and uses arm rotation as torsion-bar spring travel.

## GLB hardpoint authoring

A vehicle GLB may provide suspension points using Blender empties/nodes. The
simple naming contract is:

```text
SUS_FL_<hardpoint_id>
SUS_FR_<hardpoint_id>
SUS_RL_<hardpoint_id>
SUS_RR_<hardpoint_id>
```

Examples:

```text
SUS_FL_strut_top_mount
SUS_FL_lower_ball_joint
SUS_FR_tie_rod_outer
```

Alternatively, node extras may identify a hardpoint semantically with
`heritage.part_type=suspension_hardpoint`, `heritage.hardpoint_id` and
`heritage.corner`. Optional provenance/confidence extras are preserved.

`Vehicle.InspectAssetMetadata` exposes these nodes to Lua. Racing United imports
them automatically when GLB metadata is refreshed; they override lower-priority
estimates and can also be imported manually from the Suspension AUTHORING page.

## Creator UI and gizmos

The Vehicle `SUSP.` -> `AUTHORING` page shows:

- selected suspension mechanism and active runtime provider;
- required hardpoint completeness;
- source and confidence for every hardpoint;
- the active assisted-authoring profile;
- how many wheels currently use hardpoint kinematics;
- a button to rebuild only estimated suspension geometry; and
- a button to import hardpoints from the current GLB.

Gizmo colors:

- Green: measured.
- Magenta: GLB/asset-authored.
- Orange: estimated.
- Grey: legacy/unknown authored data.
- Cyan: reference wheel centre.
- Orange reference marker: full bump.
- Blue: full droop.
- Yellow: steering-axis direction.

Debug markers never participate in collision or vehicle forces.

## Current Peugeot architecture

Front:

```text
MacPherson kinematics
+ coil spring
+ strut damper
+ front anti-roll group
```

Rear:

```text
Trailing-arm kinematics
+ transverse torsion-bar springing
+ separate dampers
+ rear anti-roll group
```

Both axles can now run their mechanism-specific providers from estimated or
better data. Front uses `macpherson_strut_v1`; rear uses
`trailing_arm_torsion_bar_v1`. Estimated data remains replaceable point-by-point.
SUS04 supplies front and rear anti-roll coupling as a separate reusable
mechanism. It remains outside both kinematics providers and can couple any
explicit left/right contact-unit pair.

## Fidelity boundary

Both hardpoint providers remain kinematic mechanisms inside the scalable
raycast-wheel architecture rather than full compliant multibody assemblies.
MacPherson derives alignment, bump steer, steering-axis motion, strut travel and
instantaneous spring leverage. The trailing arm derives its wheel arc, arm/torsion
rotation and separate-damper leverage. Compliance, structural flex/link loads,
detailed jacking/scrub telemetry and compliant anti-roll-bar links/bushings remain future layers.
