# ADR-034 — Wheel hub datums and steering-ground geometry

## Status
Accepted for FITMENT02.

## Context
Wheel fitment cannot safely treat an arbitrary render-object origin as all of the
following at once: hub mounting face, wheel centerline and wheel spin axis. ET,
spacers, scrub radius and clearance depend on those meanings being distinct.
Creator assets may also be provisional in detailed mesh shape while carrying
accurate engineering metadata.

## Decision
Heritage separates the immutable reference vehicle from the installed setup.

For a conventional left/right wheel, the reference package resolves:

- `reference wheel centerline` — the neutral wheel center used by vehicle geometry;
- `hub_face_center` — chassis-side wheel mounting-face datum;
- `installed mounting face` — hub face shifted outward by spacer thickness;
- `installed wheel centerline` — installed mounting face shifted inward by the
  installed positive ET;
- `wheel_spin_axis` — optional explicit asset datum/orientation for future
  high-fidelity wheel/hub authoring.

Positive ET means the wheel mounting face lies toward the outside of the vehicle
relative to the wheel centerline. A spacer moves the installed mounting plane
outward. Neither operation moves suspension or steering hardpoints.

When explicit GLB datums are authored, use either semantic extras:

- `heritage.part_type = "wheel_fitment_datum"`
- `heritage.corner = "front_left" | "front_right" | "rear_left" | "rear_right"`
- `heritage.datum_role = "hub_face_center" | "wheel_centerline" | "wheel_spin_axis"`

or the stable node-name aliases:

- `FIT_FL_HubFace`, `FIT_FR_HubFace`, `FIT_RL_HubFace`, `FIT_RR_HubFace`
- `FIT_FL_WheelCenterline`, etc.
- `FIT_FL_SpinAxis`, etc.

A spin-axis datum uses the node's local +X direction after the normal glTF basis
conversion. Arbitrary `WH_*` object origins are **not** promoted to engineering
datums merely because a mesh happens to rotate around them.

Until a vehicle supplies explicit fitment datums, Heritage may derive the
reference hub face from an already-authoritative reference wheel center plus
explicit ET metadata. Provenance must remain honest about that derivation.

For steering-ground diagnostics, a suspension provider exposes a point and
direction on its current steering axis. Heritage intersects that axis with the
current contact plane and reports:

- signed scrub radius along the wheel-right direction;
- scrub-radius magnitude;
- mechanical trail, positive when the steering-axis ground intersection lies
  ahead of the contact patch in the current wheel-forward direction.

MacPherson uses its current lower ball joint as the steering-axis point. Virtual
legacy providers use their reference wheel/mount point. The calculation uses the
live road contact plane, so steering, suspension travel, chassis attitude and
installed wheel offset can all alter the diagnostic.

## Consequences
Changing ET or adding a spacer can change installed track and scrub radius while
Ackermann/suspension reference geometry stays fixed. FITMENT02 can also report the
nominal tire inboard/outboard envelope relative to the reference hub face without
claiming that a provisional render mesh is an exact clearance surface.

Exact strut/body interference, installed component mass/inertia and a finite-width
contact patch remain later mechanisms.
