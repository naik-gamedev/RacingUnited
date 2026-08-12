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

`motionRatio` is damper/spring shaft travel divided by wheel travel. Spring and
damper rates therefore reach the wheel through the square of the motion ratio;
preload reaches it through one factor. Progressive spring terms include the
additional shaft-displacement factor. This makes the authoring value useful for
pushrod, pullrod and unequal-lever layouts even before their geometry providers
derive the ratio dynamically.

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
