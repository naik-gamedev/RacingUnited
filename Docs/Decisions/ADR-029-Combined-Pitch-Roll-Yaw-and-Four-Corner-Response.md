# ADR-029 — Combined Pitch/Roll/Yaw and Four-Corner Response

**Status:** Accepted  
**Date:** 2026-08-09

## Context

Heritage Engine now applies tire forces at road contact points and separates the
authored chassis origin from the physical center of mass. That gives the general
rigid body the torque arms needed for real pitch, roll and yaw. Before adding
chassis structural compliance, the rigid-chassis vehicle path needs an explicit
contract proving that all three rotations and independent four-corner suspension
response can coexist in one manoeuvre.

## Decision

Pitch, yaw and roll remain ordinary simultaneous rigid-body rotations. Heritage
will not add a special "diagonal" rotation mode. Diagonal-looking chassis motion
is the composition of pitch and roll (and potentially yaw); diagonal wheel
loading is measured from the independent corner loads/travel.

The native regression suite must include a bounded combined manoeuvre using the
real vehicle loop. It must demonstrate:

- non-zero pitch, roll and yaw response in the same run;
- longitudinal and lateral load transfer;
- non-uniform four-corner suspension compression/load;
- active damper response;
- active independent anti-roll-bar response; and
- stable contact without requiring all four wheels to remain grounded for every
  future high-performance setup.

Creator/debug telemetry should use physical names (`pitch`, `yaw`, `roll`) rather
than exposing only raw X/Y/Z labels and should make axle/side/diagonal loading
visible.

## Consequences

ROLL02 changes no tire-force law and adds no visual-only chassis animation. It
locks down behavior already expected from the six-degree-of-freedom solver and
makes future regressions obvious. Chassis torsional compliance is a separate
structural mechanism and may be added only after this rigid baseline remains
green.
