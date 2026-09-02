# SUSP07 — Pushrod/Rocker Double-Wishbone Suspension

SUSP07 adds `pushrod_double_wishbone_v1` without cloning the SUSP06 wheel
kinematics. Upper/lower A-arms, the rigid upright, steering axis, tie rod, bump
steer, camber/caster/KPI and SUSP05 wheel-centre contact scrub remain owned by
the reusable SUSP06 double-wishbone linkage.

## Actuation chain

The new provider adds eight actuation hardpoints around that linkage:

- `pushrod_lower_arm_mount`
- `rocker_pivot_front`
- `rocker_pivot_rear`
- `rocker_pushrod_mount`
- `spring_chassis_mount`
- `spring_rocker_mount`
- `damper_chassis_mount`
- `damper_rocker_mount`

Together with the nine linkage hardpoints, a complete provider therefore needs
17 named points. The pushrod outer point is lower-arm fixed. The rocker rotates
around the axis through its two chassis pivots.

For every requested wheel position the solver first evaluates the SUSP06 linkage,
then rotates the lower-arm pushrod pickup with the arm. Rocker angle is solved
analytically from the rigid pushrod-length constraint. This is not a lookup curve
and not `wheelTravel * ratio`.

## Nonlinear spring/damper leverage

Spring and damper can occupy different rocker radii and different chassis
orientations. Their actual shaft lengths are therefore evaluated independently.
Central derivatives of those physical shaft compressions versus wheel travel
produce separate instantaneous spring and damper motion ratios.

`SuspensionModel` now consumes the actual SUSP07 spring shaft compression for
spring force, then maps shaft force back to the wheel through the instantaneous
spring ratio. Damper shaft velocity is wheel velocity multiplied by the current
damper ratio and its shaft force is mapped back through that same ratio. This is
the virtual-work-consistent formulation required for rising/falling-rate rocker
geometry.

Bump and droop stops remain wheel-travel elements in the common force provider.
No second wheel path or tire-contact authority is introduced.

## Validation

The native regression uses a synthetic Formula-style corner and verifies:

- rigid pushrod length through bump/droop;
- nonlinear rocker angle;
- different spring and damper motion ratios;
- positive spring/damper compression in bump and extension in droop;
- unchanged SUSP06 wheel/upright geometry;
- left/right mirroring;
- SUSP05 physical wheel-centre scrub;
- exact spring and damper wheel-force mapping from the live geometry; and
- VehicleDefinition compile/load/readback plus incomplete-package rejection.

Pullrod suspension is intentionally still a distinct future provider because its
outboard attachment/rocker packaging should not be represented by renaming a
pushrod.
