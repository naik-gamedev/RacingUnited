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

## Physical wheel-path contact authority (SUSP05)

Hardpoint-derived wheel-centre motion is now consumed by the physical tire-support
query, not only by upright orientation and presentation. The high-rate solver uses
the previous 1 kHz geometry state to obtain the wheel-centre displacement relative
to its authored reference position, removes the component along the suspension
axis, and offsets the next support ray by the remaining lateral/longitudinal motion.
This captures linkage scrub/fore-aft path and steering-axis scrub without creating
a second bump/droop solver. `linear_raycast_v1` retains a zero offset.

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

## Provider: `double_wishbone_v1` (SUSP06)

SUSP06 adds a conventional unequal-length double-wishbone hardpoint provider.
Upper and lower A-arms each rotate about their two chassis pivots. A bounded
Newton solve finds both arm angles that simultaneously preserve the rigid
upper/lower-ball-joint upright length and satisfy requested wheel-centre travel.

The instantaneous upper/lower-ball-joint line is steering-axis authority. The
rigid upright carries the wheel centre and tie-rod outer point; tie-rod length
therefore creates passive bump steer before commanded steering is applied. The
provider reports live camber, toe, caster, kingpin inclination and wheel-centre
path rather than using authored camber/toe travel curves.

A chassis-fixed damper upper eye and lower-arm-fixed lower eye derive damper
compression and instantaneous motion ratio. This is the direct-acting wishbone
provider; pushrod/rocker suspension remains a separate topology because its
spring/damper leverage is indirect.

As with MacPherson and trailing-arm providers, SUSP05 is the sole contact-query
bridge: only the wheel-centre motion perpendicular to the authored suspension
axis offsets the physical 1 kHz tire support query.

## Provider: `pushrod_double_wishbone_v1` (SUSP07)

SUSP07 reuses the SUSP06 unequal-length A-arm/upright/tie-rod solve and adds an
indirect inboard actuation chain. The outboard pushrod pickup is fixed to the
lower arm. Its inboard pickup rotates on a rigid rocker about a chassis-fixed
axis; rocker angle is solved from constant pushrod length for each wheel pose.

Separate spring and damper rocker pickups are supported. Their physical shaft
compression and instantaneous motion ratios are evaluated independently, so a
rocker may produce different rising/falling-rate curves for spring and damper.
The common SUSP05 wheel-centre support-query bridge remains authoritative for
physical contact scrub.


## Provider: `live_axle_v1` (SUSP08)

SUSP08 introduces pair-coupled suspension geometry. Both wheel centres belong to
one rigid axle body, so equal travel produces bounce and asymmetric travel
produces axle roll while preserving track width. A fixed-length Panhard rod and
paired trailing links determine lateral/longitudinal axle path. Independent
spring and damper attachment geometry on each side provides actual shaft
compression and instantaneous leverage.

The 1 kHz vehicle step snapshots both wheel compressions before advancing either
wheel, so left/right iteration order cannot become a hidden axle input. SUSP05
remains the sole bridge from the solved axle wheel-centre path to tire-support
query scrub.

## Provider: `live_axle_leaf_v1` (SUSP09)

SUSP09 keeps the complete SUSP08 rigid axle location/roll solution and adds a
semi-elliptic leaf-pack/shackle mechanism. Each side solves the moving rear leaf
eye from fixed rear-leaf-segment and shackle lengths. Signed pack sag relative
to the instantaneous front-eye/rear-eye chord supplies actual leaf compression;
its derivative supplies nonlinear leaf motion ratio.

Interleaf Coulomb/viscous hysteresis is an explicit spring-pack loss mechanism,
not shock tuning. A separate paired axle-housing torsional state responds to the
physical tire longitudinal reaction torque, providing bounded wind-up/release,
tramp excitation and jacking coupling while both wheel centres remain owned by
the one SUSP08 rigid axle body.

## Providers: `motorcycle_telescopic_fork_v1` and `motorcycle_swingarm_linkage_v1` (SUSP10)

SUSP10 makes conventional motorcycle suspension geometry native rather than
reusing car camber/toe curves. The telescopic-fork provider uses the authored
upper/lower steering-stem line as both steering and fork-slide authority. Axle
compression moves along that line and commanded steer rotates the moved axle
about it. The output exposes live rake, steering-axis point/direction,
wheel-centre path and wheelbase change; SUSP05 carries the transverse component
of that path into the physical tire-support query.

The rear provider rotates the axle and dogbone pickup on a rigid swingarm axis.
A fixed dogbone length solves a chassis-pivoted rocker, whose moved shock pickup
produces actual shaft compression and instantaneous shock leverage. The
countershaft-to-axle distance derivative is also exposed for the SUSP10 chain
anti-squat virtual-work force path.

These providers are suspension mechanisms only. A complete two-wheel runtime
still needs free steering/rider/lean/gyroscopic vehicle dynamics; that boundary
remains explicit in VehicleDefinition rather than being approximated here.


## Provider: `kart_chassis_flex_v1` (SUSP11)

SUSP11 models a racing kart without inventing four conventional suspension
units. A complete ten-point package authors the left/right front kingpin upper
and lower points, front wheel centres, rear axle bearing points and rear wheel
centres. The front wheel centre rotates around its physical inclined kingpin
when steering, so caster/KPI geometry creates real steering jacking. Unlike the
other SUSP05 descendants, the kart support offset deliberately preserves this
vertical kingpin displacement in the 1 kHz road-support query; removing the
suspension-axis component would erase the mechanism that unloads the inside
rear tire.

The rear wheel centres are fixed to one authored axle line and ignore requested
independent bump/droop. The compiler requires zero conventional suspension
travel. Tire radial compliance and `chassis_torsional_mode_v1` frame twist own
the support compliance instead. This makes steering jacking, tire compliance
and frame torsion capable of producing inside-rear unloading as physical force
paths rather than an authored lift percentage.

## Provider: `multilink_v1` (SUSP12)

SUSP12 adds a generic five-link independent wheel-carrier solver. The five
chassis-to-upright rods retain their authored rest lengths while a sixth
constraint requests wheel-centre travel along the suspension axis. A bounded
six-variable Newton solve therefore recovers the rigid upright translation and
rotation directly from hardpoint geometry. Camber, toe and wheel-centre scrub
are outputs of that solve rather than authored travel curves.

The fifth rod is the toe/steering link. Its inner pickup may translate along the
authored steering-rack axis. At zero rack displacement, ordinary travel retains
passive bump steer. For steering input the provider derives a reference rack
travel from the rest-pose toe sensitivity, moves the toe-link inner pickup, then
resolves all five link lengths at the requested wheel travel.

Separate spring and damper chassis/upright mounts produce actual shaft
compression and independent instantaneous motion ratios. SUSP05 remains the
sole bridge from solved wheel-centre motion to the physical tire-support query.

## Providers: `semi_trailing_arm_v1` and `twist_beam_v1` (SUSP13)

`semi_trailing_arm_v1` rotates one rigid arm around an arbitrary swept pivot
axis. Wheel-centre path, camber migration, passive toe/bump-steer and lateral /
longitudinal scrub therefore come directly from the authored pivot geometry.
Separate spring and damper lower eyes rotate with the arm and expose independent
instantaneous motion ratios.

`twist_beam_v1` evaluates a paired left/right semi-trailing-arm mechanism and
uses relative arm rotation as the crossbeam torsion coordinate. Mirrored arm
axes are transformed into a common physical beam-axis convention, so symmetric
bump does not create fictitious beam twist. Split travel produces real relative
twist and twist rate. SUSP05 applies the selected arm wheel-centre scrub to the
physical support query exactly as for the other hardpoint providers.
