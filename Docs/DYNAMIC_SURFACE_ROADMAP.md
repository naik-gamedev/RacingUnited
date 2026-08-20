# Heritage Engine Dynamic Surface Roadmap

Status: **Accepted architecture / implementation begins with DSURF00**  
Scope: Racing United + reusable Heritage Engine surface simulation  
Supersedes as the live architecture: WATER14-WATER18 presentation experiments, the persistent-cell marble/rubber storage model in `TrackRubberState`, and all camera-relative puddle-presentation ownership.

## 1. Goal

Replace separate water, wetness, rubber, marble, dirt/mud and track-temperature state with one world-owned, persistent, GPU-friendly **Dynamic Surface** system.

The surface is treated like a dynamic Substance Painter / GIMP canvas attached to the real scene surface. Physics writes state; compute shaders evolve it; the ordinary material shader reads it. The visible result must never expose simulation cells, adaptive water triangles, splat squares, clipmap rectangles or camera-relative regeneration.

The system must support:

- rain film, standing water, runoff, drainage and evaporation;
- water flow direction and speed;
- wind-driven and vehicle-wake-driven water motion;
- track temperature and heat exchange;
- deposited rubber / rubbered racing line;
- loose rubber / marble mass, maturity and migration;
- dirt, dust and mud carried off-track and redeposited on-track;
- tire pickup, cleaning, displacement and deposition;
- aerodynamic transport of water, loose rubber and dust;
- physically shared state between tire simulation and rendering;
- persistent world anchoring with no camera-relative shape changes;
- large tracks, long sessions and ~150-car grids;
- multiple local interest sources without simulating an artificial midpoint between players.

## 2. Core spatial contract

### 2.1 World chunks

The engine automatically partitions wettable/drivable collision surfaces into **100 m x 100 m Dynamic Surface chunks** in global FP64 space.

A chunk is a persistent world address, not a camera LOD. Chunk boundaries must be visually and physically impossible to infer.

### 2.2 Tile resolution — DSURF04C current contract

Each surface sheet inside a chunk owns exactly **one 64 × 64 Dynamic Surface tile**:

- 100 m / 64 = **1.5625 m per authoritative cell/texel**;
- the 64 × 64 raster is simultaneously the Hydro/Track simulation lattice and the base GPU texture;
- there is no separate 4096 × 4096 logical domain in the live Dynamic Surface path;
- there are no 6.25 m sub-pages in the live Dynamic Surface path.

This is an intentional user-directed performance simplification. Fine optical wetness, ripple and breakup detail belongs in authored/procedural material shading rather than in a centimetre-scale persistent physics texture. **DSURF04D2 makes this separation explicit:** Hydro.R is a continuously filtered scalar depth field; support height selects the correct vertical sheet only, while bounded world-space UV warp plus millimetre optical relief hide the coarse lattice.

### 2.3 Persistent tile pool

The existing software page-pool machinery remains as the persistence/residency container, but DSURF04C maps exactly **one physical page to one 100 m surface sheet**:

- one page = one 64 × 64 tile;
- page X/Z within a chunk are always zero;
- vertical surface-sheet IDs still separate bridge decks, roads below bridges, sidewalks, tunnels and stacked parking surfaces;
- base mip0 stores the authoritative 64 × 64 state directly; **DSURF04D2 samples this base only** and no longer builds a live CPU presentation mip ladder;
- inactive tiles retain persistent CPU state but consume no Hydro/Track simulation work until reactivated;
- the pool remains engine-managed and does not require hardware sparse-texture support.

The historical 4096/256 sparse hierarchy remains documented in earlier milestone records but is superseded by DSURF04C.

### 2.4 Surface sheets / vertical layers

A 100 x 100 m XZ chunk may contain multiple disconnected surface sheets. Roads under bridges, bridge decks, raised sidewalks, tunnels and stacked parking surfaces must not share the same state merely because their X/Z coordinates overlap.

The scene bake assigns triangles to connected surface-sheet IDs using geometry adjacency, normal limits, height continuity, collision material and explicit hard barriers.

### 2.5 DSURF04F GPU Water/Snow/Mud authority

DSURF04F promotes the DSURF04E hardware-gate workload into the normal live Dynamic Surface authority. The GPU compute fields themselves are rendered; the former 64×64 CPU Hydro no longer advances after GPU activation. The camera-centred 100 m world-tile envelope is:

- **LOD0:** 8192 × 8192, 1.2207 cm/cell, current 100 m tile, 2 Hz publication;
- **LOD1:** 4096 × 4096, 2.4414 cm/cell, first 8 neighbour tiles, 1 Hz publication;
- **LOD2:** 1024 × 1024, 9.7656 cm/cell, next 16 tiles, 0.5 Hz publication;
- **beyond 300 m:** no high-resolution Dynamic Surface rendering and no outer authority ring.

Water uses `R32UI` with nonlinear 16-bit depth and signed 8-bit X/Z velocity, Snow uses `R16UI` with 12-bit depth plus 4-bit compaction, and Mud uses `R8UI` persistent rut/depression. Both sides of every ping-pong pair are initialized. Publication work is sliced into 128-row units and phase-distributed over render frames, while high-rate tire contacts are merged into bounded world-anchored GPU footprint updates.

The renderer manually decodes/interpolates the integer Water authority and progressively reduces free-surface/Fresnel detail toward the 300 m horizon. F7 removes the previous fixed periodic sine-wave ripple normals because they repeated the same highlight motif across unrelated receivers. Snow and Mud share the same ring addressing/fade. DSURF04F1 preserves overlapping world-tile authority across each 100 m camera-centre rebase by migrating the same world tiles between the 8192/4096/1024 rings; only newly entering tiles start empty.

DSURF04F4 makes local `WaterState` the sole live wetness/puddle source as well as the hydrology state. The legacy `SurfaceWeather` road-film scalar is compatibility/telemetry fallback only while GPU authority is active. Baked infiltration, drainage, flow roughness, depression storage and precipitation exposure/shelter are uploaded as static parameters and consumed directly by the GPU Water solver; they are inputs to the authority, not another water field.


## 3. Dynamic texture/state layout

Do not force all state into one RGBA8 texture. Use typed planes because some quantities are vector-valued or require physical precision.

### 3.1 Water authority — `R32UI` GPU LOD field

DSURF04F supersedes the old live `RGBA16F`/64×64 Hydro presentation plane for normal runtime water. Each WaterState cell is one packed 32-bit word:

- **bits 0..15:** nonlinear standing-water depth, 0..0.50 m;
- **bits 16..23:** signed X/East velocity;
- **bits 24..31:** signed Z/North velocity.

Flow direction and speed are derived from the two stored velocity components. The renderer decodes the same published `R32UI` field that compute evolves. Static support/sheet pages remain separate read-only geometry input, and the old 64×64 CPU Hydro representation remains fallback/regression code only after GPU authority activation.

### 3.2 Track plane — `RGBA16F`

- **R:** surface temperature (°C)
- **G:** adhered/rubbered-track mass or normalized rubber state
- **B:** loose-rubber / marble areal mass
- **A:** marble maturity / aggregation state [0,1]

The old persistent `TrackRubberState` cell/marble storage is retired after migration. Nearby visible 3D marbles become presentation representatives reconstructed from this authoritative field, never the authority themselves.

### 3.3 Contamination plane — initially `RGBA8_UNORM`, promote channels to 16F if physics proves it necessary

- **R:** dry dirt/dust contamination
- **G:** mud/wet-soil contamination
- **B:** generic loose debris / gravel contamination
- **A:** reserved contamination class / future snow-ice coupling

### 3.4 Static surface metadata

Generated once from scene/collider data and cached:

- exact surface height / local micro-height residual;
- surface normal / gradient;
- material ID and permeability;
- drainage coefficient;
- mapped drain/outlet IDs and capacities;
- barrier/curb connectivity;
- surface-sheet ID;
- deterministic microtopography seed;
- optional authored overrides.

Static data can be stored in compressed chunk assets and GPU textures separate from dynamic state.

## 4. Authoritative simulation rules

### 4.1 One world-owned state

`SurfaceWorld` owns `DynamicSurfaceSystem`. Tires, weather, renderer and aerodynamics consume it through narrow APIs. No vehicle owns track state.

### 4.2 Water

Rain adds water/moisture. Water transport is driven by hydraulic head, local surface gradient, flow velocity, drains, permeability, evaporation, tire clearing and aerodynamic impulses.

The renderer never owns water mass. The material shader reads water state and static microtopography to derive organic wet/puddle appearance.

### 4.3 Track temperature

Surface temperature evolves from:

- solar input / time of day;
- ambient temperature;
- cloud cover;
- convection and wind;
- rain temperature / rain cooling;
- evaporation cooling;
- material thermal properties;
- tire friction/contact heating;
- shade/occlusion when available.

The same temperature is queried by tire thermal physics and used by evaporation/drying.

### 4.4 Rubber

Tire contact deposits adhered rubber based on load, slip energy, temperature, compound and surface properties. Rain and abrasion can wash/scrub it. Rubber modifies tire grip through the same sampled Dynamic Surface state.

### 4.5 Marbles / loose rubber

Persistent marbles become a **field**, not hundreds of thousands of authoritative pieces/cells.

Store at minimum:

- loose-rubber areal mass;
- maturity/rounding state;
- mobility derived from maturity, temperature and wetness.

Tire shedding deposits mass. Tire contacts pick it up or shove it. Vehicle wakes and wind can advect/migrate it. Rain can redistribute/wash it.

For graphics, nearby individual 3D flakes/marbles are deterministically synthesized from field mass + chunk/page seed. They are presentation only. The old `SurfacePresentationRenderer` marble GPU cell cache and persistent `TrackRubberState` marble-cell storage are deleted after DSURF05 migration.

### 4.6 Dirt / mud

Off-track tire contact loads contamination into tire tread state. Rejoining the road deposits dirt/mud into Dynamic Surface pages. Subsequent tires pick it up, smear it and clean the line. Water flow can dissolve/advect mud toward drains.

### 4.7 Aerodynamics and wind

Vehicle wakes submit bounded surface impulses rather than running per-particle CFD.

Impulses may modify:

- water velocity / displacement;
- loose marble migration;
- dust/dirt transport;
- evaporation/drying rate.

High-frequency water ripples are **not** stored in the state texture. The shader derives them procedurally from wind vector, water-flow vector, water depth and time. This keeps capillary/ripple detail cheap and temporally smooth.

## 5. Update cadence and interest sources

DSURF04C uses one deliberately simple cadence: **every active 100 m tile updates at 2 Hz**.

A tile is active when its 100 m chunk AABB lies within **1000 m** of at least one real simulation-interest source. Multiple local players are independent sources and activate the union of their nearby tiles. Heritage must never use the midpoint between players as a synthetic simulation centre. Tiles outside the active radius keep their persistent state but are dormant and consume no Hydro/Track polling work.

The old 30/20/6/2 Hz distance bands are superseded. This trades temporal fidelity for predictable low CPU cost and matches the explicit DSURF04C performance direction.

Rendering may interpolate/filter the 2 Hz state. Tire physics may run much faster (including 1000 Hz); tire contacts read the latest tile state and accumulate bounded impulses/deposits for the next 2 Hz Dynamic Surface update.

## 6. Rendering contract

### 6.1 One authored scene draw

No separate settled-water mesh. No adaptive water triangles. No water quadtree. No camera-relative puddle geometry. No duplicate coplanar water surface.

The ordinary road/terrain material shader samples Dynamic Surface textures.

### 6.2 Persistent authority, organic reconstruction

Dynamic state is world/surface anchored and survives camera movement. **DSURF04D2 samples the same authoritative 64×64 mip0 state at every distance** as a continuous scalar depth field; distance never selects a different ownership raster. Screen filtering, analytic contour coverage and optical material detail may soften with distance, but puddle identity cannot change or flicker when the camera moves.

### 6.3 Organic puddles

Visible puddle shape comes from a continuous combination of:

- water amount / free-surface level;
- static high-resolution microtopography;
- local surface elevation;
- drainage basins / barriers;
- the existing `Water_ShorelineBreakup_A8` texture only near the shoreline;
- optional deterministic world-space procedural relief.

Blur may be used as a small finishing/filtering stage, never as a method for hiding square state ownership.

### 6.4 Water optics

- thin film: substrate darkening + roughness/specular change;
- standing water: dielectric Fresnel, reflections, transmission and ripples;
- SSR should be preferred when implemented, environment reflection as fallback;
- ripple normal direction combines wind and stored water velocity;
- deep-water/free-surface geometry is a future separate problem and does not reintroduce a mesh for ordinary track puddles.

## 7. Persistence, streaming and multiplayer

- Chunk addresses and state are FP64-world anchored.
- Dynamic pages can be compressed/serialized for long sessions.
- Dry untouched chunks require no high-resolution resident pages.
- Coarse state remains available when high-res pages are evicted.
- Network authority transmits chunk/page deltas or deterministic events, not visual marble particles.
- Rendering state is never authoritative for physics.

## 8. Explicit legacy retirement

The following architectures are **superseded and must not survive as parallel authorities** once their Dynamic Surface replacement step lands:

- WATER14 adaptive visible-water mesh / stitching / quadtree experiments;
- WATER15 camera-relative hydrology presentation clipmaps;
- WATER16 basin presentation atlas as a separate puddle authority;
- WATER17 implicit-puddle presentation field;
- WATER18 sponge-brush camera-relative canvas;
- old water presentation polygon offset / normal-offset hacks;
- old persistent marble-cell / marble-GPU-cache storage;
- separate `TrackRubberState` persistent spatial authority after rubber/marble migration;
- any renderer path that derives visible puddle silhouettes from solver-cell rectangles.

Historical ADRs/reports remain in `Docs/History`/existing decision history for provenance, but validators must identify them as superseded rather than demanding their runtime code.

Tire-mark ribbons remain a separate persistent presentation system because they represent explicit contact-history geometry, not a scalar/vector surface field.

## 9. Implementation milestones

### DSURF00 — roadmap + engine foundation **(complete)**

- add `DynamicSurface` subsystem directory;
- define the original 100 m chunk / 4096-logical / 256² sparse-page architecture;
- **historical:** this resolution hierarchy is superseded by DSURF04C while the 100 m FP64 chunk/sheet identity is retained;
- define typed hydro/track/contamination state contracts;
- make `SurfaceWorld` own the new `DynamicSurfaceSystem`;
- no visual/physics behavior change yet.

### DSURF01 — static scene surface bake **(complete in DSURF01)**

- partition wettable collider triangles into 100 m chunks;
- build connected surface-sheet IDs;
- bake surface height, normals, material, permeability, barriers, drains and microtopography seed;
- cache deterministic bake to disk;
- regression: bridge over road remains two independent surface sheets;
- regression: 15 cm curb is a hard transport boundary unless overtopped.

### DSURF02 — GPU persistent page pool **(complete in DSURF02)**

- software virtual-texture/page-table manager;
- **historical:** 4096 logical per surface sheet with 256² physical pages;
- **DSURF04C supersedes the hierarchy with one 64×64 page per 100 m surface sheet while retaining persistent chunk/page identity independent of camera;**
- **historical DSURF02:** ordinary mip generation was provisioned; **DSURF04D2 supersedes live water sampling with base-mip continuous-scalar reconstruction**;
- dirty-page queues and GPU compute update plumbing;
- strict VRAM telemetry/budgets.

### DSURF03 — water + moisture migration **(DSURF03B authority cutover complete)**

- **DSURF03A complete:** WATER15-18 camera-relative puddle state/render paths are removed from the live renderer; the ordinary authored material reads persistent Dynamic Surface pages directly;
- **DSURF03A complete:** legacy water/moisture/flow was mirrored into real DSURF01-covered pages as a temporary migration bridge;
- **DSURF03B complete:** persistent Dynamic Surface Hydro pages now own runtime rain accumulation, infiltration, mapped drainage, evaporation, moisture, conservative shallow-sheet flow, tire water clearing, redistribution and spray;
- **DSURF03B complete:** runtime local-condition/tire/material water reads no longer use legacy `SurfaceHydrology` state and `SurfaceWorld` no longer advances that solver;
- **DSURF03B complete:** bridge/road/tunnel precipitation separation comes from DSURF01 surface-sheet geometry and exact support heights;
- **historical DSURF04A:** the former distance-band scheduler used deterministic `VirtualPageAddress` phasing; DSURF04C removes those bands in favor of fixed 2 Hz tile polling;
- **historical DSURF03B/04:** the first authority raster used 16×16 controls per 6.25 m page; DSURF04C supersedes it with one direct 64×64 authority/texture raster per 100 m sheet;
- temporary `SurfaceHydrology` code remains only for static precipitation-cover compatibility and historical regression/benchmark paths until those final non-state responsibilities migrate and DSURF10 removes it.

### DSURF04 — thermal surface **(complete)**

- persistent Track.R owns sheet-aware `surfaceTemperatureC`; DSURF04C now stores it directly in the 64×64 tile raster (1.5625 m/cell);
- ambient, solar/cloud, local rain/wetness, evaporation, material thermal response and shelter/sky exposure evolve each Track cell;
- real tire slip-dissipation energy deposits heat into the exact contacted world/sheet cell;
- `SurfaceWorld::localConditions()` samples the Dynamic Surface thermal authority for tire physics;
- `SurfaceWeatherState::roadTemperatureC` remains only a compatibility/environmental reference and no longer integrates a duplicate persistent road-temperature state;
- Track tiles upload directly at 64×64 base mip0 into the persistent DSURF02 GPU pool alongside Hydro with independent refresh stamps;
- **historical DSURF04B:** page-local static lookup, per-page revisions and resident-only simulation removed the worst hotpaths but retained the over-detailed sparse hierarchy;
- **DSURF04C current:** one 100 m × 100 m / 64×64 tile per surface sheet, fixed 2 Hz active polling, 64×64 static cell bins, direct authority-to-GPU copy and cached tile-indirection updates;
- **DSURF04D2 current presentation:** sample only 64×64 mip0 as a continuous Hydro.R depth scalar; support height selects the vertical sheet only; bounded world-space UV warp plus multi-frequency millimetre shoreline relief hide the authority lattice; exact rendered triangle height is never subtracted from coarse support; no CPU mip ladder or solver-cell boundary may become visible;
- stacked road/bridge thermal separation, tire heating, bounded active-tile simulation and reserved Track G/B/A channels are protected by native regression.

### DSURF04F8 — exact-geometry GPU centimetre-field Water/Snow/Mud authority

- exact 8192²/4096²/1024² Water/Snow/Mud camera-ring authority;
- 2/1/0.5 Hz publication through staggered 128-row compute units;
- Water `R32UI` depth+velocity, Snow `R16UI` depth+compaction, Mud `R8UI` rut state;
- compute-published fields are the exact resources sampled by rendering;
- CPU 64×64 Hydro advancement is retired once GPU authority is active;
- tire contacts directly disturb the live GPU Water/Snow/Mud state;
- asynchronous GPU timer queries and F8 memory/cell/dispatch telemetry remain mandatory;
- next persistence refinement: asynchronous high-resolution WaterState samples back to tire physics, followed by fully conservative filtered cross-LOD transfer if live testing shows nearest-state rebase resampling needs refinement.

### DSURF05 — rubber + marbles migration

- migrate adhered rubber, loose-rubber mass and marble maturity to Dynamic Surface Track plane;
- migrate tire shedding/pickup/migration and rain wash;
- migrate vehicle-wake marble transport;
- generate nearby 3D marble representatives deterministically from field state;
- **delete old persistent `TrackRubberState` spatial marble storage**;
- **delete old `SurfacePresentationRenderer` marble GPU cell cache**;
- keep only transient airborne shred particles as presentation effects.

### DSURF06 — dirt / mud / contamination

- tire tread contamination pickup off-track;
- road deposition on re-entry;
- vehicle cleaning/smearing;
- rain wash and flow transport;
- material-dependent grip coupling.

### DSURF07 — aerodynamic and environmental transport

- bounded vehicle wake impulse buffers;
- water/marble/dust advection;
- wind coupling;
- procedural ripple shader driven by wind + stored flow.

### DSURF08 — rendering maturity

- organic puddle contour from water level + microtopography;
- shoreline mask restricted to edge breakup;
- SSR + environment fallback;
- wet/dry material transitions;
- stable far appearance from the same persistent base authority; any future GPU filtering must remain presentation-only and must not expose cell ownership;
- no camera-relative regeneration/flicker.

### DSURF09 — persistence / fleet / networking

- chunk/page compression and streaming;
- long-session persistence;
- 150-car stress tests;
- multiplayer chunk delta/event replication;
- deterministic interest-source scheduling.

### DSURF10 — hard cleanup

- delete superseded water presentation source files and validators;
- delete old marble storage/render caches;
- delete duplicate rubber/water authorities;
- move historical WATER14-WATER18 implementation docs to history if useful;
- update architecture/project-state docs;
- add validator that fails if retired authorities are reintroduced.

## 10. Acceptance criteria

Dynamic Surface is complete when:

1. No water puddle can reveal a square/triangle simulation footprint at any camera distance.
2. Puddle shapes remain identical in world space as the camera approaches/recedes; only optical filtering/anti-aliasing detail may change.
3. A road puddle cannot visually or physically jump onto a raised sidewalk unless water depth reaches the overtopping height.
4. Water, temperature, rubber, marbles and dirt queried by tires are the same authoritative state rendered by the material system.
5. Tire clearing, rubbering, contamination and aerodynamic wake effects alter persistent world state.
6. Nearby 3D marbles are presentation representatives of Dynamic Surface loose-rubber state, not an independent persistent marble simulation.
7. A 150-car stress test does not require per-car full-world surface scans.
8. No camera midpoint is used as a multiplayer physics source.
9. Chunk boundaries are invisible in state, visuals, tire forces and persistence.
10. Legacy WATER14-WATER18 and old marble-storage authorities are absent from the active runtime.

## 11. Non-negotiable design rule

> **Simulation discretization is never presentation geometry. Dynamic Surface state is persistent paint attached to the world surface.**


### DSURF04F8 exact geometry input

The 8192/4096/1024 authority no longer accepts a 64×64 support raster as hydraulic input. Static collision triangles are queried on GPU at each authority texel through a 256×256 triangle-candidate acceleration index. This acceleration index is not surface state and cannot imprint a 256² lattice because the actual height is barycentrically evaluated on the authored triangle plane at the centimetre-scale texel coordinate.


### DSURF04F9 — finite-volume high-resolution WaterState

- Keep WaterState as the exact simulation/render authority at 8192² / 4096² / 1024².
- Replace prototype head-relaxation/8-neighbour transfer with conservative finite-volume depth+momentum face fluxes.
- Use hydrostatic reconstruction over exact authored triangle support and Rusanov fluxes with positivity-preserving donor limiting.
- Apply authored flow roughness as momentum friction; preserve local rain/infiltration/drainage/evaporation source terms.
- Remove camera-Y physical receiver selection; use persistent authored surface-sheet identity.
- Preserve stitched world/LOD ghost sampling, 300 m horizon, tire-contact GPU disturbances and direct rendering from published WaterState.


### DSURF04F10 — concurrent visible-ring WaterState **(superseded by DSURF04G)**

- Preserve the F9 finite-volume R32UI WaterState and exact per-cell authored geometry.
- LOD0 8192² x1 remains 2 Hz with four internal Water solves; LOD1 4096² x8 remains 1 Hz with two internal solves; LOD2 1024² x16 remains 0.5 Hz with one internal solve.
- A row work unit dispatches across every layer of its LOD array in the compute Z dimension. No ring serializes complete 100 m tiles, so every visible tile receives rain/flow/drainage continuously at its requested cadence.
- Recentring only migrates/resamples existing world state between LOD rings; it does not begin hydrology for a tile that should already have been live.
- F8 reports tile-slices serviced, per-LOD publication cycles and exact-geometry coverage for all 1/8/16 tiles.


### DSURF04G / G2 — 10 m / 512 fixed-resolution WaterState + filtered presentation **(current candidate)**

- Replace 100 m simulation-resolution rings with 10 m x 10 m world/surface-sheet tiles.
- Every resident Water tile is always 512² R32UI (~1.953 cm/cell); simulation resolution never LODs with distance.
- Distance changes cadence only: 2 Hz to 100 m, 1.5 Hz to 150 m, 1 Hz to 220 m, 0.5 Hz to 300 m.
- Simulate the full 300 m interest envelope independent of camera frustum; rendering occurs only for drawn geometry and fades toward zero by 300 m.
- Add an invisible velocity-predicted prewarm corridor beyond 300 m (about five seconds of travel, capped at 600 m extra) so high-speed vehicles cannot reach cold water state.
- Store N/E/S/W curb/wall/surface-sheet barrier topology in WaterState itself and use the same bits for finite-volume flux and presentation filtering. Permit crossing only by physical overtopping.
- Keep exact authored-triangle evaluation at every WaterState cell and keep the finite-volume solver as the one water authority.
- Use a 3072-slot 2D atlas plus one scratch tile for Water; Snow R16UI and Mud R8UI remain lazy.
- Presentation may filter/LOD with distance, but it must derive from the same 512² authority and never smear across hard topology.
