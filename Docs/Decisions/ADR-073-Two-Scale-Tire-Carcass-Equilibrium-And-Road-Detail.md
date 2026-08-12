# ADR-073 — Two-Scale Tire Carcass Equilibrium And Road Detail

**Status:** Accepted for TIRE35 / VIS28 (2026-08-12)

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
