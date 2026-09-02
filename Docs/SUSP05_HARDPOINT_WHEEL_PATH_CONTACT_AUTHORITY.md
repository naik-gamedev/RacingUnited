# SUSP05 — Hardpoint Wheel-Path Contact Authority

## Problem

Heritage already solved mechanism-specific wheel-centre motion for
`macpherson_strut_v1` and `trailing_arm_torsion_bar_v1`, but the high-rate tire
support ray still originated from the legacy straight `localMount` line. This
meant the upright presentation and alignment could follow the linkage while the
physical tire/road query ignored the linkage's lateral and longitudinal wheel
scrub.

That violated the suspension/tire handoff contract: suspension owns wheel-centre
kinematics, while tire owns local contact and force generation.

## Decision

`SuspensionGeometry` now exposes `evaluateSuspensionSupportOffset()`.

For hardpoint providers it:

1. evaluates the mechanism-specific wheel centre at the previous 1 kHz
   compression/steering state;
2. subtracts the authored reference wheel centre;
3. removes the component along the suspension axis, because bump/droop travel
   along that axis is already solved by the ray/unsprung-mass path; and
4. supplies only the bounded lateral/longitudinal linkage offset to the next
   high-rate road-support query.

The compatibility `linear_raycast_v1` provider returns zero offset and therefore
keeps legacy behavior exactly.

The one-substep explicit predictor is intentional. At 1000 Hz it avoids a
circular dependency between road support and current linkage compression, and
matches the established bounded ordering already used by motorcycle crown
contact.

## Physical consequence

MacPherson steering scrub, lower-arm arc motion and trailing-arm fore/aft wheel
path can now move the actual road/tire support query and its chassis force lever
arm. They are no longer presentation-only geometry.

The axial suspension travel remains single-authority; SUSP05 does not add a
second vertical wheel-travel solver.

## Regression evidence

The native regression verifies:

- zero offset at authored ride height;
- finite nonzero MacPherson transverse movement in bump;
- finite nonzero steering-axis scrub movement under steer;
- left/right mirror symmetry;
- zero offset for `linear_raycast_v1`; and
- full 1000 Hz front MacPherson + rear trailing-arm vehicle stability after the
  contact-authority change.

A representative synthetic MacPherson corner produces about 5.25 mm lateral /
9.45 mm longitudinal bump-path offset at +80 mm compression and about 3.39 mm /
16.07 mm steering scrub at 20 degrees steer. These are regression geometry, not
Peugeot measurements.

## Scope boundary

SUSP05 deliberately does not activate low-confidence estimated Peugeot
hardpoints. `TIRE45D` remains authoritative: estimates are authoring evidence
only until stronger legacy-authored, asset-authored or measured coordinates are
available.

The next suspension topology milestone can now add reusable double-wishbone
kinematics without inheriting a fake straight-line tire-contact path.
