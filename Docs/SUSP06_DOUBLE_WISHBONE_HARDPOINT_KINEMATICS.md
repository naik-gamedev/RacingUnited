# SUSP06 — Double-Wishbone Hardpoint Kinematics

SUSP06 adds `double_wishbone_v1` as a reusable native suspension provider on top
of the SUSP05 wheel-path contact-authority contract.

## Mechanism

Each corner owns eleven chassis-local reference hardpoints:

- upper arm inner front / rear;
- upper ball joint;
- lower arm inner front / rear;
- lower ball joint;
- tie-rod inner / outer;
- wheel centre;
- damper upper / lower mounts.

The upper and lower wishbones are rigid triangles rotating about their authored
inner-pivot axes. For requested suspension travel, the solver resolves both arm
angles simultaneously against two constraints:

1. the upright distance between the upper and lower ball joints remains rigid;
2. the wheel centre moves by the requested compression along the authored
   suspension direction.

The resulting ball-joint line is the instantaneous physical steering axis. The
rigid upright carries the wheel centre and tie-rod outer point with it.

## Steering and alignment

Tie-rod length is preserved to derive passive bump steer. Commanded steering is
then applied about the current upper/lower-ball-joint axis. The provider reports
physical camber, toe, caster, kingpin inclination, upright basis/rotation and
wheel-centre path.

SUSP05 remains the sole route by which lateral/longitudinal wheel-centre scrub
moves the high-rate tire support query, so this provider does not create a
second contact solver.

## Spring and damper leverage

The damper upper mount is chassis-fixed and the lower mount is lower-arm-fixed.
Damper compression and its instantaneous wheel motion ratio are therefore
computed from hardpoint geometry. The same direct-acting ratio drives the
current spring/damper force model for `double_wishbone_v1`.

Pushrod/rocker suspension remains a later provider because its spring/damper
motion is intentionally indirect and must not be approximated as a direct
wishbone damper.

## Authority and provenance

The provider is solver-ready only with all eleven valid hardpoints. Estimated or
measured provenance remains a data-authoring concern; this milestone does not
promote low-confidence Peugeot hardpoints or invent double-wishbone geometry for
a vehicle that does not use it.

## Regression

Native regression checks:

- bump and droop wheel-centre travel;
- unequal-length-arm camber gain;
- passive bump steer;
- left/right mirroring;
- caster/KPI output;
- geometry-derived damper motion ratio;
- SUSP05 transverse support-query scrub;
- native VehicleDefinition compiler/loader acceptance;
- rejection of incomplete eleven-hardpoint definitions.
