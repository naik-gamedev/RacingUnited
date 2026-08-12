# ADR-063 — Dynamic Track Rubber and Procedural Tire Marbles

## Status

Accepted; TIRE15C3 user validated, TIRE15C5 authoritative moving-rubber/aerodynamic-migration amendment pending user validation.

## Context

TIRE11 already carries rubber pickup on the tire, CLEAN10 established world-owned/global-coordinate surface state, and TIRE15B established one-way driven-surface presentation. Track rubber must now evolve across all vehicles without turning marbles into a generic terrain material or thousands of persistent rigid bodies.

## Decision

1. `Physics/Surfaces/Rubber/TrackRubberState` owns deposited racing-line rubber and loose-rubber/marble concentration. It is specialized rubber state, separate from deformable `SurfaceField`.
2. State is addressed at FP64 global positions, chunked and bounded, with a coarse vertical layer so stacked roads do not share rubber.
3. A tire samples rubber **before** its force evaluation. Deposited dry rubber can provide a bounded grip gain; deposited wet rubber can be less favorable; loose rubber reduces grip/increases rolling drag and feeds TIRE11 `surfaceRubberDebrisFraction` pickup.
4. After tire wear/thermal state advances, tire use deposits bonded rubber and generates loose rubber from load, motion, slip work, tread wear and tread temperature. The passing tire sweeps/migrates loose rubber laterally so marble concentration tends to build beside the repeatedly driven path.
5. Global rain/wet exposure ages state lazily. Loose rubber washes/ages much faster than bonded rubber; no whole-track per-frame scan is required.
6. Visible marbles are deterministic procedural presentation: small tapered/curling ribbon segments and occasional tangled crossings generated from nearby loose-rubber cells. They are not physics objects and require no authored marble model. Optional authored archetypes may be added later without changing authoritative rubber state.
7. The named wheel telemetry table gains deposited rubber, loose rubber, rubber friction scale and rubber pass count. The legacy positional wheel ABI remains frozen.

## Consequences

- A dry asphalt/default test scene can build visible rubber and marbles through normal driving.
- Multiple vehicles share one evolving racing line and one marble field.
- Rubber pickup and track state remain correctly separated: world owns availability; each tire owns what it carries.
- Rain interaction remains scalable for long circuits and large grids.
- Rendering fidelity can change or be disabled without changing tire physics.


## TIRE15C1 tuning amendment

First in-game validation showed that the initial prototype generated loose rubber far too readily during ordinary rolling, used overlong/brownish procedural ribbons, and exposed the square 0.5 m rubber-state cell as a translucent overlay at night. TIRE15C1 therefore establishes these rules:

- **vehicle speed alone is not a loose-marble source**; loose rubber requires meaningful scrub/slip dissipation and/or actual tread wear, with load and tread temperature modulating the rate;
- ordinary straight-line rolling may build bonded racing-line rubber only very slowly;
- loose-rubber lateral migration is slower and remains contact-driven;
- procedural pieces are smaller, lower to the pavement, neutral black and more sparsely instantiated;
- procedural marble slots use stable spatial seeds, so accumulating rubber reveals additional fixed pieces instead of re-randomizing/jumping on every physics update;
- deposited rubber is rendered as a feathered wheel-path streak rather than a full spatial-cell square.

The world-rubber physics clock remains driven by physics simulation steps and is independent of the day/night `EnvironmentSystem::timeScale()`.


## TIRE15C2 maturity / scalability / lab amendment

Second in-game validation confirmed the C1 sparse/stable presentation is much more plausible and established the next rules:

- shedding has **no hard tread-life gate** such as 70% remaining; tread wear contributes continuously to shedding susceptibility while fresh tires can still shed under sufficiently high slip/load/temperature stress;
- `TireWearDescription::rubberSheddingPropensity` is explicit tire data (neutral 1.0, bounded 0-3) for compound/construction/tread tendency to shed; it is not inferred from manufacturer name or peak grip;
- loose track state stores persistent `marbleMaturity`: fresh additions enter as low-maturity shreds/flakes, and repeated tire contact/agitation/tack/concentration matures them into rounder/clumpier pieces. Merely waiting does not mature them;
- mature marbles are more mobile under a passing tire, so a vehicle leaving the racing line can physically reshuffle the shared off-line band while pickup and rain remove material; dry rubber has no arbitrary age-out timer and remains session-persistent until one of those physical removal/reset events;
- the shared active rubber budget is 524,288 cells. One track state is shared by every vehicle, so a 150-car grid does not allocate 150 copies; visual geometry remains camera-local and capped at 3,500 near marbles;
- presentation uses detailed procedural geometry only within the near field and an aggregate irregular dark loose-rubber LOD out to roughly 220 m, preserving the visible off-line band without rendering every marble at distance;
- development-only SurfaceWorld controls can accelerate tire abrasion, loose-rubber generation and maturity independently up to 1000x. They are exposed in the Racing United Tire LAB and are never vehicle/tire authored parameters.

This remains a scalable-state design rather than a claim that 150-car performance is already benchmark-certified. A future large-grid profiling pass must measure CPU cost at target vehicle counts/rates and may lower rubber-state update frequency while preserving rate-integrated behavior if required.


## TIRE15C3 presentation-stability amendment

The mutable contact heading stored in a rubber cell is physical migration state and is not a stable transform for persistent visual debris. Procedural marbles therefore derive their presentation basis from fixed world axes projected onto the local support plane; deterministic per-piece seeds provide local orientation. Existing debris cannot rotate merely because another tire steers through the cell. The temporary per-cell bonded-rubber rectangles are removed from rendering while bonded rubber remains authoritative simulation state.


## TIRE15C4 persistence / stacking / shed-event amendment

Further in-game validation found two presentation/state issues: already-resting pieces could move vertically when a later steering direction rewrote the cell support frame, and the bounded renderer could make established debris appear to disappear as more rubber cells entered the camera query. TIRE15C4 establishes these rules:

- the first valid support height/normal for a rubber cell becomes its stable presentation anchor; later steering/contact direction changes do not move already-resting debris vertically;
- dry-session loose rubber has no visual lifetime. A persistent logical piece population is world-owned alongside loose-rubber density and survives idle time; it changes only through physical pickup/sweeping, rain/washing, bounded streaming/eviction policy, or explicit reset;
- the default world budget is **500,000 logical rubber pieces**. These are aggregate cell-state counts, not 500,000 rigid bodies or draw calls. The renderer reconstructs deterministic nearby representatives and uses aggregate medium/far LOD;
- persistent piece population is transferred with swept rubber and reduced with pickup/rain, preserving the distinction between material moving and material disappearing;
- tire stress records a `fragmentSeverity` state. Mild scrub biases toward small flakes while severe slip-power/wear/overheat increases the probability/scale of larger chunks; subsequent maturity still shortens/thickens/clumps them;
- dense cells can present multiple stable vertical layers so local debris reads as piles/clumps rather than a single flat carpet;
- fresh tear-off can emit a brief ballistic `RubberShred` presentation particle derived from actual tire shedding. Vertical velocity is deliberately modest; it settles visually onto the support plane, while the authoritative persistent resting material already lives in `TrackRubberState`.

TIRE15C4 deliberately deferred aerodynamic wake displacement and still committed resting material before the visual shred completed its flight. TIRE15C5 supersedes that temporary ownership model.


## TIRE15C5 authoritative moving-rubber / aerodynamic-migration amendment

TIRE15C5 promotes shed/migrating rubber motion into `TrackRubberState` itself rather than treating it as disposable presentation:

- fresh tear-off follows **AIRBORNE -> MOBILE_GROUND -> RESTING** authoritative states. Quantity and logical piece population remain in the moving-packet reservoir until settlement, then transfer into the resting cell reservoir;
- nearby moving packets are visualized as a deformable world-space quad made from exactly two triangles. The shared p0-p2 diagonal stays fixed while p1/p3 can bend independently for cheap flutter/curl. The quad has full 3D position/orientation/velocity despite its two-dimensional geometry;
- the rubber pass is two-sided (face culling disabled). The present shader is unlit and therefore has no lighting-normal dependency; a future lit rubber material must flip its back-face normal rather than culling or lighting the reverse side incorrectly;
- gravity, flake drag, angular motion, a small support-plane impact/bounce and brief ground sliding are bounded lightweight mechanics, not per-piece rigid-body simulation;
- high-rate shedding events merge into a bounded moving-packet pool (default 8,192 packets), while the shared logical resting+moving piece budget remains 500,000 by default;
- each vehicle evaluates one analytical wake at the vehicle/world-step rate. Speed, supported load/reference weight, ride height, footprint and an explicit future `aeroWakeFactor` drive front-pressure, underbody/body and trailing-wake influence;
- established loose rubber migrates conservatively between cells and sufficiently light/fresh material can be lifted into the moving-packet reservoir. Existing moving packets receive consistent wake impulses;
- aerodynamic migration acts on local aggregate cells, not on every logical marble. This preserves the architecture for large grids while allowing nearby representatives to show the same motion;
- tire-contact sweep transfer is now materially conservative. The earlier C4 weighting that redistributed only 92% of swept loose rubber is removed; pickup, rain/washing and explicit reset remain intentional removal mechanisms.

The wake is an analytical approximation, not CFD. Large-grid performance and final wake coefficients remain calibration/profiling work rather than assumed truths.


## TIRE15C5A visual reconstruction amendment

In-game validation showed that aggregate storage ownership must never be visually legible as a regular 0.5 m lattice. Authoritative rubber remains cell/packet based for scale, but visible representatives are not cell tiles: they are stable two-triangle flakes placed from FP64 world coordinates around deterministic anchors that may cross cell boundaries and overlap/stack with neighboring representatives. Dense moving packets reconstruct multiple visual flakes from their logical piece population; fractional populations are stochastically but deterministically represented, preventing both normal-rate overdraw and 1000x underrepresentation. Local orientation remains FP32 because it is camera-local centimetre-scale data rather than a large-world coordinate.
