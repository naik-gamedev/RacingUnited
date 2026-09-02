# SUSP13 — Semi-Trailing Arm and Twist-Beam Suspension

SUSP13 closes the common older-European and front-wheel-drive semi-independent
rear-suspension family with two related native providers.

## `semi_trailing_arm_v1`

A rigid arm rotates about an arbitrary authored pivot axis. Because that axis may
be swept in plan and elevation, wheel-centre scrub, camber migration and passive
toe change emerge from the hardpoint geometry rather than authored curves.
Separate chassis-fixed spring/damper upper mounts and arm-fixed lower mounts
supply actual shaft compression and instantaneous leverage through travel.

Required seven-point package:

- `arm_pivot_inner`
- `arm_pivot_outer`
- `wheel_center`
- `spring_upper_mount`
- `spring_lower_mount`
- `damper_upper_mount`
- `damper_lower_mount`

## `twist_beam_v1`

A twist beam is not represented as two independent trailing arms plus a generic
anti-roll bar. SUSP13 evaluates a left and right semi-trailing arm and joins them
with one torsionally compliant crossbeam. Relative arm rotation is the beam twist
coordinate. Relative arm angular velocity supplies beam twist rate. Torsional
stiffness/damping are converted back to each wheel through the signed derivative
of relative beam twist with respect to that wheel's compression coordinate.

Mirrored left/right arm axes are normalized into the same physical beam-axis
convention, so equal two-wheel bump produces zero beam twist while split travel
twists the beam and produces equal-purpose/opposed coupling forces.

Required sixteen-point package: the seven semi-trailing-arm points for each side
using `left_` / `right_` prefixes, plus `beam_left_attachment` and
`beam_right_attachment`.

SUSP05 remains the only bridge from linkage-derived wheel-centre scrub to the
1 kHz tire-support query. Spring/damper constitutive behavior remains owned by the
shared nonlinear suspension model. Generic anti-roll bars remain a separate
mechanism for vehicles that physically have one.
