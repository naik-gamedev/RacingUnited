# ADR-028: Body Reference Origin vs Physical Center of Mass

**Status:** Accepted  
**Date:** 2026-08-09

## Decision

Heritage Engine treats an authored rigid-body/entity origin and the body's
physical center of mass (COM) as two different concepts.

`RigidBodyPose.position` remains the creator-facing/reference origin used by
entities, colliders, wheel mounts, suspension hardpoints, visual children and
asset authoring. A dynamic body may additionally define
`centerOfMassLocal`, expressed in that same body-local frame.

The physics core uses the physical COM for:

- linear-velocity state;
- impulse and force lever arms;
- contact-point velocity;
- constraint-point velocity;
- collider-derived inertia and parallel-axis offsets; and
- rotational integration.

When a body rotates around an offset COM, the authored reference origin is
allowed to move around the COM. The COM itself follows the body's linear
translation. Existing bodies with a zero COM offset retain the previous
behavior exactly.

## Vehicle consequence

Vehicle suspension mounts, hardpoints and wheel reference coordinates remain
anchored to the authored chassis datum. A wheel/tire swap, visual mesh change
or suspension-authoring edit therefore cannot silently move the physical COM.
Likewise, changing the COM does not rewrite authored geometry.

Racing United's current Peugeot-oriented prototype uses a deliberately
low-confidence estimated COM of `{0.0, 0.52, 0.20}` m in chassis-local
coordinates. Its provenance is `estimated_compact_fwd_hatch_v1` with confidence
0.20. This is a useful starting estimate, not claimed Peugeot factory data.
Better measured, documented, reconstructed or component-derived mass data may
replace it later without changing the solver contract.

## Why this is required for body roll

Tire forces already act at their contact points. If the body reference origin
near the road plane is incorrectly treated as the COM, lateral tire force has
little vertical lever arm and therefore creates little roll torque. It also
causes collider inertia to be evaluated about the wrong point, artificially
inflating roll inertia.

Separating the physical COM restores the intended force path: lateral contact
force generates chassis torque around the elevated COM, the chassis rotates,
left/right suspension travel and tire load diverge, and springs/dampers/anti-roll
bars resist that motion. No visual-only body-roll animation is required.

## Safety contract

Headless regressions require:

1. an offset-COM rigid body to acquire angular velocity from an off-COM impulse
   while preserving world-COM position during pure rotation; and
2. a complete vehicle driven through a bounded turn to exhibit non-zero chassis
   roll and left/right load transfer without numerical instability.

