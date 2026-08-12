# ADR-039 — Adaptive 2D tire footprint and rigid-ring rotation

**Status:** Accepted — TIRE06
**Date:** 2026-08-09

## Context

TIRE05 proved that finite-footprint road information and a dynamic belt/ring can sit between
road geometry and MF6.2 without destabilizing the existing vehicle stack. A single longitudinal
probe row cannot distinguish a curb under one shoulder of the tire, road crossfall, partial
support, or split-surface contact. At the same time, issuing dense contact/force calculations
for every tire at 1000 Hz would conflict with Heritage's large-grid goals. TIRE05 also imported
yaw and wind-up structural parameters without activating their rotational states.

## Decision

- `TireRoadEnveloping` supports a bounded 2D footprint lattice driven by the public
  `ELLIPS_NWIDTH/ELLIPS_NLENGTH` fidelity hints.
- Adaptive mode starts with a low-cost centre cross and refines to the full bounded lattice
  when height, partial-support, or surface/wetness discontinuities require lateral resolution.
- The current Racing United prototype requests 3x3: five locations when coarse and nine when
  refined. The provider supports other bounded odd resolutions without making them mandatory.
- Smooth local road grade and crossfall are removed before obstacle roughness is evaluated.
- Extra road queries are independently rate-limited: 125 Hz on quiet contact and 250 Hz on
  complex/refined contact by default. Rigid-ring and MF state remain in the high-rate tire loop.
- Surface profiles from supported footprint samples are aggregated before one MF6.2 evaluation.
  TIRE06 deliberately does not evaluate independent MF force laws for every sample.
- `TireRigidRing` activates yaw and wind-up second-order modes using belt rotational inertia,
  structural frequency/damping and tire moments. These states feed subsequent slip kinematics.
- All current Racing United structural/enveloping numbers remain explicitly synthetic until
  identified or measured tire data replaces them.

## Non-goals

TIRE06 does not claim exact proprietary MF-Swift enveloping filters, distributed brush/FEM
contact stress, dense per-cell Magic Formula evaluation, tire thermal/pressure/wear state, or
Simcenter Tire 2512 wet-road numerical parity.

## Consequences

Heritage can now detect and react to laterally nonuniform road/support/surface conditions with
a small bounded number of extra collision queries. Player/important vehicles can therefore gain
useful 2D footprint fidelity while future physics LOD can lower sampling for distant AI. Ring
rotation adds another physically meaningful transient layer without merging road contact, tire
structure and MF force generation into one monolithic solver.
