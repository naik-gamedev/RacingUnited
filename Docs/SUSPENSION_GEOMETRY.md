# Native Suspension Geometry

## Purpose

Step 29P separates wheel/upright kinematics from spring and damper forces.
`SuspensionGeometry` is the native provider boundary that owns the orientation
and wheel-centre geometry used by tire contact, telemetry and articulated wheel
presentation. Lua may author and display geometry, but it does not reconstruct
steering, camber, toe or linkage motion from visual transforms.

## Provider: `linear_raycast_v1`

The compatibility `linear_raycast_v1` provider keeps the existing straight
suspension-axis wheel path and evaluates signed alignment curves from wheel
compression `x` in metres:

`angle = static + gain * x + 0.5 * progression * x * abs(x)`

Camber and toe have independent static, gain and progression values. Each
contact also carries a normalized three-dimensional steering axis in chassis
local coordinates. The provider composes toe, camber and Ackermann steering,
then returns an orthonormal right-handed upright basis plus the local Euler pose
used by the entity presentation API.

Positive and negative curve signs are authoring conventions, not automatic
left/right mirroring. A definition must give each contact the values measured
for that corner. This permits asymmetric cars, oval setups and damaged geometry
later without category-specific solver code.

## Provider: `macpherson_strut_v1` (SUS02)

SUS02 adds the first hardpoint-derived linkage provider. It consumes the fixed
MacPherson contract authored per corner:

- `strut_top_mount`
- `strut_upright_mount`
- `lower_arm_inner_front`
- `lower_arm_inner_rear`
- `lower_ball_joint`
- `tie_rod_inner`
- `tie_rod_outer`
- `wheel_center`

The lower control arm is treated as a rigid triangle rotating about the axis
through its two inner pivots. Requested wheel compression solves the lower ball
joint position analytically about that hinge. The current steering axis runs
from the solved lower ball joint to the fixed strut top mount. A rigid upright
reference basis carries the wheel centre and strut-upright mount with that
steering axis.

The tie rod keeps its authored reference length. A passive steering rotation is
solved around the current steering axis so the outer tie-rod point still meets
that length; this becomes hardpoint-derived bump steer. Commanded steering is
then composed on the same current steering axis. Camber, toe, wheel-centre
position, steering-axis direction, upright pose and strut compression all come
from the solved linkage instead of fitted travel curves.

The provider also derives an instantaneous spring motion ratio numerically from
the local derivative of strut compression versus wheel compression. The
existing nonlinear spring/damper force model consumes this live ratio, so the
linkage geometry can change wheel force through travel without a separate
Peugeot-specific force solver.

## Provider: `trailing_arm_torsion_bar_v1` (SUS03B)

SUS03B promotes the trailing-arm scaffold to a runnable reusable provider. Each
corner consumes five hardpoints:

- `arm_pivot_inner`
- `arm_pivot_outer`
- `wheel_center`
- `damper_upper_mount`
- `damper_lower_mount`

The arm is rigid and rotates about the line joining its two pivots. Requested
wheel compression is solved as arm rotation, so the wheel centre follows the
actual circular linkage path instead of a straight-line approximation inside
the kinematics provider. The lower damper eye rotates with the arm while the
upper eye remains chassis-fixed, producing geometry-derived damper compression
and instantaneous damper motion ratio.

Arm rotation is also the torsion-bar twist coordinate. Its instantaneous angular
motion ratio (radians of bar twist per metre of wheel travel) is passed to the
rotational torsion-bar spring helper. This keeps rear springing mechanically
distinct from a coil spring while retaining a creator-friendly reference wheel
rate until better torsion-bar stiffness data is available.

## Authoritative consumers

- `VehicleSystem` evaluates geometry in every high-rate wheel substep after the
  current suspension compression is known.
- The tire contact basis consumes native forward orientation, so steering-axis
  inclination, toe and hardpoint-derived bump steer affect force direction.
- The suspension force path consumes MacPherson instantaneous strut leverage or
  trailing-arm torsion/damper leverage when a valid mechanism solution exists.
- `Vehicle.GetWheelUprightPose` exposes the native pose and basis.
- Articulated wheel meshes compose authored mesh facing and spin after the
  native upright pose.
- Dynamics Lab records native wheel state for captures and diagnostics.
- `Vehicle.Set/GetWheelSuspensionGeometry` remains the validated live geometry
  boundary for providers it can represent directly; VehicleDefinitionV2 is the
  canonical path for authored mechanism hardpoints.

## Deliberate limits

`macpherson_strut_v1` is a hardpoint-derived kinematic provider inside the
existing raycast-wheel vehicle architecture. It is not yet a compliant
multibody suspension simulation. Bush/link/chassis compliance, component
inertia, structural loads, flex, explicit ball-joint constraints, exact curved
wheel-path/road intersection, jacking-force decomposition and camber thrust are
separate future work.

The compatibility curve provider also remains useful where only measured
alignment traces are known or where a lower simulation tier is desired.
Hardpoint providers and curve providers therefore coexist behind the same
native output contract rather than one replacing the other globally.

## Peugeot-oriented prototype status

The 206-oriented prototype declares MacPherson as its preferred front mechanism
and trailing-arm/torsion-bar as its preferred rear mechanism. SUS03B can promote
all four corners to mechanism-specific providers using the versioned low-confidence
assisted packages when no better hardpoints are available. Both estimators use
chassis reference-package scales rather than the currently installed wheel/tire,
so fitment changes cannot silently move suspension pickup points (ADR-025).

Its provisional alignment seed remains nominal zero front camber, approximately
7 arcminutes toe-out per front wheel, approximately one degree negative rear
camber and 16 arcminutes toe-in per rear wheel. The front steering axes carry
family-reference values of approximately 3 degrees 16 minutes caster and
9 degrees 42 minutes steering-axis inclination.

Named GLB hardpoint nodes or measured/modelled coordinates can replace the
estimated points without changing tire, spring/damper, telemetry or presentation
contracts.

## Separate mechanism: suspension anti-roll bar (SUS04)

Anti-roll coupling is intentionally outside the kinematics providers.
`SuspensionAntiRollBar` couples two explicit contact units through torsional
stiffness/damping, left/right lever arms and link motion ratios. The same native
mechanism therefore works with MacPherson, trailing-arm, double-wishbone and
future axle layouts. VehicleDefinitionV2 carries these bars independently and
the 1000 Hz vehicle loop evaluates them from a synchronized pre-wheel snapshot
to avoid wheel-order-dependent coupling. See ADR-027.

The current Peugeot-oriented front and rear bars are low-confidence (`0.20`)
`estimated` starting data rather than claimed factory rates.

## Assisted authoring and provenance (SUS03A/SUS03B)

Hardpoint providers no longer require factory-CAD coordinates as their only
practical input path. `MacPhersonHardpointEstimator` and
`TrailingArmHardpointEstimator` construct complete mechanism contracts from
reference wheel centres plus immutable chassis suspension-package scales; the
front estimator additionally accepts provisional caster/SAI. The estimators are
creator tools, not alternative physics providers: their ordinary hardpoint
output is consumed by the same native mechanism solvers as measured data.

Every inferred point is explicitly tagged `estimated` with low confidence.
Vehicle GLB nodes can later replace individual estimates through stable
`SUS_FL/SUS_FR/SUS_RL/SUS_RR` names or semantic extras. This preserves one
physics mechanism per suspension type while progressively improving its input
evidence.
