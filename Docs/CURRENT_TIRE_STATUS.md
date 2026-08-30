# Current Tire Physics Status

**Authority date:** 2026-08-29
**Scope:** Heritage Engine native tire, contact, driven-surface, failure, presentation and authoring systems.

This is the authoritative tire-status ledger. It records what the current source tree actually
contains, what has automated evidence, what still relies on estimates or live visual judgement, and
what is genuinely absent. Older TIRE roadmaps and ADRs retain design history, but their status prose
is not authoritative when it conflicts with this file or the current source/tests.

Update this ledger in the same commit as every material tire milestone. Do not infer completion from
a chat message, screenshot, filename, numbered milestone, or the presence of a scaffold.

## Status vocabulary

- **Validated:** compiled implementation with deterministic regression evidence and, where visual or
  driving behaviour matters, recorded user validation.
- **Implemented:** active compiled implementation with automated evidence, but still awaiting measured
  data, broader live testing, or production calibration.
- **Partial:** a useful bounded mechanism exists, but it does not yet satisfy the final physical scope.
- **Scaffold:** an ownership/file boundary exists without active runtime behaviour.
- **Missing:** no current implementation should be assumed.
- **Deferred:** intentionally outside the current completion gate.

## Current architecture checkpoint

The tire is a deterministic reduced-order physical system. It is not an unrestricted soft body. The
1000 Hz vehicle loop owns force/contact transients; thermal, wear, surface and presentation work may
use bounded lower-rate state where doing so does not change authoritative forces unpredictably.

TIRE44 / ADR-140 is the active carcass-deformation architecture, with the TIRE45B moving-wheel
road-cache correction. One persistent 24x13 physics-owned displacement/velocity lattice advances at
125 Hz from the 1000 Hz wheel path while its visible readback lease is active. It reuses actual
tire-force road-envelope collision points/normals/misses as unilateral contacts, includes an internal
rim/flange contact boundary, and consumes pressure plus rigid-ring/tread structural state. Road-envelope
samples cached across 1 kHz wheel substeps store point offsets from the envelope's centre contact, not
absolute world points; each carcass solve re-anchors that local road shape to the live centre contact.
This prevents vehicle translation between envelope refreshes from masquerading as carcass deformation.
The 24 circumferential lattice stations are Eulerian wheel-frame stations: whole-field state is not
convected by wheel angular velocity; material-fixed effects such as flat spots use wheel rotation
explicitly. Physical `tireDeflection` remains contact/force telemetry but is not prescribed as a
carcass-node shape. Historical shader dents, support-plane locks, stacked bulges, render-time carcass
solvers and whole-field wheel-spin advection must not be restored.

Visual flexible-ring vertex deformation is evaluated only for tire-node centres within 50 metres
of the active camera. Distant tires retain their complete physical, thermal, wear and failure state;
only close-range mesh displacement and its matching deformed shadow are omitted. The F8 performance
overlay reports active and distance-culled tire deformation ranges.

Hard road now has two explicit contact-fidelity tiers around the same calibrated whole-tire force
target. `Aggregate` remains the scalable/default tier. `Distributed3x3` allocates load and shear
through a bounded nine-cell local brush, admits split material/support state and integrates local
forces and moments back to the hub without evaluating nine unrelated MF tires. Racing United opts
the player vehicle into this tier; other vehicles remain aggregate until a measured proximity/LOD
policy selects otherwise. The persistent 16x3 tread field remains the authority for rotating
temperature, wear and contamination history; the 3x3 force cells are an ephemeral contact solve.

## Implemented and evidenced

| Area | Status | Current evidence / boundary |
| --- | --- | --- |
| MF6.2-style road tire | Implemented | Pure and combined Fx/Fy, Mx/My/Mz, pressure/load/camber sensitivity, stiffness and trail telemetry; seeded estimates remain distinct from fitted `.tir` data. |
| Turn slip and low speed | Implemented | Rolling turn-slip modifiers plus separate deterministic standstill contact-patch torsion. Full public PHYP equation parity remains partial. |
| Transient slip | Implemented | Longitudinal/lateral relaxation state is rate-stable at 1000 Hz and 120 Hz. |
| Motorcycle tire contour | Partial | High-camber contour/contact telemetry and force branch exist; a complete two-wheel vehicle solver is a separate vehicle-topology milestone. |
| Contact geometry | Implemented | Loaded/effective radius, finite footprint, adaptive road envelope and SWIFT-like rigid-ring structural modes. |
| Visual carcass deformation | Implemented / awaiting live validation | TIRE44 physics-owned 24x13 dynamic carcass with persistent displacement/velocity, 125 Hz structural solve, road-envelope unilateral contacts, rim/flange contact and copy-only visible/shadow presentation. The rejected support-plane lock is absent. Live resting/braking/acceleration/flat-tire calibration is still required. |
| Thermal and pressure | Implemented | Tread/carcass/contained-gas/wheel-rim energy, brake-to-rim and rim-to-carcass conduction, ideal-gas pressure, grip/stiffness response and rate-stable integration. |
| Spatial tread state | Implemented | Rotating 16x3 surface temperature, tread depth/wear, flat spots, retained water and contamination history. |
| Wear and flat spots | Implemented | Slip-energy/load/temperature abrasion, local radius loss, grip change and vibration inputs. Graining/blistering/cord damage are absent. |
| Contamination | Implemented | Dirt, grass, gravel, rubber and water pickup/cleaning with local grip and heat-transfer effects. |
| Wet road and weather | Implemented baseline | Deterministic rainfall, bounded road film, drainage, evaporation, wind/road temperature, footprint water-depth sampling, film lubrication, tread drainage, hydrodynamic lift, water memory and progressive hydroplaning. Spatial puddle flow is absent. |
| Winter | Implemented baseline | Temperature-sensitive hard ice, melt film, compacted snow, siping/stud response and packed-snow tread state. Dynamic world temperature/accumulation is absent. |
| Gravel and hard dirt | Implemented baseline | Reduced-order shallow granular interaction with tread engagement and bulldozing terms. Coefficients are largely estimated. |
| Mud, sand and deformable terrain | Implemented baseline | Persistent SurfaceField sinkage/shear/rut state and reduced direct hard-interface force. Specialty flotation calibration remains incomplete. |
| Track evolution | Implemented baseline | Dynamic rubber, marbles, shedding, washing/migration, tire marks and bounded LOD/presentation caches. Endurance streaming/performance remains open. |
| Tire failures | Implemented baseline | Healthy, slow puncture, rapid loss, blowout, detached tread, collapsed carcass and bare-rim logical stages; gas loss and structural progression are regression tested. Full mesh destruction/rim damage are deferred. |
| Reusable tire parts | Implemented baseline | Family taxonomy, engineering dimensions, optional authoritative `.tir`, creator biases, provenance and per-wheel fitment cold pressure. |
| TIRE18 steady-state calibration runner | Implemented | Native deterministic 1D/2D sweeps for pure longitudinal, pure lateral, combined slip, load, pressure, camber and turn slip, with CSV-ready output and regression evidence. |
| TIRE18 installed-tire laboratory | Implemented | Selected installed wheel/tire sweeps, native plots, A/B comparison without axis normalization, CSV plus provenance/validity/build manifest. |
| TIRE18 stateful scenario laboratory | Implemented | Relaxation, heating/cooling, sustained-corner wear, flat spots, brake/rim heat soak, slow puncture and blowout scenarios with common CSV/plot schema. |
| Calibration acceptance envelopes | Implemented infrastructure | Provenance-labelled synthetic/measured envelope type; finite, validity, sign, continuity and force/moment bounds are regression tested. No measured tire-rig envelope ships in the repository. |
| Distributed contact-force tier | Implemented experimental tier | Bounded 3x3 load/shear allocation behind an explicit switch; homogeneous-baseline, split-friction, partial-support, moment and work-bound regressions pass. Broader live driving calibration remains required before making it the universal default. |

## Partial mechanisms that must not be described as finished

### Distributed contact-patch promotion

The bounded 3x3 tier is implemented and deterministic, but remains an explicit high-fidelity tier.
Promotion to every nearby vehicle requires more live curb, split-water and transient validation. It
intentionally preserves one calibrated whole-tire target rather than pretending that nine standalone
MF evaluations are physically identified local tread blocks.

### Thermal depth and wheel coupling

The active thermal network has bulk tread, carcass, contained gas and wheel/rim nodes plus 16x3
spatial tread-surface offsets. Brake friction power heats the rim and conducts into the carcass; the
Tire Lab has a dedicated heat-soak/cooldown scenario. Explicit inner/outer sidewall nodes and
multiple through-thickness rubber layers remain an optional higher-order model requiring identified
capacities/conductances; the existing surface-plus-bulk network must not be described as measured
depthwise thermodynamics.

### Structural tire constructions

The family/bias system supplies coherent parameter baselines, but it is not equivalent to measured
construction-specific structural solvers. Radial, cross-ply, historical tall-sidewall, slick,
motorcycle, kart, commercial, run-flat and low-pressure/off-road tires still require identified data,
specialized mechanisms where justified, and calibration. `LowPressureTireModel.cpp` is currently a
non-compiled scaffold, not a working provider.

### Damage and endurance

Pressure loss and reduced-order stage progression exist. Missing physical depth includes puncture
object/material differences, valve and bead leaks, repairs, belt/cord fatigue, graining, blistering,
delamination, impact cuts, bead unseating, run-flat support, rim damage and authoritative detached
tread geometry/interaction.

### Dynamic environment input

The world now authors deterministic global rainfall, a bounded road-water film, drainage,
humidity/wind evaporation and road temperature. Tire contacts consume the exact hard-surface film
depth and wind affects thermal convection. This is not yet a complete spatial weather field: local
rain cells, puddle geometry/flow, racing-line drying, spray coupling, snow accumulation/compaction
and ice formation/melt remain open.

## Genuine remaining completion gates

### TIRE18A - deterministic steady-state evidence — implemented

- Pure longitudinal-slip curve.
- Pure lateral-slip curve.
- Combined slip-ratio/slip-angle map.
- Load, pressure, camber and turn-slip sensitivity curves.
- Canonical SI inputs, deterministic sample ordering and CSV-ready complete force/moment telemetry.
- Regression checks for repeatability, curve direction and sensitivity.

### TIRE18B - active fitted-tire laboratory UI — implemented

- Run TIRE18A sweeps against a selected installed tire part/wheel in the Vehicle/Tire Lab.
- Plot Fx, Fy, Mz, trail, stiffness and grip utilization.
- Compare two runs or tire definitions without silently normalizing their axes.
- Export CSV plus tire identity, parameter provenance, confidence and validity envelope.
- Save a compact machine-readable run manifest for later automated comparison.

### TIRE18C - transient, thermal and endurance scenarios — implemented core

- Relaxation step response at multiple speeds and rates.
- Loaded/effective-radius, cleat, curb and rough-road enveloping tests.
- Heating/cooling, pressure growth and brake/rim heat soak/cooldown.
- Sustained cornering, braking flat spot, wear and contamination scenarios.
- Split-mu, split-water, hydroplaning, winter, gravel, mud/sand and motorcycle high-camber cases.

### TIRE18D - measured-data calibration and acceptance envelopes — infrastructure implemented; data external

- Import measured/fitted datasets with source, units, conditions and confidence.
- Store expected curve envelopes rather than tuning only to one exact synthetic result.
- Reject changes that create non-finite values, discontinuities, wrong force signs, energy creation or
  out-of-envelope behaviour.
- Keep synthetic datasets visibly labelled as estimates.

### TIRE18E - distributed contact-force tier — implemented experimental tier

- Add local cell force/shear integration behind an explicit fidelity tier.
- Compare integrated results against the current aggregate-MF baseline.
- Preserve deterministic 1000 Hz behaviour and bounded work.
- Prove split-surface, curb and partial-water improvements before promoting it.

### TIRE18F - large-grid scalability — executable tire-stack benchmark implemented; full scene pending

- Profile player/full-fidelity, near-AI and distant-AI tiers.
- Validate a 150-car field without changing physical definitions between tiers.
- Permit deterministic rate/spatial reduction for distant vehicles; never silently select arcade
  physics because a vehicle is AI-controlled.

The native Tire Lab now executes and times a 150-car / 600-tire workload at 1000 Hz, including
thermal, wear and wet-state work at 100 Hz and the player's bounded `Distributed3x3` cells. Native
regression verifies exact work counts without asserting a hardware-specific time threshold. This is
still a single-threaded tire-stack diagnostic, not a claim that a 150-car Nürburgring scene—including
collision, suspension, AI, rendering, audio, networking and streaming—has been profiled.

## Remaining authoring/tooling work

- Topology-aware `All -> axle/group -> wheel` tire assignment for cars, motorcycles, trikes,
  dual-wheel trucks and multi-axle vehicles.
- Tire/Vehicle Parts Lab for identity, dimensions, pressure ranges, construction, tread, provenance,
  creator biases, measured property files and calibration results.
- Explicit engineering displays for cold/hot pressure, temperature window, construction, load/speed
  rating, mass/inertia, tread void/sipes/studs and data confidence.

## Deliberately deferred or rejected

- Unrestricted soft-body tires for every vehicle.
- Dozens of independently simulated rigid bodies per tire.
- Proprietary MF-Tyre/Simcenter equation or dataset cloning.
- GPU-only deformation that disagrees with authoritative physics.
- Manufacturer/model names selecting performance without engineering data.
- Full detached-tread mesh cutting, bare-rim visual destruction and rim fragmentation until the
  underlying damage/contact architecture and performance budgets are ready.

## Documentation precedence and continuity rules

1. Current compiled source and passing regression tests establish what exists.
2. This ledger establishes current completion status and priority.
3. Accepted ADRs establish architecture decisions and invariants.
4. `TIRE_MODEL.md`, `TIRE_SURFACE_ROADMAP.md` and `TIRE_PART_AUTHORING_ROADMAP.md` provide detailed
   mechanism history and planned scope.
5. `TireDeformation_IMPORTANT/` is a postmortem. Its abandoned attempts are evidence, not selectable
   alternative implementations.

Before changing tire physics, an AI or human contributor must read this ledger, the relevant accepted
ADR, the active provider source and its regression. A material change is incomplete until this ledger
is updated with its actual evidence level.
