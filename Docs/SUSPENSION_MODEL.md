# Native Suspension Model

## Current healthy model

Step 29M expands `linear_raycast_v1` from a single linear spring/damper equation
into a bounded non-linear force provider. It supports:

- spring preload;
- linear spring rate plus quadratic progression;
- separate low- and high-speed bump damping with a velocity knee;
- separate low- and high-speed rebound damping with a velocity knee;
- progressive bump-stop force;
- droop/rebound-stop force;
- suspension motion ratio; and
- an absolute normal-force ceiling.

All inputs are evaluated inside the native high-rate vehicle step. The provider
returns spring, damper, bump-stop, droop-stop, unclamped, and final normal-force
components plus instantaneous damper energy dissipation in watts.

Positive compression and compression velocity are bump. Negative values are
rebound/droop. The ground-contact path cannot pull the road toward the tire, so
the final contact-normal force is clamped to zero even when the internal spring,
damper, and droop-stop sum is negative. The unclamped force remains available
for diagnostics and future constrained unsprung-mass providers.

## Motion ratio

`motionRatio` is damper/spring shaft travel divided by wheel travel for the
legacy/direct-acting force contract. With a constant ratio, spring and damper
rates reach the wheel through the square of that ratio; preload reaches it
through one factor. Progressive spring terms include the additional
shaft-displacement factor.

Mechanisms with geometry-derived *variable* leverage must not approximate shaft
displacement as `wheel travel * current ratio`. SUSP07 therefore supplies actual
spring shaft compression plus independent spring/damper derivatives from the
rocker geometry. The authored scalar remains a compatibility/default value for
providers that do not expose those generalized coordinates.

## Torsion-bar springing (SUS03B)

`trailing_arm_torsion_bar_v1` keeps spring and damper leverage separate. The
trailing-arm geometry provider supplies torsion-bar twist, instantaneous angular
motion ratio (radians per metre of wheel travel), and separate-damper motion
ratio.

The current `TorsionBar` helper accepts the same creator-friendly reference wheel
preload/rate/progression fields used elsewhere, converts them to equivalent
rotational preload torque and torsional stiffness at the reference arm leverage,
then evaluates spring torque from actual arm twist. Wheel spring force is obtained
through the current angular leverage. This is a rotational spring model; it does
not pretend the rear torsion bar is a coil spring acting directly at the wheel.

When better physical data becomes available, direct torsion-bar dimensions or
measured torsional stiffness can extend this mechanism without changing the
trailing-arm kinematics contract.

## Telemetry

`Vehicle.GetWheelState` appends the following values after its existing surface
data, preserving the older return order:

- spring force;
- damping force;
- bump-stop force;
- droop-stop force;
- unclamped suspension force; and
- damper dissipation watts.

The Dynamics Lab records those values at the requested native capture rate,
offers force/power plots and writes them to CSV. Peak travel-stop force and peak
damper dissipation are part of the capture summary.

## Live per-wheel tuning

Step 29N adds atomic native set/readback APIs for the complete healthy force
description. The Racing United Vehicle `SUSP.` tab selects an individual wheel,
edits spring, damper, motion-ratio and travel-stop parameters live, and can copy
that tune to every wheel. Its `LIVE` page displays the force breakdown and
damper watts from the 1000 Hz solver.

The ordinary prototype creation path now supplies the same nonlinear fields as
the versioned Workshop definition. Older modules that call `Vehicle.AddWheel`
with the original argument list retain linear-compatible defaults.

## Unsprung mass and tire compliance

Step 29O adds an optional scalar wheel/upright inertia after the suspension
force provider. When enabled, the suspension link accelerates that mass and a
radial tire spring/damper generates road-normal load. Consequently the hub is
no longer forced to follow the road ray exactly: tire deflection and wheel hop
are simulated at the 1000 Hz vehicle rate.

This scalar state is common infrastructure, not a MacPherson or wishbone
solver. Future geometry providers will determine authoritative wheel paths,
motion ratios, camber, toe and upright pose while continuing to use this
force/inertia layer where appropriate. See `UNSPRUNG_MASS_MODEL.md` and ADR-016.

## Upright geometry

Step 29P adds that geometry boundary without pretending the first curve
provider is a complete linkage. Each contact now owns a local 3D steering axis
and signed quadratic camber/toe curves versus wheel compression. Native code
composes those values with Ackermann steering into one orthonormal upright pose
used by tire direction, telemetry and articulated wheel presentation.

The curve provider can fit measured alignment traces while Workshop hardpoint
authoring is built. MacPherson, double-wishbone and later layout providers must
produce the same output contract. See `SUSPENSION_GEOMETRY.md` and ADR-017.

## Damage and wear ordering

Damage is intentionally not simulated yet. A defensible later model can build
on the healthy telemetry in this order:

1. Integrate damper dissipation over time into oil/gas temperature.
2. Derive temporary viscosity fade and pressure/cavitation effects.
3. Accumulate seal and bushing wear from temperature, shaft velocity, travel,
   contamination and load cycles.
4. Apply impact failures from bump/droop-stop energy and force thresholds.
5. Represent bent rods, arms and hardpoints as changed geometry, friction,
   travel, camber, toe and motion ratio—not a generic health percentage.
6. Represent spring fatigue, sag or fracture through preload/rate/free-length
   changes.

Those effects require linkage geometry and trustworthy baseline captures first.
Unsprung inertia and radial tire energy now exist, but no random degradation
belongs in the solver until healthy geometry, loads and thermal state are
authoritative.

## Geometry-owned nonlinear rocker actuation (SUSP07)

`pushrod_double_wishbone_v1` no longer uses the constant-ratio spring shortcut.
Its geometry provider supplies actual spring shaft compression plus independent
instantaneous spring and damper motion ratios. Spring force is evaluated from
the real shaft displacement and mapped to wheel force through the current
spring ratio. Damper shaft velocity is derived with the current damper ratio and
the resulting shaft force is mapped back through that ratio.

This preserves virtual-work leverage through nonlinear bellcrank motion while
leaving the legacy/direct-acting providers behavior-compatible.


## Leaf-spring live-axle force path (SUSP09)

`live_axle_leaf_v1` uses the actual leaf generalized compression and geometry-
derived leaf motion ratio from the SUSP09 kinematic provider. The common
preload/rate/progression fields therefore calibrate effective leaf-pack bending
rather than a fictitious coil spring at the wheel. The direct shock keeps its
own SUSP08 shaft geometry and motion ratio.

Interleaf friction is evaluated separately using a smooth Coulomb term plus a
viscous term at leaf generalized velocity. Its wheel force and dissipated power
are exposed separately from shock dissipation. One paired axle-housing wind-up
state uses authorable torsional stiffness, damping and inertia; physical tire
longitudinal reaction torque excites it, and only a bounded jacking component is
fed back into vertical support.

## Motorcycle suspension force path (SUSP10)

`motorcycle_telescopic_fork_v1` is direct acting: the common spring/damper force
model consumes the actual fork compression with 1:1 shaft leverage. No generic
"anti-dive percentage" is injected into a conventional fork.

`motorcycle_swingarm_linkage_v1` consumes the actual shock shaft compression and
instantaneous motion ratio produced by the swingarm/dogbone/rocker geometry.
The same virtual-work convention as SUSP07 maps spring/damper shaft force back
to wheel support through current leverage.

Rear chain geometry adds one separate physical contribution. The kinematics
provider exposes `d(countershaft-to-axle distance)/d(wheel compression)`. The
force model combines that derivative with previous-step longitudinal tire force,
effective wheel radius and authored rear-sprocket pitch radius. This produces a
bounded chain-jacking term with the correct sign from geometry, rather than a
throttle-based anti-squat scalar. Its value is exposed separately as
`motorcycleChainJackingForceN`.


## Kart chassis force path (SUSP11)

`kart_chassis_flex_v1` intentionally returns zero generic spring, damper and
travel-stop force. A racing kart has no hidden four-corner coilover model. The
high-rate support query fixes hub length at its authored rest length, while the
pneumatic tire/structural tire model supplies radial compliance. The resulting
tire normal force is transmitted directly to the chassis.

Frame compliance remains owned by the reusable `chassis_torsional_mode_v1`
solver. VehicleDefinition therefore rejects a kart provider unless that chassis
torsional mode is enabled, and rejects nonzero authored bump/droop travel. Front
kingpin steering jacking enters through geometry/SUSP05 rather than as a force
scalar, so diagonal load transfer and inside-rear unloading emerge from the
combined chassis, tire and steering geometry.

## Five-link generalized force path (SUSP12)

`multilink_v1` supplies actual spring and damper shaft coordinates from the
solved rigid-upright geometry. The common nonlinear spring/damper model therefore
uses `springCompressionM`, `springMotionRatio` and `damperMotionRatio` exactly as
the SUSP07/08/10 generalized mechanisms do. This avoids a constant motion-ratio
approximation while keeping spring/damper constitutive behavior reusable.

## Semi-trailing / twist-beam generalized force path (SUSP13)

Both SUSP13 providers use actual geometry-derived spring and damper coordinates.
For `twist_beam_v1`, crossbeam torque is
`K_beam * relativeArmAngle + C_beam * relativeArmAngularVelocity`. The wheel
coupling force follows virtual work by multiplying that torque by the signed
instantaneous derivative of relative beam twist with respect to the selected
wheel's compression. Beam damping dissipation is tracked independently from the
shock absorber. A generic anti-roll bar is not substituted for this structural
beam mechanism.
