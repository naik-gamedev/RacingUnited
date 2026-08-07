# ADR-016: Scalar Unsprung Mass and Radial Tire Compliance

Status: Accepted in Step 29O.

## Context

The massless raycast wheel transfers the suspension force directly to the
chassis. It cannot store vertical wheel/upright momentum, so it cannot reproduce
wheel hop or the second spring/damper formed by the tire carcass. Creating a
free rigid body and constraints for every wheel would add cost and instability
before authoritative suspension geometry exists, particularly for the planned
150-car fields.

## Decision

Add an optional native one-degree-of-freedom unsprung-mass provider. Each wheel
may carry an effective mass constrained to its suspension axis and a bounded
radial tire spring/damper. The provider runs inside the existing high-rate
vehicle step and supplies authoritative hub travel, velocity and normal load.

Effective mass zero preserves the historical massless path. Vehicle definitions
and live APIs own physical values; vehicle categories do not choose equations.
The provider remains separate from `SuspensionModel` so later linkage geometry,
coupled axle and rigid upright providers can reuse or supersede it cleanly.

## Consequences

- Wheel hop and tire radial compliance now exist in native simulation.
- The feature costs only a small scalar state per enabled contact unit.
- Large fields can select fidelity per vehicle or wheel without maintaining a
  separate arcade vehicle implementation.
- This is not yet a free upright rigid body and does not calculate camber, toe,
  scrub, caster, bump steer, axle coupling or linkage loads.
- Geometry providers and authoritative upright pose remain the next suspension
  milestone; anti-roll and cross-linked systems follow those foundations.

