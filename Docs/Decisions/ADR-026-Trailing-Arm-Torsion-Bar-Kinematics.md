# ADR-026: Trailing-Arm + Torsion-Bar Rear Suspension Provider

**Status:** Accepted  
**Date:** 2026-08-09

## Decision

Heritage Engine implements `trailing_arm_torsion_bar_v1` as a reusable
mechanism-specific rear suspension provider rather than a Peugeot-specific
solver.

Each corner owns five chassis-local reference points:

- `arm_pivot_inner`
- `arm_pivot_outer`
- `wheel_center`
- `damper_upper_mount`
- `damper_lower_mount`

The trailing arm rotates rigidly about the line through its two pivots. Wheel
centre and damper lower eye follow that arc. The transverse torsion bar is
represented as a rotational spring keyed to arm rotation, and the separate
damper uses its geometry-derived instantaneous motion ratio.

When direct torsion-bar dimensions/rate data are unavailable, the current
provider converts the familiar authored reference wheel rate into equivalent
torsional stiffness at the reference angular leverage. This preserves a useful,
creator-friendly tuning contract while the spring itself is evaluated in
rotation rather than as a fictitious coil spring at the wheel.

## Assisted authoring

`estimated_trailing_arm_torsion_bar_road_v1` may provide a deterministic,
low-confidence five-point starting package from wheel-centre position and the
chassis reference-package scale. It is explicitly an estimate and may be
replaced point-by-point by GLB-authored or measured data.

## Limits

This provider intentionally does not own left/right anti-roll coupling, bushing
compliance, structural flex or full multibody link loads. SUS04 implements
anti-roll coupling as the separate reusable mechanism defined by ADR-027.
