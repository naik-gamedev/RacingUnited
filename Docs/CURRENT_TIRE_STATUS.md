# Current Tire Physics Status

**Authority date:** 2026-08-31
**Active completion milestone:** `TIRE46_FINAL_TIRE_PHYSICS_FREEZE`
**Scope:** Heritage Engine native tire force/contact, construction, thermal, wear, failure, surface coupling, carcass presentation and tire authoring data.

This is the authoritative tire-status ledger. Current compiled source plus deterministic regressions
establish what exists; this file establishes whether that mechanism is still open for architecture
work. Older TIRE roadmaps/ADRs remain design history only when their status prose conflicts here.

## TIRE46 freeze decision

The production **road-car and motorcycle tire solver is feature-complete and frozen as a dependency
for the suspension phase**. New suspension work must consume this tire system instead of adding a
parallel tire model or replacing its contact/thermal/damage architecture.

"Frozen" does **not** mean every historical tire parameter is known. For tires such as a 2003
Pirelli P7000, proprietary compound chemistry and complete tire-rig data may never be available.
Heritage therefore accepts **evidence-informed reconstruction** as production data when it is clearly
labelled with source/provenance/confidence. Period specifications, contemporary reviews, known
construction technology, comparable measured tires, vehicle tests and physical bounds may all
constrain an estimate. Better evidence may improve `.tir` values later without reopening the solver.
Measured datasets remain welcome but are not a completion gate.

## Frozen architecture

- **1000 Hz force/contact authority** with deterministic longitudinal/lateral relaxation and separate
  standstill contact-patch torsion.
- **MF6.2-style clean-room force/moment provider**: pure/combined Fx/Fy, Mx/My/Mz, load, pressure,
  camber and public turn-slip coefficient semantics including the signed PHYP lateral-shift branch.
- **Universal `Distributed3x3` contact** for every newly created native vehicle. One calibrated
  whole-tire target is distributed through nine bounded local contact cells; homogeneous contact
  reproduces the aggregate target while split friction/support/water can redistribute/cap local
  shear and generate hub moments. `Aggregate` remains only an explicit diagnostic/performance
  fallback, never the implicit AI physics model.
- **Finite contact geometry**: loaded/effective radius, finite footprint, adaptive 2D road envelope,
  SWIFT-like rigid-ring structural modes and unilateral road/rim boundaries.
- **Physics-owned flexible carcass**: persistent 24x13 displacement/velocity lattice at 125 Hz,
  consuming real pressure/contact/rigid-ring state. TIRE45K distributes real Fy as lower-tread shear
  and Mz as an equal/opposed footprint shear pair. Render code copies the physics field; it does not
  solve a second tire. The expensive lattice is activated by a short presentation-demand lease;
  tires outside visual demand keep their authoritative force/contact/thermal/wear/failure state
  without paying for an unseen deforming render mesh.
- **Seven-node construction thermal network**: tread, belt/undertread, carcass, inner sidewall,
  outer sidewall, contained gas and rim, plus the rotating 16x3 / 48-cell tread-surface thermal field.
  Structural-loss heat is partitioned energy-conservatively between belt/carcass/sidewalls; authored
  fractions above 100% are rejected instead of creating heat.
- **Pressure coupling** from contained gas temperature/mass and reference cavity state.
- **Complete physical damage/endurance coordinates** underneath the compatible coarse failure stage:
  tread puncture/cut, sidewall cut, valve/bead leaks, bead unseating, belt/cord/sidewall fatigue,
  graining, blistering, delamination/tread separation, underinflation collapse, blowout, rim contact,
  rim damage and optional run-flat support degradation. Damage feeds force capacity, carcass support,
  pressure and rolling resistance rather than existing as telemetry only.
- **Spatial tread state**: 48 rotating cells for temperature offsets, tread depth/wear, flat spots,
  retained water and contamination.
- **Surface/weather coupling**: wet-film lubrication, drainage, retained water, progressive
  hydroplaning, compacted snow/hard ice, gravel/hard dirt and persistent deformable mud/sand/deep-snow
  terrain interaction, plus track rubber/marbles/tire-mark evolution.
- **Motorcycle tire physics**: high-camber MF branch, camber thrust/moments, rounded crown contact
  geometry as the actual support/query authority, crown lateral contact offset, 3x3 local contact,
  seven-node thermal state, 48-cell tread state, wet/winter/granular behavior and the same complete
  construction/failure model. Fork/swingarm/steering-head geometry, rider mass motion and whole-bike
  balance/lean dynamics are vehicle/suspension responsibilities and are not missing tire physics.

## Validation status

| Area | TIRE46 status | Evidence / boundary |
| --- | --- | --- |
| MF6.2-style dry force/moment core | **Frozen** | Deterministic force/moment regression, combined-slip checks and turn-slip sign/reduction checks pass. |
| Turn slip / PHYP | **Frozen** | PDXP/PDYP/PKYP/PECP/QDTP/QBRP/QDRP and signed PHYP branch are parsed/evaluated; +/- spin regression proves the signed lateral shift is active. |
| Low-speed torsion / relaxation | **Frozen** | Standstill torsion and 1000/120 Hz relaxation rate-stability regressions pass. |
| Universal 3x3 contact | **Frozen** | Native vehicle default is `Distributed3x3`; homogeneous, split-friction, partial-support and moment regressions pass. 150-car benchmark exercises 600 distributed tires. |
| Contact geometry / rigid ring | **Frozen** | Effective radius, finite footprint, adaptive 2D road envelope and rigid-ring rate-stability regressions pass. |
| Flexible carcass | **Frozen architecture** | 24x13 solver, road/rim constraints, zero-lateral symmetry and TIRE45K physical Fy/Mz distribution regressions pass. Per-tire visual calibration remains data tuning. |
| Thermal / pressure | **Frozen** | Seven construction nodes + 48-cell surface field; brake/rim heat, camber sidewall asymmetry, gas pressure, energy-conserving structural-loss partition and rate stability are exercised by regression. |
| Wear / flat spots / distress | **Frozen** | Spatial abrasion/flat spots plus graining/blistering/delamination and their force effects are active and regression-tested. |
| Failure / endurance / rim | **Frozen** | Gas leaks, cuts, bead, fatigue, blowout, collapse, rim/run-flat progression and incident API are deterministic and tested. |
| Wet / hydroplaning | **Frozen tire side** | Drainage, water memory, lubrication, hydrodynamic lift/drag and progressive hydroplaning regression pass; water field ownership remains Dynamic Surface. |
| Winter / granular / deformable terrain | **Frozen tire side** | Ice/snow, gravel/dirt and mud/sand/deep-snow tire interactions pass deterministic regressions; world material values remain surface data. |
| Motorcycle tire | **Frozen tire scope** | Rounded crown owns support geometry at lean; +/- camber symmetry and high-lean force/moment regressions pass. Whole-bike dynamics move to suspension/vehicle work. |
| `.tir` import / provenance | **Frozen** | MF + Heritage thermal/damage/tread/surface extensions parse with unit conversion and provenance; prototype files explicitly exercise TIRE46 nodes/parameters. |
| Historical tire calibration | **Ongoing data work, not solver work** | Estimates must retain provenance/confidence. No measured dataset is required to keep TIRE46 frozen. |
| 150-car tire-stack workload | **Correctness proven; isolated single-thread cost measured** | All 150 cars / 600 tires execute 3x3. On the 2026-08-31 Windows Release verification, 50 ms of isolated tire work took 58.16 ms dry and 59.33 ms wet (0.86x/0.84x real time on one thread). This is a deliberately pessimistic single-thread tire-only benchmark, not a complete race or final multicore performance claim. |

## Production data policy for historical tires

A tire definition may be production-usable even when some parameters are inferred. Every parameter
set must make its epistemic status visible:

1. **Measured/fitted** where reliable source data exist.
2. **Derived** where dimensions/physics determine a value from sourced facts.
3. **Evidence-informed estimate** where period reviews, construction, comparable tires and vehicle
   behavior constrain a plausible range.
4. **Low-confidence placeholder** only when no better evidence exists; it must never be silently
   presented as a manufacturer measurement.

For the Peugeot/Pirelli work, the existing prototype `.tir` files remain explicit synthetic/evidence
seeds. A later P7000 reconstruction should improve those values with research and validation runs,
not create another tire solver.

## Things intentionally outside the TIRE46 solver freeze

These may be implemented later without reopening tire-force physics:

- Visual mesh tearing/chunk detachment, rim mesh deformation/fragmentation and decal/particle polish.
- Heritage Studio tire-parts UX and topology-aware `All -> axle/group -> wheel` assignment UI.
- Better historical tire datasets and family-specific estimates.
- Whole-vehicle motorcycle dynamics and rider controller.
- Full-scene 150-car performance work across suspension, chassis, collision, AI, rendering, audio,
  networking and streaming.
- Compatibility with proprietary/obfuscated vendor MF-Swift/T&V extensions that Heritage does not
  own. Unknown assignments stay visible in importer diagnostics; Heritage's own thermal/velocity
  physics remains authoritative rather than pretending unsupported vendor equations are active.

## Rules after TIRE46

1. Suspension/chassis systems consume current tire outputs; they do not duplicate tire physics.
2. A tire change requires a demonstrated missing physical mechanism or regression failure—not merely
   a desire to add more parameters.
3. Historical evidence normally changes data/provenance/calibration envelopes, not architecture.
4. `Aggregate` may be selected explicitly for diagnostics, but AI status must never select it
   implicitly.
5. Tire visual presentation may improve independently provided it remains a copy of authoritative
   physical state.
6. Any future material tire-physics change must update this ledger and add deterministic evidence.
