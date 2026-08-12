# Tire + Driven-Surface Completion Roadmap

## Purpose

Heritage Engine now treats tire simulation and driven-surface interaction as one continuous
engineering program. The tire remains its own physical system, but the force available at the
contact patch depends on what the tire is contacting. Hard pavement, standing water, gravel,
compacted snow, ice, mud, sand and deep snow must not collapse into a single `friction = X`
lookup.

The project directive is to finish the tire/contact/surface stack before returning to unrelated
vehicle-domain expansion. The implementation remains modular so large race grids can select a
bounded fidelity tier without changing the player vehicle's authoritative model.

## Architecture boundary

The intended high-level chain is:

```text
road / terrain state
    -> surface-interaction provider
    -> finite footprint / road enveloping
    -> tire structural state (loaded radius + rigid ring)
    -> transient/contact-patch state
    -> MF6.2 force/moment core where applicable
    -> spatial tread thermal / wear / contamination state
    -> hub force + moment
    -> unsprung mass / suspension / steering / chassis
```

Magic Formula 6.2 remains the handling-force core on hard and hard-supported surfaces. It is
not forced to impersonate deformable terrain. Shallow loose layers may use a hybrid of MF6.2
and granular/soil mechanics. Fully deformable terrain uses a dedicated terramechanics provider
for the ground reaction while still reusing the pneumatic tire's geometry, pressure, thermal,
wear and structural state where meaningful.

`Vehicles/Tires/TireSurfaceInteraction.*` is the intended native provider boundary. It must not
become a generic dumping ground: each surface regime owns narrow, testable mechanisms.

## Performance contract

- The normal high-fidelity path continues to perform one MF6.2 evaluation per tire unless a
  deliberately selected distributed-contact tier is enabled.
- The 16 x 3 tread field is cheap state/history, not 48 independent tire solvers.
- Tire structural/contact state may remain at the high-rate vehicle cadence (normally 1000 Hz).
- Expensive environment/terrain queries are adaptive and may run at lower or event-driven rates.
- Surface providers expose deterministic state and explicit fidelity tiers so distant AI does not
  pay the player vehicle's full terrain-query cost.

## Authoring contract — tire data + SurfaceMaterial / SurfaceField

Tire presentation geometry and tire engineering behavior are deliberately separate authoring
channels. A detailed high-poly tread may be baked to a lower-poly runtime mesh without losing
simulation fidelity: the tire's `.tir` / Heritage metadata remains the authoritative place for
measured, fitted or explicitly estimated engineering traits. Runtime mesh detail is presentation
and optional geometry evidence, not the tire-force database.

Per-tire engineering metadata may include, as evidence becomes available:

- MF6.2 / structural / thermal / wear parameters;
- tread depth, void ratio and directional groove/drainage capacity;
- compound class and temperature behavior;
- effective siping density and snow tread interlock/self-cleaning traits;
- stud count, protrusion and future stud geometry/material metadata;
- loose-surface tread openness/aggressiveness and mud/gravel/snow self-cleaning traits.

These are mechanism inputs, not a pile of direct `dryGrip`, `wetGrip`, `snowGrip` multipliers. For
example, drainage metadata feeds TIRE12 water evacuation, and siping/stud metadata feeds TIRE13
mechanical winter interaction. Future authoring tools may estimate tread depth/void/groove/sipe
features from sufficiently accurate modeled geometry, but the mesh is never mandatory for serious
physics and inferred values must retain provenance/confidence.

Scene authoring uses one surface identity that survives render and collision generation. The
preferred Heritage content workflow is vertex/material/custom-property painting in Blender ->
`SurfaceMaterial` identity -> renderer + collision/surface lookup + audio + particles + weather.
A simplified collider must not erase a precise asphalt/kerb/grass/gravel boundary merely because
its triangles are coarser than the visible mesh. Adaptive tire-footprint samples query the local
surface identity/mixture at each point.

`SurfaceMaterial` owns mostly static material identity/parameters. The spatial `SurfaceField` introduced by TIRE15 owns/paves the way for dynamic local state such as:

- water-film depth and drainage;
- surface temperature;
- rubber/debris/contamination;
- loose-layer depth and particle class;
- snow depth/packing;
- moisture/mud state;
- compaction, rut depth and shear history.

The same authored identity/state should drive tire physics, visible material behavior, rolling
audio, spray/dust/debris and weather interaction rather than requiring unrelated duplicate paint
maps. See ADR-047.

## Milestones

### TIRE10 - physical flat-spot radius + VIS02 contact-plane lock — USER VALIDATED

- Convert TIRE08's material-fixed tread-depth field into real circumferential tire-radius change.
- Uniform tread loss reduces the physical outer/rolling radius.
- Local non-uniform tread loss changes the support radius as the wheel rotates and therefore
  excites tire radial compliance, unsprung mass and suspension instead of existing only as a
  friction/graphics effect.
- Keep the GPU dent driven by the same wear field.
- Give visual tire deformation the native contact normal and center-to-contact-plane distance so
  the rendered footprint follows the authoritative road plane on slopes/banking rather than
  assuming world-down.

### TIRE11 - contamination, pickup and cleaning — USER VALIDATED

Promote the 48-cell tread state with local contamination channels. Initial materials:

- grass/organic contamination;
- dirt/dust;
- gravel fines;
- rubber pickup / marbles;
- mud film groundwork.

Contacting a contaminating surface deposits material into the currently contacting sectors and
bands. Asphalt contact, slip work and rotation clean it progressively. Contamination modifies
local contact friction/thermal behavior but still does not multiply MF evaluations.

Current TIRE11 implementation consumes the TIRE06 supported-sample material blend, owns five persistent 48-cell material channels, an explicit
dynamic-track rubber-debris input, bounded friction/rolling-resistance/road-heat feedback and
material-specific pickup/retention/cleaning. Snow/ice pickup is deliberately deferred to TIRE13.
Prototype coefficients are synthetic development data and retain that provenance.

### TIRE12 - wet pavement, water film and hydroplaning — USER VALIDATED

Replace the current scalar wet multiplier with an independent hard-surface water layer. Inputs
include at minimum:

- local water-film depth across the adaptive footprint;
- speed and slip;
- normal load;
- tire width and footprint geometry;
- dynamic inflation pressure;
- tread depth and local wear;
- tread void/drainage capacity authored per tire;
- road texture/drainage state.

Outputs include local water evacuation, wet friction scaling, hydrodynamic lift/load relief and
progressive partial-to-full hydroplaning. One side of a tire may be in standing water while the
other remains on merely damp pavement.

Current TIRE12 implementation promotes `TireWetSurfaceInteraction.*` into a compiled clean-room
hard-surface provider. The legacy normalized scene `surfaceWetness` is converted to physical
water-film depth through tire-authored compatibility metadata; TIRE06 footprint wetness is blended
so split wet/dry support can affect the tire before the centre ray alone would see it. Drainage
demand compares incoming water with tread-depth/void-ratio/pressure-dependent groove capacity, then
a bounded water wedge drives hydrodynamic lift, pavement-contact loss, water-plowing drag and
relaxation/rolling/thermal modifiers around one MF6.2 evaluation. TIRE08's 48 cells also retain and
release local water film. The classical pressure hydroplaning-speed relation is diagnostic only.
No proprietary Simcenter 2512 wet-road equations are copied or claimed.

### TIRE13 - ice and compacted snow — USER VALIDATED

Hard frozen surfaces remain non-deformable or weakly deformable contact providers rather than
soft-soil terrain. `TireWinterSurfaceInteraction.*` keeps one MF6.2 force/moment evaluation per
tire and supplies a clean-room winter surface layer using temperature, slip speed, footprint snow/
ice fractions, tread state and tire-authored winter mechanisms. Ice is not represented by one
constant friction value: colder/drier ice and near-melt ice have different base response, slip/
flash heating can grow a thin interface melt film, and tread compound/siping contribute bounded
rubber friction. Optional studs add a separate count/protrusion-dependent mechanical contribution.

Compacted snow uses tread-block/sipe interlock and a modest packed-snow-on-snow contribution. The
existing 16x3 tread field stores packed snow in the rotating contact cells and progressively sheds
it according to rotation, speed, slip and tire-authored self-cleaning. This remains compacted snow
on a load-bearing surface; deep-snow sinkage/plowing remains TIRE15 terramechanics.

The current static-scene bridge supplies an explicit -5 C local winter surface temperature because
the dynamic `SurfaceField` does not yet exist. TIRE13's provider is already temperature-dependent;
future scene/weather authoring will replace that compatibility input with local field temperature.
Current Peugeot prototype winter coefficients are synthetic low-capability road-tire placeholders,
not measured Pirelli winter data. See ADR-046.

### TIRE14 - shallow granular gravel / hard dirt — USER VALIDATED

For rally-style surfaces with a loose layer over a load-bearing base, TIRE14 now promotes
`TireShallowGranularInteraction.*` as a compiled hybrid provider. MF6.2/SWIFT remains the pneumatic
tire force/moment core against the supporting base, while the shallow layer contributes:

- load/contact-area-dependent loose-layer penetration and bounded sinkage;
- a reduced-order pressure-sinkage relation for the current static-surface bridge;
- Mohr-Coulomb-style available granular shear strength;
- Janosi/Hanamoto-style shear-displacement mobilization in longitudinal and lateral directions;
- tread-depth/aggressiveness/edge-density/open-void effectiveness;
- slip-angle-dependent lateral passive-wedge bulldozing;
- longitudinal plowing/compaction resistance and energy loss;
- partial gravel/dirt footprint mixing through the existing adaptive 2D sampler;
- sinkage coupled into the actual hub/contact support datum rather than existing only as a grip scalar.

Current gravel/dirt material properties are explicit synthetic compatibility presets inside the provider.
They belong to the future `SurfaceMaterial` / `SurfaceField`, not to the tire `.tir` file. The `.tir`
section owns only tire/tread traits. TIRE14 deliberately has no persistent rut/terrain-memory state;
that becomes authoritative in TIRE15. One MF6.2 evaluation per tire remains the normal path.

This is the preferred high-speed gravel/dirt regime. It is not implemented as a single friction multiplier.
See ADR-048.

### TIRE15 - deformable terrain / terramechanics — USER VALIDATED

For mud, soft soil, sand and deep snow, ground reaction is primarily a terrain mechanics problem.
TIRE15 promotes `TireDeformableTerrainInteraction.*`; CLEAN10 places its shared persistent state behind the world-owned `Physics/Surfaces/SurfaceWorld.*` and chunked `Physics/Surfaces/SurfaceField.*`.
MF6.2/SWIFT still owns the pneumatic tire state, but its direct hard-interface contribution is
deliberately reduced and the terrain provider becomes the primary source of ground reaction.

The current clean-room reduced-order model includes:

- Bekker-style pressure-sinkage using contact pressure, effective footprint width and a material
  pressure/sinkage preset;
- Mohr-Coulomb-style shear capacity with cohesion and internal friction angle;
- Janosi/Hanamoto-style longitudinal and lateral shear-displacement mobilization;
- passive-wedge lateral bulldozing plus longitudinal plowing/compaction resistance;
- tire-authored tread aggressiveness, edge density, void ratio, flotation and worn-tread coupling;
- four explicit deformable surface identities: mud, sand, soft soil and deep snow;
- partial deformable/hard footprint mixing through the TIRE06 adaptive sampler;
- physical support sinkage coupled into the wheel/hub datum;
- persistent sparse world cells storing loose depth, compaction, moisture, rut depth, shear history,
  displaced volume and approximate wheel-pass count;
- bounded cell size/capacity so persistent terrain memory cannot grow without limit.

`SurfaceField` is shared world/physics state, not tire-owned state, so subsequent wheels and other
vehicles query the same rut/compaction history. The current field is a reduced-order 0.25 m sparse X/Z grid grouped into bounded 16 m chunks and addressed from stable FP64 global coordinates, with a coarse 2 m vertical layer separating stacked driveable surfaces. It changes the physical support datum; visible terrain-mesh tessellation/deformation, network replication, weather recovery/erosion and per-footprint-subcell field writes are later presentation/world-system refinements rather than hidden inside the tire solver. Chunk snapshots/restore and eviction callbacks provide the seam those future world systems can use.

Granular visuals (stones, dust, mud spray, snow spray) remain presentation driven by the physical
terrain state rather than requiring every particle to participate in the tire solver. See ADR-049.

### TIRE15B - SurfaceField authoring + visible driven-surface state

**TIRE15B1 USER VALIDATED:** authored physical surface parameters + live world wetness/temperature.
**TIRE15B2 USER VALIDATED:** bounded visual/particle/audio consumers of the same authoritative state.

Promote TIRE15's synthetic terrain presets into authored `SurfaceMaterial` data and connect the
shared `SurfaceWorld`/`SurfaceField` to scene/weather/presentation systems. This follow-up should provide:

- Blender/importable per-surface terrain parameters (loose depth, density, Bekker coefficients,
  cohesion, friction angle, shear modulus, moisture/compaction response);
- dynamic water/surface-temperature inputs that replace the temporary TIRE12/TIRE13 compatibility
  bridges;
- visible rut/sinkage deformation or a bounded GPU/terrain-patch representation driven by the same
  physical field;
- mud/sand/snow displacement, spray/dust/debris and rolling audio driven from field/contact state;
- field streaming/persistence rules appropriate for long tracks/stages and later network replication.

Physics remains authoritative: visuals consume `SurfaceField`; they do not invent an independent
rut/deformation state.

TIRE15B1 implements the first two bullets through `Physics/Surfaces/SurfaceMaterialProperties` and
`SurfaceWorldEnvironment`. Static collision GLB metadata can override terrain mechanics per node with
parent-to-child inheritance; wet/winter/thermal tire paths resolve one shared effective wetness and
road temperature. The original TIRE15 material presets remain the default values, preserving existing
scenes when no new metadata is authored. TIRE15B1 is user validated.

TIRE15B2 adds a strictly one-way `Physics/Surfaces/Presentation/SurfacePresentation` consumer. It keeps
bounded, floating-origin-safe global track marks and transient spray/dust/mud/snow/loose-debris
particles driven from post-physics wheel-contact state. Deformable marks visualize rut depth and
displaced volume without changing collision support; hard loose surfaces such as gravel/dirt use
sparse deterministic transient emission without allocating fake terrain state. The renderer consumes
this data with bounded dynamic GPU buffers and distance culling. `Physics.GetSurfacePresentation()`
exposes normalized rolling/spray/dust/debris mechanism intensities for future authored audio assets;
TIRE15B2 does not invent placeholder tire sounds. Presentation state is never read back by tire or
collision physics. See ADR-062.

### TIRE15C - dynamic rubbering-in + tire marbles

**Current candidate implementation:** `Physics/Surfaces/Rubber/TrackRubberState` is compiled and world-owned through `SurfaceWorld`. It stores bounded chunked deposited/loose rubber at FP64 global coordinates, lazily ages it from world wet exposure, migrates loose generation laterally into marble-rich cells, feeds loose-rubber availability into TIRE11 pickup, and supplies a bounded dry/wet/loose contact modifier. `SurfacePresentationRenderer` consumes nearby rubber cells to darken the rubbered line and generate deterministic curled/torn procedural rubber ribbons and clumps; no authored marble mesh or persistent rigid body is required.

Promote the existing TIRE11 rubber-pickup input and TIRE15/15B dynamic surface-state foundation
into an authoritative evolving track-rubber system. The goal is physical track evolution over a
session or endurance race, not merely a visual decal:

- tires deposit rubber according to compound, tread state, temperature, normal load, slip work and
  local contact conditions;
- the repeatedly used racing line progressively rubbers in while heavily sheared/abraded rubber can
  become loose debris and migrate/accumulate off-line as tire marbles;
- loose-rubber density modifies local contact behavior and feeds the existing tire rubber-pickup /
  contamination state, so driving through marbles can temporarily change grip and tire condition;
- subsequent vehicles read the same shared spatial state, allowing race grids to evolve one common
  track rather than each car owning a private rubber map;
- rain/water and later track-cleaning/weather processes can reduce, wash, redistribute or otherwise
  age deposited and loose rubber instead of treating rubber build-up as permanent;
- presentation consumes the authoritative state to darken the rubbered racing line; a dedicated
  tire-rubber/marble presentation path renders visible loose marble clusters with bounded
  decals/particles/GPU instancing or similar techniques;
- marbles are treated as a specialized tire/rubber subsystem rather than generic deformable terrain.
  Bulk deposited/loose-rubber concentration can share world spatial infrastructure, while marble
  migration/pickup presentation and optional sparse high-detail physical debris keep dedicated
  ownership;
- individual marbles are not thousands of persistent rigid bodies by default. Explicit high-detail
  pieces are reserved for cases where they provide measurable value.

The dynamic rubber field must remain deterministic, bounded/streamable and compatible with
large-grid fidelity tiers. TIRE11's tread-side rubber pickup remains the tire-owned state; TIRE15C's
deposited/loose rubber concentration is world-owned driven-surface state.



### TIRE15C4 — persistent logical pieces, stable support anchoring and shed-event presentation

User validation of TIRE15C3 confirmed world-space orientation but revealed that later tire contacts could still rewrite a cell's support height/normal, making established pieces rise/sink as steering changed. It also showed that a small near-render cap could make debris appear to vanish even though aggregate rubber remained.

TIRE15C4 keeps the physics architecture cell-based and scalable while adding a persistent logical debris population:

- up to **500,000 logical rubber pieces** are represented in shared world state; they are not rigid bodies and are not all rendered simultaneously;
- each rubber cell stores loose-rubber concentration, piece population, maturity and fragment-severity state;
- dry idle time does not delete the piece population; tire pickup, tire-driven sweeping/migration, rain/washing, explicit reset and bounded world streaming are the meaningful removal/relocation mechanisms;
- presentation queries select the nearest relevant cells deterministically rather than returning arbitrary unordered-map entries when the query is capped;
- near debris reconstructs deterministic 3D shreds/chunks from the stored piece population, with up to several vertical stack layers in dense piles; medium/far concentrations remain aggregated;
- fresh shed events can briefly toss a representative rubber shred with tire-surface/slip-driven velocity, then settle it visually onto the support plane. Permanent resting state remains cell-owned;
- fragment size distribution is influenced by the physical severity of the shedding event rather than only by procedural density/maturity.

TIRE15C5 supersedes C4's presentation-only shed toss with authoritative moving rubber and adds analytical aerodynamic migration. Large-grid/Nordschleife profiling remains required before declaring final performance certification.

### TIRE15C5 — authoritative moving flakes and aerodynamic marble migration

- Fresh tear-off follows **AIRBORNE -> MOBILE_GROUND -> RESTING** in `TrackRubberState`; moving quantity/piece population remains authoritative until it actually settles.
- Near moving rubber uses one deformable two-triangle, two-sided flake: a fixed shared diagonal and two independently bending free vertices. Geometry is 2D while motion/orientation is fully 3D.
- Gravity, drag, tumble/flutter, small impact/bounce and short slide are lightweight packet mechanics rather than rigid-body marbles. High-rate events merge into a bounded packet pool.
- Each vehicle applies one analytical wake per world step. Speed, vehicle footprint, supported load/reference weight, ride height and an explicit future aero coefficient determine front/underbody/trailing influence.
- The wake conservatively moves established loose rubber between aggregate cells and can lift fresh/light material into moving packets. Moving visual packets receive matching wake impulses.
- Tire-contact sweep is conservative as well; material disappears only through modeled pickup, rain/washing, explicit reset or bounded world-state policy.
- Large-grid profiling remains a TIRE18 gate; wake frequency/candidate budgets may be LOD-decimated independently of the high-rate tire solver if measured performance requires it.

### Pre-TIRE16 modularization gate — COMPLETE

The focused CLEAN01-CLEAN13 architecture program is user validated. The 169-value positional telemetry
ABI is compatibility-only, wheel simulation has explicit phase ownership, SurfaceWorld is world-owned
and floating-origin safe, and tire authoring/runtime/build safety boundaries are established. The
architecture-only cleanup stop rule now applies: new restructuring requires a concrete feature blocker.
Tire/surface implementation resumes through TIRE15B/TIRE15C before the TIRE16 tire-mark presentation pass and later specialty families.

### TIRE16 - pressure-resolved continuous tire marks

- Hard-surface tire marks are connected wheel-owned FP64 trails, not independent square decals or one stamp per high-rate contact sample.
- Formation strength is continuous from dissipated slip work, slip speed, load, tread temperature, wetness and receiving material, so ordinary rolling remains nearly clean while lockup/wheelspin/sliding progressively darken the trace.
- TIRE08's three-band pressure/load distribution feeds each trail endpoint. Pressure, camber and load transfer can therefore make one shoulder/region subtly darker across the tire width.
- Each segment also stores independent start/end intensity, so changing slip conditions naturally create longitudinal darken/fade gradients while the mark remains geometrically continuous.
- Lateral shoulders feather smoothly to zero; only exposed trail ends receive soft longitudinal caps. Connected segments suppress their touching caps, explicitly avoiding rectangular stamp boundaries.
- Stationary wheelspin can layer sparse soft-capped footprints. Overlapping marks blend naturally.
- Dry marks age toward roughly half their fresh visibility rather than disappearing on an arbitrary timer; future explicit rain/abrasion/streaming policy may reduce them further.
- The current bounded segment cache is a presentation implementation. Endurance-scale persistence/streaming and large-grid profiling remain TIRE18 work.
- TIRE16B adds an explicit genuine-slide gate from tire-model slip ratio and slip angle: normal elastic cornering can use substantial grip without leaving a visible black trace, while lockup, wheelspin and high-angle sliding progressively activate transfer.
- TIRE16C keeps that gate but permits a tightly capped pre-slide shoulder whisper; fixes the old first-9000 visible-segment starvation, extends tire-mark range to 325 m with 9/5/3-sample distance LOD under a triangle budget, slows dry ageing toward the 50% floor, and uses a dedicated neutral-dark night shader so rubber does not turn brown.
- TIRE16D replaces the single nearest-first mark budget with independent 0–110 m / 110–260 m / 260–430 m / 430–650 m budgets. Near marks retain full pressure gradients, mid marks simplify them, and far/horizon marks become one solid-width quad at progressively reduced visibility (~50% at the horizon), preventing dense nearby history from starving distant parking-lot/track marks. It also forces pure-black tire-mark source RGB and raises production marble generation from 0.04 to 0.12 after the C5A calibration proved too sterile in multi-minute cornering tests.
- TIRE16E adopts the production 6/3/1/1/off mark LOD requested after visual testing: 0–100 m uses six lateral control samples, 100–200 m uses three, 200–300 m is one solid-width quad, 300–500 m is one solid-width quad with a reserved horizon budget, and 500+ m is culled. The FP64 tire-mark history budget doubles from 131,072 to 262,144 segments. Dry age now trends slowly toward a permanent 62% visibility floor while pressure-band contrast progressively softens, separating historical persistence from close-range definition. Far marks are intentionally less opaque so distant parking-lot history reads naturally without dominating the scene.
- TIRE16F keeps all TIRE16E FP64 history/persistence behavior but reduces presentation tessellation to the user-selected 4/2/1/off ladder: 0–100 m uses four lateral controls, 100–300 m uses two controls that preserve only left/right load asymmetry, 300–500 m is one solid-width two-triangle strip under its own horizon budget, and 500+ m is culled. This reduces close/mid geometry while preserving distant track history and never converts authoritative mark positioning to FP16.
- Tire-mark color is neutral charcoal/grayscale; hard-surface type may alter darkness but must not tint rubber brown under night lighting.

### TIRE17 - specialty tire families

**TIRE17A status:** Heritage now has a reusable `TireFamily` taxonomy and an explicit family-baseline generator for road summer/performance, racing slick, racing wet, winter, studded ice, rally gravel, motorcycle, kart, commercial/truck and low-pressure off-road tires. When measured/fitted property data is unavailable, section width, aspect ratio, rim radius, nominal load and inflation pressure seed a low-confidence `TireModelDescription`; family selection changes mechanism inputs such as tread void/drainage, winter compound/siping/studs, granular tread engagement, flotation, thermal target and wear/shedding tendency. Manufacturer/model strings never select performance. Current fitted vehicle tires are not replaced automatically by TIRE17A.

**TIRE17B status:** the bounded Dry/Wet/Snow-Ice/Mud/Sand/Gravel/Wear-Endurance authoring biases now have a versioned native mapping (`TirePerformanceBiasMapping`) into coherent pre-solver parameters. Dry adjusts the seed compound/stiffness/temperature envelope; Wet adjusts void/drainage/thin-film tolerance; Snow-Ice adjusts cold compound/siping/interlock/self-cleaning without inventing studs; Mud/Sand/Gravel tune the corresponding granular/deformable-terrain traits; Wear-Endurance adjusts abrasion, shedding and hot-temperature endurance with a small grip/life trade-off. Imported/fitted tire data is preserved rather than being overwritten by this estimated calibration layer. Runtime Parts Lab selection/assignment remains later TIRE17 work.

**TIRE17C status:** reusable tire parts now resolve into fitted runtime tire models through `TirePartResolver`. The part owns stable identity, family, engineering dimensions/load/reference pressure, creator biases and an optional authoritative `.tir` property file. Explicit property-file data outranks generated estimates; otherwise the TIRE17A family baseline + TIRE17B bias mapping produces an explicitly estimated runtime model. Vehicle fitment owns optional per-wheel cold pressure, so the same tire part can be reused at different front/rear/vehicle pressures without modifying the part asset. `VehicleSystem::assignWheelTirePart` applies the resolved model per wheel, records assignment identity/source, resets tire thermal/wear state, and direct low-level tire-model overrides deliberately clear that assignment identity. Topology-aware axle/group assignment and Parts Lab UI remain later TIRE17 work.

**TIRE17C1 visual/pressure follow-up:** the existing adaptive road envelope now retains its already-queried refined 3x3 support heights for local tire-mesh curb/step conformity; smooth support keeps the single-plane fast path. The Tire Lab exposes cold fitted pressure within the common tire validity range, while zero-pressure puncture/collapse remains deferred to tire damage. Rigid-ring physical state is unchanged, but extreme visual belt translation is bounded to prevent transient mesh tearing.

**TIRE17C2 three-axis visual follow-up:** tire presentation now treats carcass/belt deformation as three coupled but bounded axes rather than a flat contact-plane effect. Radial load forms the tread footprint and lower-sidewall bulge; rigid-ring longitudinal displacement visibly shears the belt under braking/drive; lateral displacement bends the carcass under cornering. The bead region stays anchored. VehicleSystem's authoritative world wheel-forward/right vectors are passed through the Entity tire-visual bridge so mirrored wheel nodes cannot invent or reverse the deformation basis. Main and shadow passes use the same geometry deformation.

TIRE17 also establishes reusable tire-part authoring. Tire dimensions/engineering metadata generate or
select a physically reasonable baseline; the normal creator workflow exposes bounded **Dry**, **Wet**,
**Snow/Ice**, **Mud**, **Sand**, **Gravel** and **Wear/Endurance** performance biases around that baseline
instead of direct final-force multipliers. These controls exist for every tire family because any vehicle
may encounter these surfaces; they are not an "off-road-only" feature. The simple Snow/Ice control may
share one authoring slider while Advanced data keeps distinct snow, slush, ice and stud mechanisms.
Measured/fitted data remains authoritative when available. Tire definitions are reusable vehicle parts and
manufacturer/model names never hard-code performance assumptions. See `TIRE_PART_AUTHORING_ROADMAP.md`
and ADR-055.

Complete reusable tire-family authoring for:

- road summer/performance tires;
- slick racing tires;
- wet racing tires;
- winter tires;
- studded ice tires;
- rally gravel tires;
- motorcycle tires;
- kart tires;
- truck/commercial tires;
- low-pressure ATV/off-road tires.

Reuse common MF6.2/SWIFT-like, thermal, pressure, wear and surface mechanisms where valid. Do not
force radically different carcass/terrain behavior through one parameter set merely for API
uniformity.

### TIRE18 - tire/surface laboratory, calibration and scalability gate

Before declaring the tire/contact program complete, add repeatable laboratory tools/regressions
for:

- pure/combined-slip curves and full moments;
- low-speed/turn-slip behavior;
- loaded/effective radius;
- curb/cleat/rough-road enveloping;
- rigid-ring modes;
- thermal/pressure endurance;
- 48-cell heat/wear patterns;
- flat-spot vibration;
- contamination/cleaning;
- damp-to-standing-water transitions and hydroplaning;
- split-mu and split-water footprints;
- gravel/dirt shear and sinkage;
- ice/snow behavior;
- mud/sand/deep-snow sinkage and rut evolution;
- motorcycle high-camber contact;
- performance tiers on large vehicle grids.

Measured/fitted data remains preferred. Estimated historical tire/surface data must retain
provenance/confidence rather than being presented as measured truth.

## Deferred until this program is complete

Wheel-fitment mass/clearance expansion, new suspension topology families, full motorcycle vehicle
dynamics, FFB, damper thermal state and other unrelated vehicle mechanisms remain valid roadmap
items, but they follow the tire + driven-surface completion sequence unless a blocker forces a
short detour.


## TIRE15C2 — marble maturity, large-circuit persistence and developer acceleration

- Persistent loose rubber carries a normalized maturity state: fresh shreds/flakes -> partially clumped -> mature marbles.
- Maturity comes from repeated contact, agitation, tack/temperature and concentration, not elapsed time alone.
- No hard tread-wear threshold controls shedding; wear is a continuous factor alongside stress, temperature and explicit tire `rubberSheddingPropensity`.
- Passing vehicles sweep/rearrange existing loose rubber; mature pieces are more mobile, while tire pickup and rain can remove it. Dry rubber does not disappear merely because time passes.
- Shared track state remains independent of vehicle count and uses a 524,288-cell active budget; near visuals remain capped, with aggregate medium/far LOD.
- Tire LAB development controls: wear speed, rubber/marble generation and maturity speed from 0-1000x, plus reset tire physical state and reset track rubber/marbles. All default to 1x and are non-authored runtime controls.
- Future large-grid benchmark: profile 150 vehicles on Nürburgring-scale content and, if needed, decimate rubber state writes independently from the high-rate tire solver while preserving integrated rates.

### TIRE15C3 — world-anchored marble visuals and removal of cell-mark presentation

- Procedural loose-rubber geometry uses a deterministic world-space surface basis rather than the latest wheel/contact heading. Existing marbles must not rotate when a vehicle steers through the same rubber cell.
- The temporary deposited-rubber per-cell streak/rectangle presentation is removed. Bonded-rubber physics remains intact; a future smooth material/racing-line renderer may visualize the field without exposing storage-cell boundaries.
- Fresh/mature procedural pieces are visually thicker and crossing/tangled strands occur more often at meaningful density/maturity so concentrations read as rubber chunks/clumps rather than thin hairs.


### TIRE15C5A — marble visual/calibration hotfix

User visual validation of C5 showed excessive production-rate debris, insufficient visible multiplicity at 1000x, and the 0.5 m storage lattice leaking into resting presentation. C5A keeps FP64 authoritative packet/cell coordinates and the aggregate 500k logical-piece architecture, but calibrates normal loose-rubber generation down to 4% of the C5 provisional rate, reconstructs transient visual multiplicity from logical piece population, gives fresh flakes a more readable short ballistic separation, and uses one common two-triangle two-sided flake primitive for moving/resting/far rubber. Resting representatives use cross-cell deterministic pile anchors and deliberate overlap/stacking so presentation is not clipped into one square per authoritative cell.

- TIRE16G retains the 4/2/1/off 0–500 m LOD but expands near/mid/horizon render populations so the 100–300 m band cannot disappear between visible near and horizon history. Fresh marks receive a short presentation-priority window, ground-history LOD uses horizontal FP64 distance, and coplanar marks use raster depth bias instead of millimetre-scale world lift. Abrupt kerb/support transitions break the ribbon rather than bridging through air. Low-speed lateral dragging is gated out of visible skid transfer while stationary longitudinal wheelspin remains supported. Production marble generation rises exactly 3x from 0.12 to 0.36 after user testing found the prior baseline too sparse.

**TIRE17C3 force-resolved visual correction:** TIRE17C2 exposed two presentation issues in live testing: a conservative centre-to-road measurement could overwrite the solved radial deflection and visually restore a round tire, while using rigid-ring translation too directly could make braking deformation read as tire/rim separation. TIRE17C3 keeps solved pneumatic radial deflection as a minimum visual compression, allows the measured/support planes only to request additional compression, and derives bounded longitudinal/lateral carcass shear from Fx/Fz and Fy/Fz blended with the transient rigid-ring state. Tangential motion is localised toward the road-facing tread and lower sidewall; the bead remains anchored.

**TIRE17C8/VIS11 exact-collider correctness correction:** TIRE17C7's real creator-triangle GPU constraint was wired end-to-end but mixed coordinate spaces: rendered tire vertices were camera-relative while uploaded collider vertices were still local-world. Main and layered-shadow collider uploads now subtract the render eye before the shader tests each tire vertex. The bounded nearby-triangle selection also ranks by exact closest-point-on-triangle distance, so very large road/kerb faces cannot be displaced from the 64-triangle reference-quality budget merely because their authored vertices and centroid are distant.

**TIRE17C9/VIS12 elastic exact-collider correction:** live C8 testing proved the real-triangle path was finally active but exposed the deeper deformation bug: the shader cumulatively projected a vertex through every qualifying nearby plane, so road/kerb/chamfer triangles could build an impossible spike, while any nonzero exact-triangle count disabled the smooth support-grid curb deformation. VIS12 evaluates all exact faces from the original deformed vertex, reconstructs/orients geometry against the rendered tire centre, uses metric triangle-edge proximity, retains only one dominant plus one independent corner constraint, redirects the correction primarily inward toward the rim, bounds it by physical sidewall span, and leaves the bead anchored. The smooth 3x3 curb field now remains active and exact geometry serves only as the final anti-penetration authority.
