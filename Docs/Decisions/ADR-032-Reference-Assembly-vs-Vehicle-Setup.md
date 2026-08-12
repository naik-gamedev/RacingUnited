# ADR-032 — Reference Assembly vs Vehicle Setup

**Status:** Accepted  
**Date:** 2026-08-09

## Context

Racing United vehicle assets are authored as clean reference assemblies. The
creator may accurately model wheelbase, track, hub/wheel locations and technical
component metadata while intentionally leaving wheels at neutral camber/toe.
Gameplay and motorsport setup must still support wheel/tire replacement, ET,
spacers and independent per-corner alignment.

Suspension geometry is chassis data. Letting wheel setup rewrite suspension mounts
would corrupt hardpoint kinematics, Ackermann geometry and assisted-authoring
provenance.

GLB object origins also do not yet have one universal physical meaning: a wheel
root may represent a hub face, spin pivot or centerline. Inferring semantics from
its name alone is unsafe.

## Decision

1. The authored asset is an immutable **reference assembly** for physical geometry
   whose semantic meaning is known.
2. Wheel/tire fitment and alignment are **setup overlays**, stored independently
   per corner.
3. Front and rear left/right setup linking is a UI convenience only; every corner
   remains independently representable.
4. ET/spacers move the installed wheel/tire centerline downstream of the upright;
   they do not move suspension hardpoints, the suspension-force attachment, or the
   Ackermann reference track.
5. Camber/toe/caster setup modifies the active alignment state. It does not require
   the Blender reference mesh to be authored at that alignment.
6. Caster retains the hardpoint-derived/reference path unless the setup actually
   requests a different caster value.
7. An arbitrary GLB node transform may not be treated as an authoritative wheel
   centerline/hub datum until its origin semantics are explicitly declared by an
   authoring contract.
8. When strong data is unavailable, estimates remain allowed but must preserve
   provenance/confidence and may not overwrite stronger authored evidence.

## Consequences

- Wheel swaps, spacers and alignment can become Forza-style setup operations
  without destructive asset edits.
- Oval-racing and other asymmetric setups are first-class.
- Suspension providers remain reusable and independent of installed wheel style.
- Better future vehicle assets can replace provisional geometry without requiring
  vehicle-specific solver code.
- A later fitment milestone must formalize hub-face/centerline/spin-axis origin
  semantics before automatic GLB-transform-derived track/ET calculations are made
  authoritative.
