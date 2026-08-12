# ADR-037 — Turn slip and parking contact-patch state

**Status:** Accepted at TIRE03 (2026-08-09).

## Decision

Heritage separates rolling MF6.2 turn slip from zero/near-zero-speed tread torsion.
Rolling turn slip is supplied to the Magic Formula provider as curvature-like spin in
`1/m` and uses the public `[TURNSLIP_COEFFICIENTS]` vocabulary. Parking steering is
represented by an engine-owned `TireContactPatchState` that stores elastic torsional
deformation, releases it with rolling distance, and produces a bounded QCRP1-scaled
turning moment.

The state lives per wheel and is integrated in the existing high-rate vehicle substep.
It is reset when contact is lost. This avoids singular `yawRate / Vx` behavior at zero
speed and prevents an ordinary lateral-velocity damper from being treated as a physical
model of parking torsion.

Imported PTX/PTY transient coefficients may determine relaxation lengths when valid;
otherwise explicit engineering fallback lengths remain authoritative. Turn-slip
coefficient families whose exact public equation use has not yet been validated (notably
PHYP*) are preserved but remain inactive rather than receiving guessed semantics.

## Consequences

- Steering at standstill can build and release tire rubber twist instead of requiring an
  infinite slip quantity.
- Rolling corner curvature can influence MF6.2 force, stiffness, trail and aligning moment.
- Low-speed and high-speed tire mechanisms stay independently testable.
- Future brush/contact-mass, rigid-ring and road-enveloping models can replace individual
  layers without changing the vehicle API.
- Heritage does not claim proprietary Simcenter solver parity from coefficient names alone.
