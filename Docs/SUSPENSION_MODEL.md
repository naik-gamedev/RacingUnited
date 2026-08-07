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

Those effects require linkage geometry, unsprung mass and trustworthy baseline
captures first. Until then, no random degradation belongs in the solver.
