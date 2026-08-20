# Tire Baseline and Suspension Handoff

**Handoff date:** 2026-08-14  
**Authority:** read together with `CURRENT_TIRE_STATUS.md` and passing native regressions.

## Decision

The native tire program has a sufficiently complete engineering baseline for primary development to
move to suspension. This does **not** mean that every tire construction in history is commercially
calibrated. It means the reusable mechanisms, state ownership, evidence tools and extension points
needed by suspension are present and no known missing tire software layer blocks suspension work.

The suspension solver may rely on:

- deterministic 1000 Hz load, force, moment and relaxation behavior;
- loaded/effective radius and adaptive road enveloping;
- pressure/load/camber-sensitive contact geometry and force response;
- aggregate and explicit bounded `Distributed3x3` contact tiers;
- authoritative flexible-ring visual deformation fed by native tire state;
- thermal, gas-pressure, wheel/rim heat, wear, flat-spot, contamination and failure state;
- hard-road, wet, winter, shallow-granular and persistent deformable-terrain providers;
- installed-tire calibration sweeps, stateful scenarios and acceptance-envelope checks.

## What remains open without blocking suspension

These items are calibration, specialization, presentation or scale-certification work. They should
return to the tire program when evidence/assets exist; they are not reasons to postpone suspension:

- measured tire-rig datasets and per-construction fitted acceptance envelopes;
- production calibration for cross-ply, historical tall-sidewall, racing, motorcycle, kart,
  commercial, run-flat and low-pressure off-road constructions;
- optional explicit sidewall and multi-layer through-thickness thermal nodes;
- graining, blistering, cord/belt fatigue, bead unseating, repairs and rim damage;
- detached-tread mesh mechanics and bare-rim visual destruction;
- complete spatial rain cells, puddle flow, drying-line, snow and ice simulation beyond the
  deterministic global rainfall/film/drainage/evaporation/wind baseline;
- real Nürburgring-scale 150-car CPU/GPU profiling and resulting distance policy beyond the
  executable 600-tire workload laboratory;
- topology/group tire-assignment UI and a polished reusable Parts Lab.

No contributor may silently fill those gaps with manufacturer folklore or invented proprietary
coefficients. Estimated baselines remain labelled, measured data retains provenance, and physical
definitions do not change merely because a vehicle is AI-controlled.

## Suspension integration contract

Suspension owns geometry, linkage constraints, spring/damper/anti-roll forces, unsprung motion and
the wheel/upright pose. Tire owns local contact, road envelope, structural tire state, forces/moments,
rolling radius and tire-local persistent state. Their boundary is the installed wheel centre/upright
frame, normal load, contact kinematics, surface sample and returned hub force/moment.

New suspension topology work must not:

- create a second tire-deflection or contact solver inside suspension;
- infer suspension hardpoints from a subsequently changed tire size;
- bypass the tire's pressure, thermal, wear or failure state;
- enable expensive contact fidelity based on “player versus AI” identity alone;
- modify the TIRE41 visual ring as a substitute for physical wheel/upright motion.

## Verification gate

Before and after material suspension changes, run the Release build and full
`HeritagePhysicsTests`. Tire-specific required checks include calibration sweeps, acceptance
envelopes, stateful scenarios, distributed contact, relaxation/torsion, contact geometry,
flexible/rigid ring, road envelope, thermal/pressure/rim heat, failure, spatial wear,
contamination and surface-provider regressions.
