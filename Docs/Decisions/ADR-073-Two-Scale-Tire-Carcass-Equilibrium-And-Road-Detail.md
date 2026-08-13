# ADR-073 — Two-Scale Tire Carcass Equilibrium And Road Detail

**Status:** Superseded by ADR-074 / TIRE41 (2026-08-12)

This document is retained only as the failure history for the removed layered
presentation path. None of the equilibrium, residual, probe, collider, bulge or
profile composition described below remains live deformation authority.

## Context

The live tire path combines authoritative native wheel state with a bottom-biased
21x13 CollisionSystem probe lattice. The previous implementation put ordinary
radial load and irregular obstacle compression into one scalar field, relaxed that
field, and applied it only at mesh vertices sampling a non-zero probe value.

That produced the wrong structural shape: a few vertices near the footprint could
move deeply while most of the lower tread and sidewall remained nearly circular.
Broadening rigid-ring shear did not solve normal-load deformation because the
shader explicitly disabled its analytic radial-deflection mode whenever the probe
grid was valid. The grid is valid during normal driving, so this disabled the very
mode intended to form a smooth loaded carcass.

## Decision

Tire presentation uses two coupled spatial scales with distinct authority:

1. **Pneumatic equilibrium mode.** Native radial deflection, free radius and finite
   contact-patch dimensions form a broad, gravity/contact-normal-anchored loaded
   shape. The footprint tread flattens, the complete lower belt and sidewall rise
   smoothly toward the rim, the sidewall bulges outward, and the bead remains
   anchored. This reduced-order mode stays active whether or not detailed probes
   are valid.
2. **Irregular road-detail mode.** The 21x13 collision field is decomposed into the
   equilibrium footprint plus compression above that footprint. Only this residual
   is spatially relaxed and applied as a local constraint for kerbs, rocks, steps
   and broken road. Adding equilibrium and residual preserves the deeper of the
   native footprint and direct collision sample without applying ordinary vehicle
   weight twice.

The main and shadow vertex paths implement the same two-scale position deformation.
The source tire mesh, rim, brake and vehicle collision shapes remain unchanged.
Physics continues to own forces, deflection, contact-patch dimensions and rigid-ring
state; the GPU only maps that compact state onto visible mesh vertices.

## Invariants

- A valid detailed probe grid must never disable native radial-deflection shaping.
- Flat-road load must not be applied both as broad equilibrium and as a local dent.
- Direct obstacle compression remains a lower bound before geometric capacity and
  non-inversion limits.
- The bead/rim interface remains fixed while lower sidewall, shoulder and tread
  participate continuously.
- Tire mesh density must change smoothness, not the physical extent of deformation.
- Visible and shadow silhouettes must use the same deformation policy.

## Consequences

- A normally loaded tire deforms across its lower half instead of pinching a small
  cluster of vertices.
- Local terrain detail remains possible without an unrestricted soft-body tire.
- Work per visible tire remains bounded: a fixed 273-sample residual solve plus an
  analytic GPU deformation, suitable for presentation LOD scaling on large fields.
- Visual confirmation is still required for each new tire construction/profile;
  construction-specific equilibrium coefficients can later replace the initial
  common envelope without changing this decomposition.

## TIRE36 live-shape refinement

TIRE35 proved the two-scale split, but live inspection still showed a concentrated
lower-sidewall lobe during lateral deformation. Two remaining sources were removed:

- Detailed sphere casts whose contact normal and plane match the native primary
  support are classified as duplicate flat-road support, not irregular contact.
  Diagonal outer-band casts can therefore no longer add a false local dent/bulge.
- Rigid-ring lateral displacement now accumulates progressively from just above
  the equator to the footprint. The sidewall support and broad radial envelopes
  use smooth unions instead of `max` boundaries, avoiding a translated lower chunk
  and the derivative shoulder that appeared in its silhouette.

The ordinary sidewall bulge remains driven by native deflection. Additional local
bulge consumes only compression above the analytic equilibrium and uses a smaller
gain, so kerb displacement can redistribute rubber without double-counting normal
vehicle weight.

## TIRE37 residual-only transport refinement

Live testing at nearly zero inflation exposed a temporal ownership error. The CPU
had separated equilibrium from irregular contact while solving the 21x13 lattice,
but recombined both fields before the entity presentation filter. The shader then
subtracted the current equilibrium from that filtered total. With a soft tire, the
filtered equilibrium lagged the rapidly changing authoritative deflection and the
difference appeared as small false dents.

The presentation contract is now unambiguous:

- Native deflection/contact-patch state exclusively transports the smooth pneumatic
  equilibrium and drives the broad lower-carcass deformation.
- The 21x13 payload transports and temporally filters only coupled irregular road,
  kerb and obstacle residuals.
- Visible and shadow shaders consume that residual directly; they do not reconstruct
  or subtract a second equilibrium approximation.

This makes the flat-road carcass shape independent of probe-filter lag across the
full pressure range while preserving bounded local contact detail on uneven terrain.

## TIRE38 asymmetric cross-section refinement

Live sidewalk testing at 1.50 bar showed that a curb load could pull the unsupported
side of the lower tire inward. The scalar contact field had been smoothed across
width bands and every non-zero band was interpreted as inward compression, even
when its value was inherited from contact on the opposite side.

The shader now compares every width sample with its mirrored sample and decomposes
irregular contact into three bounded responses:

- compression shared by both sides shortens the section radially;
- compression exceeding the mirrored side indents only the actual contact side;
- compression arriving from the mirrored side becomes outward free-side volume
  redistribution through the lower sidewall.

A three-sample section load proxy (centre plus both outer shoulders) supplies a small
symmetric outward response around genuine irregular contact. The bead remains fixed,
the tread is excluded by a radial-depth mask, and contact-side indentation remains
bounded by the existing rim-core and lateral non-inversion limits. Visible and
shadow geometry use the same decomposition.

## TIRE39 relaxed redistribution refinement

TIRE38 corrected the direction of free-side motion, but live inspection showed the
outward response forming a distinct lower pouch. The remaining shape came from two
presentation discontinuities: a hard maximum chose one of three section probes, and
the common contact mask delayed almost all expansion until the lowest carcass rows.

The reduced-order response now:

- derives section load from a weighted root-mean-square of left shoulder, centre and
  right shoulder compression, avoiding dominant-probe switching;
- grows continuously from just above the equator to the footprint;
- uses separate envelopes for common section expansion and free-side transfer, so
  contact-side indentation cannot suppress the unsupported-side response;
- retains bounded magnitudes, bead anchoring, sidewall-depth selection and identical
  visible/shadow deformation.

This remains a deterministic low-order volume response rather than a soft body, but
its silhouette should read as one loaded carcass instead of an attached bottom lobe.

## TIRE40 native carcass-profile authority

Live inspection of TIRE39 still showed a discrete change in the sidewalk-facing
silhouette. Smoothing shader samples could disguise the discontinuity but could not
make its deformation physically authoritative: the shader still chose how much
rubber to move using presentation constants.

TIRE40 moves that decision into `TireCarcassProfile`, a deterministic native
reduced-order provider with 21 circumferential cross-sections. Each section emits:

- negative-wheel-right sidewall expansion in metres;
- positive-wheel-right sidewall expansion in metres;
- lower-carcass rise toward the hub in metres.

The equilibrium derives from free/rim radius, section width, native radial
deflection, finite footprint dimensions, normal load, live inflation pressure and
the residual 21x13 irregular-contact field. An area-preserving ellipse relation
approximates displaced cross-section volume while pressure changes the participating
compliance. One-sided obstacle residuals direct most of that volume to the opposite,
unsupported sidewall. Bead, section-width and sidewall-height bounds prevent
inversion and ballooning.

The visible and shadow shaders now interpolate these physical metre-domain outputs.
They retain only geometry ownership: bead/belt masks, mesh-space conversion and the
direct bounded road-intrusion constraint. They no longer calculate a deflection
bulge, select a probe-derived section load or assign free-side transfer gains.

MF6.2 remains responsible for tire forces and moments. TireRigidRing remains
responsible for transient belt translation, yaw and wind-up. TireCarcassProfile is
the structural shape bridge between those physics results and the rendered tire;
it is not a replacement force law and it is not an unrestricted soft body.
