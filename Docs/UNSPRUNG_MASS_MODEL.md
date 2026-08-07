# Scalar Unsprung-Mass and Radial-Tire Model

## Purpose

Step 29O adds a bounded one-degree-of-freedom wheel/upright state to each
contact unit. The state moves only along the suspension axis. This introduces
the essential vertical interaction between chassis spring/damper force,
unsprung inertia and radial tire compliance without creating a freely moving
rigid body for every wheel.

The model is deliberately a scalable middle layer. It can represent wheel hop
and tire carcass compression for the player and nearby high-fidelity vehicles,
while a zero effective mass selects the older massless raycast-wheel path for
large fields or compatibility.

## Authored parameters

Each contact unit owns:

- effective unsprung mass in kilograms;
- radial tire stiffness in newtons per metre;
- radial tire damping in newton-seconds per metre;
- maximum radial tire deflection; and
- maximum normal load.

These are physical authoring values, not vehicle-category switches. A future
MacPherson, wishbone, trailing-arm, live-axle, pushrod, kart or motorcycle
geometry provider may all feed the same scalar force/inertia layer when that
is the appropriate fidelity. More complex layouts may replace it with coupled
or full rigid-body upright providers without changing the definition graph.

## Solver contract

`UnsprungMassModel` receives suspension-axis travel bounds, the suspension-link
force, the current road-to-hub target and its velocity, and the road-normal
alignment. It integrates wheel/upright velocity and displacement at the native
vehicle substep rate, evaluates radial tire spring/damper load, clamps travel
and tire deformation to authored safety bounds, and returns authoritative
wheel-center motion and contact load.

The current implementation is reduced order:

- motion perpendicular to the suspension axis is constrained;
- the chassis rigid body continues to own the vehicle's externally simulated
  mass and inertia;
- unsprung mass is an internal inertial state and is not a collision body;
- lateral/longitudinal tire forces still act through the chassis contact path;
- there is no upright collision, linkage compliance or axle-to-axle coupling.

Those limits are intentional. They keep the cost predictable for 1000 Hz
vehicle physics and large grids while establishing the provider boundary needed
for later geometry and coupling systems.

## Telemetry and tuning

`Vehicle.GetWheelState` appends unsprung velocity, tire deflection, tire
deflection velocity and radial tire dissipation. Dynamics Lab records and
exports all four channels and summarizes peak wheel-hop velocity, peak tire
deflection and peak radial damping power.

The Vehicle `SUSP. > UNSPRUNG` page edits the five native parameters per wheel.
Changes use atomic native set/readback calls, may be copied to every wheel and
may be restored from the prototype definition. Setting effective unsprung mass
to zero intentionally returns that wheel to the compatibility path.

## Validation

The native regression suite covers static radial deflection, a 20 mm road step,
damped wheel-hop reversals and convergence, invalid definition rejection, live
set/readback, 1000 Hz Dynamics Lab capture and all existing parked, slope,
drop, braking and rate-comparison scenarios.

