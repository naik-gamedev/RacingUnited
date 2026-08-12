# Wheel Fitment and Alignment

## FITMENT01 scope

FITMENT01 separates the **reference vehicle assembly** from the **current setup**.
The authored vehicle/GLB describes the neutral mechanical reference. The setup
layer may then install different wheels, tires, spacers and alignment values
without rewriting chassis or suspension geometry.

This is deliberately compatible with creator-built vehicles whose reference
wheels are modeled at zero camber/toe. Racing or road alignment belongs to the
setup, not to destructive edits of the source asset.

## Authority hierarchy

Use the strongest available evidence for each quantity:

1. Explicit measured/factory data or intentionally authored engineering metadata.
2. Explicitly semantic authoring nodes/transforms whose origin role is defined by
   the Heritage authoring contract.
3. Creator-authored reference geometry where its meaning is unambiguous.
4. Versioned Heritage estimates with provenance/confidence when stronger evidence
   is unavailable.

The current Peugeot prototype trusts its explicit 17x7, ET28 and 205/40 R17
technical metadata. Its detailed provisional wheel/tire mesh shape is **not**
claimed to be factory-accurate and may be replaced by a better model later.

## Reference geometry versus setup

Reference/chassis data includes:

- suspension hardpoints;
- steering-rack/tie-rod hardpoints;
- reference upright/hub placement;
- authored wheel-center layout when its semantic origin is known;
- wheelbase and track reference geometry;
- reference wheel/tire engineering metadata.

Setup data includes, per corner:

- installed wheel offset / ET;
- spacer thickness;
- camber;
- toe;
- caster override when the suspension architecture permits it.

Changing setup data must never relocate suspension hardpoints or silently
regenerate assisted suspension geometry.

## Per-corner setup and linking

Every wheel owns an independent setup. The Racing United Workshop may optionally
link the two front corners and/or the two rear corners for normal symmetric road
or circuit setup work. Disabling the links allows asymmetric setups such as oval
racing, damage compensation or unusual historical vehicles.

Human-facing alignment signs are:

- camber: negative = top of tire inward;
- toe: positive = toe-in, negative = toe-out;
- caster: positive = normal rearward steering-axis inclination in side view.

The Lua setup layer converts those friendly symmetric signs to the native
per-corner local coordinate convention.

## ALIGN01 factory specification evidence and exact tuning

Factory alignment evidence is separate from the current setup. The Peugeot 206 RC
reference currently uses a user-supplied MIN/MAX specification table with these
ranges (degrees):

- front total toe: -0.20 to -0.03;
- front per-wheel toe: -0.10 to -0.02;
- front camber: -0.50 to +0.50;
- rear total toe: +0.43 to +0.60;
- rear per-wheel toe: +0.22 to +0.30;
- rear camber: -1.50 to -0.50;
- caster: +2.70 to +3.70;
- steering-axis inclination: +9.20 to +10.20.

The supplied table has no populated standard/nominal column. Heritage therefore
uses midpoint values only as explicitly labelled workshop defaults: front camber
0.00, front toe -0.06 per wheel, caster +3.20, rear camber -1.00 and rear toe
+0.26 per wheel. SAI reference remains +9.70. These are derived defaults, not
claims of exact factory targets.

The Workshop shows the factory range and whether the current corner/axle remains
inside it, but never clamps custom setup to the factory envelope. Normal alignment
sliders snap to 0.01 degree increments; a visible exact numeric field accepts
finer typed values (for example +0.82 or -2.347) within the broader engine-safe
ranges. Advanced mode exposes the broad slider ranges as well.

See ADR-033.

## Current native effects

FITMENT01 implements the following physics effects:

- tire size resolves a nominal unloaded tire radius used by the wheel contact
  unit;
- installed ET and spacer thickness move the **installed tire/wheel centerline**
  laterally relative to the reference upright/hub;
- the suspension/raycast attachment and chassis hardpoints stay fixed;
- Ackermann/reference steering geometry continues to use the reference suspension
  mount rather than the displaced installed tire centerline;
- camber and toe modify the current upright/tire orientation per corner;
- caster can be overridden by setup without rewriting the authored hardpoints;
- leaving caster at its reference value keeps the native hardpoint-derived caster
  path unchanged.

This separation is regression-tested: a +23 mm outward displacement on each front
wheel increases installed front tire-center track by 46 mm while the reference
Ackermann steering track remains unchanged.

## Wheel-node origin semantics

Heritage **does not assume that every `WH_*` GLB node origin is the tire
centerline**. A creator may reasonably place an object origin at the hub mounting
face, wheel rotational pivot, geometric centerline or another engineering datum.
Treating all of those as the same thing would create track/ET errors.

FITMENT02 therefore defines explicit semantic datums: `hub_face_center`,
`wheel_centerline` and `wheel_spin_axis`. Only nodes carrying one of those declared
roles (or a stable `FIT_*` alias) are promoted as authoritative fitment datums.
Arbitrary wheel-mesh origins remain non-authoritative. The current provisional
Peugeot asset does not yet contain these explicit datums, so its technical metadata
and established reference layout remain the stronger evidence.

## Intentionally future work

FITMENT01 established the reference/setup boundary and FITMENT02 added explicit hub
datums plus scrub-radius/mechanical-trail geometry. Later fitment milestones should
add independently reusable mechanisms for:

- finite tire contact-patch width/shape;
- wheel/tire/spacer mass and rotational inertia feeding the MASS01 accumulator;
- unsprung-mass changes;
- bearing/steering loads from large offsets;
- bump/steer/lock clearance and interference against strut, arms and bodywork;
- replacement wheel/tire visual assets;
- hub PCD, center-bore, fastener and tire/rim compatibility checks.

Those systems must consume the same reference/setup model rather than relocating
chassis suspension geometry to make an aftermarket part fit.

## FITMENT02 — hub datums, tire envelope, scrub radius and trail

FITMENT02 promotes two previously empty fitment scaffolds into compiled reusable
mechanisms:

- `HubReferenceGeometry` resolves the reference wheel centerline, reference hub
  mounting face, spacer-shifted installed mounting plane, installed wheel
  centerline and nominal inboard/outboard tire planes;
- `ScrubRadiusGeometry` intersects the current steering axis with the live road
  contact plane and reports signed scrub radius, magnitude and mechanical trail.

The reference geometry remains immutable. For positive ET, the hub mounting face
is outward of the reference wheel centerline. The installed centerline is then
resolved from the installed ET plus any spacer thickness. This is the same
relationship used by FITMENT01's track-change regression, but the individual
engineering datums are now explicit and inspectable.

The current Peugeot asset does not yet contain explicit wheel-fitment datum nodes.
Therefore Heritage continues to trust the established reference wheel centers and
technical ET/tire metadata for this provisional asset. A future high-accuracy GLB
may author explicit nodes using either semantic extras or these stable aliases:

```text
FIT_FL_HubFace
FIT_FL_WheelCenterline
FIT_FL_SpinAxis

FIT_FR_...
FIT_RL_...
FIT_RR_...
```

Semantic extras use `heritage.part_type=wheel_fitment_datum`, `heritage.corner`
and `heritage.datum_role`. Valid roles are `hub_face_center`, `wheel_centerline`
and `wheel_spin_axis`. A spin-axis node's local +X is the authored axis direction.
Heritage intentionally does not guess these meanings from arbitrary `WH_*` mesh
origins.

`Vehicle.GetWheelFitmentGeometry` exposes the resolved hub/tire envelope plus
live steering-ground diagnostics to Lua/Workshop tooling. This makes an ET/spacer
change observable as a real scrub-radius change while suspension hardpoints and
Ackermann reference track remain unchanged.

The tire envelope in FITMENT02 uses the authored/metadata tire width as a nominal
lateral envelope. It is not yet an exact body/strut interference test and does not
pretend the current provisional Peugeot tire mesh is factory CAD. Exact clearance,
component mass/inertia and finite-width contact-patch behavior remain later work.

See ADR-034.
