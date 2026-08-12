# Current Project State

## Milestone status

**User-confirmed baseline:** Step 29J.6B — Adaptive Three-Column Topology Grid
**User-confirmed tire/visual checkpoint:** TIRE15 — persistent deformable-terrain terramechanics built, launched and driven in-game after the Lua local-limit and Lua C-stack hotfixes; TIRE14 through TIRE10/VIS02 remain user-confirmed.
**User-confirmed cleanup checkpoint:** CLEAN10 — world-owned chunked SurfaceWorld with FP64 global addressing and streaming seams builds, launches and drives correctly.
**Current candidate:** CLEAN11 — tire-property authoring responsibility split + reusable tire-part authoring contract.
**Architecture gate:** core cleanup CLEAN01-CLEAN04 is performed before TIRE16; see `CODE_MODULARIZATION_ROADMAP.md`.
**Safety-net note:** legacy `Vehicle.GetWheelState` remains compatibility-checked, while first-party Racing United telemetry migrates to the named `Vehicle.GetWheelTelemetry` table so future tire fields no longer extend a positional ABI.
**Post-CLEAN08 inspection plan:** CLEAN09-CLEAN13 finish runtime ownership/build hygiene, world-owned chunked surface state, tire-part authoring boundaries, Lua-runtime/validator boundaries and the last clear large-file ownership splits before returning to tire milestones.
**Tire-part authoring direction:** tires become reusable vehicle parts with geometry/engineering-derived baselines plus bounded Dry, Wet, Snow/Ice, Mud, Sand, Gravel and Wear/Endurance creator biases; the controls are available to every tire family and nudge physical mechanisms rather than multiplying final force. See `TIRE_PART_AUTHORING_ROADMAP.md` and ADR-055. Tire marbles remain a specialized rubber subsystem even when bulk loose-rubber concentration shares world spatial infrastructure.

### CLEAN11 — tire-part authoring architecture — CURRENT CANDIDATE (2026-08-10)

The former 1,243-line `Vehicles/Tires/MagicFormula/TirePropertyFile.cpp` is now a small public load/parse façade. Compiled authoring owners under `Vehicles/Tires/Authoring/` separately own raw `.tir` parsing/units, generic property mapping, Magic Formula coefficient mapping, common tire metadata, Heritage clean-room extension metadata, structural/enveloping metadata, and diagnostics/final validation. The public `TirePropertyFile.hpp` contract and imported physical behavior are intentionally unchanged.

A reusable compiled `TirePartDefinition` authoring contract now reserves Dry, Wet, Snow/Ice, Mud, Sand, Gravel and Wear/Endurance biases in the normalized [-1,+1] creator range. Zero means the future geometry/construction-derived average baseline. The biases are explicitly parameter-generation inputs and are not direct final-force multipliers. No vehicle runtime consumes the bias contract yet; that remains Tire/Vehicle Parts Lab work. See ADR-058 and `TIRE_PART_AUTHORING_ROADMAP.md`.

### CLEAN10 — world-owned driven-surface state — USER VALIDATED (2026-08-10)

CLEAN10 moves persistent driven-surface state out of `VehicleSystem` and into a `PhysicsWorld`-owned `Physics/Surfaces/SurfaceWorld`. Vehicle/tire simulation now consumes that shared world service instead of owning private terrain memory. `SurfaceWorld` is the one local-FP32 -> global-FP64 addressing boundary, so floating-origin rebases change local coordinates without re-keying or losing driven-surface history. The deformable `SurfaceField` implementation moved to `Physics/Surfaces/SurfaceField.*`; the old `Physics/SurfaceField.hpp` remains a compatibility include and the old `.cpp` is non-compiled.

The original flat 16,384-cell sparse map is replaced by sparse cells grouped into bounded LRU chunks. Defaults are 0.25 m X/Z cells, 64 cells per chunk edge (16 m tiles), a 2 m coarse vertical layer to keep bridges/tunnels distinct, 262,144 resident cells and 8,192 resident chunks. Chunk eviction is LRU and never scans the whole world for the oldest cell. Chunk snapshots, restore support and an eviction callback provide a persistence/streaming seam without imposing a file/network format. A dedicated non-compiled `Physics/Surfaces/Rubber/TrackRubberState.*` scaffold reserves TIRE15C rubbering/marbles as a specialized subsystem rather than pretending tire marbles are generic deformable terrain. A native regression verifies that the same absolute surface cell survives a simulated floating-origin shift and that chunk eviction/restore remains bounded. See ADR-057.

### CLEAN09 — runtime phase ownership + incremental build loop — USER VALIDATED (2026-08-10)

The CLEAN07 runtime scaffolds are now compiled owners. `EngineFrame` owns frame timing/input/presentation; `EngineHotkeys` owns developer/application key edges; `EngineSimulation` owns fixed stepping/module/environment/entity-camera update; and `EngineRendering` owns render targets, AA/scale/span preparation, GPU timing and scene rendering. Process-lifetime settings/core services moved out of anonymous globals into private `HeritageEngine`-owned `EngineRuntimeState`, while frame/render/hotkey transient state remains responsibility-specific rather than one giant mutable context. The rolling build helper now defaults to incremental `/t:Build`; invoke `00_BuildAndRunCurrent.cmd FULL` for an explicit full rebuild. See ADR-056.

### CLEAN08 — Lua responsibility cleanup — USER VALIDATED (2026-08-10)

Racing United Lua now follows the same subsystem-ownership rule established by the native cleanup.
Suspension authoring is split into source/provenance, estimation, native activation and gizmo ownership;
wheel presentation is split into transform math, articulated wheels, embedded GLB binding and frame
coordination; VehicleDefinitionV2 separates structural/core validation, dynamics/component validity and
current-native-solver compatibility. `Runtime/Lifecycle.lua` now dispatches physics-demo fixed-step work,
and `Runtime/Common.lua` no longer implements physics-demo teardown. Root suspension/visual-wheel paths
remain tiny compatibility load coordinators. No tire or vehicle physics equations are intentionally changed.

### CLEAN07 — Heritage Engine shell + domain-owned Lua registration — USER VALIDATED (2026-08-10)

The compiled executable `main.cpp` is reduced to a tiny entry point that constructs
`heritage::engine::HeritageEngine` and calls `run(argc, argv)`. Stable process-level responsibilities
are extracted into `EngineStartup`, `EngineUiStyle`, `DisplayModeController`, `PerformanceOverlay`
and the Windows `BackbufferClipboard` bridge. `HeritageEngine.cpp` remains the process/frame
coordinator for now; non-compiled EngineFrame/EngineSimulation/EngineRendering/EngineHotkeys files
make the intended future phase ownership visible without inventing a giant shared mutable context
just to move lines.

Lua API registration is now domain-owned. `LuaModuleRuntime::registerBindings()` retains only the
engine-owned `print` override and ordered `register*Bindings()` calls. UI/Input/etc. register beside
their handlers; Entity/Physics/Vehicle have dedicated registration units. The Lua manifest generator
scans distributed registration sources and still verifies exact names and handler ownership. See
ADR-053 and `CODE_MODULARIZATION_ROADMAP.md`.

### CLEAN06 — input + glTF responsibility split — USER VALIDATED (2026-08-10)

The user confirmed the CLEAN06 Windows build launches and runs correctly after `InputSystem.cpp` was
split into lifecycle/bindings/devices/profiles/persistence ownership and the glTF importer was split
into JSON/document/mesh/metadata/collision responsibilities behind the stable `GltfBinary.hpp` API.
Current vehicle and scene loading/input behavior remains correct.

### CLEAN05 — entity mesh renderer split + High shadows — USER VALIDATED (2026-08-10)

The renderer is split into root orchestration, asset cache, animation, shadows, render math and shader
ownership. Default cascaded shadow-map resolution is now the centralized High preset at 3072x3072 per
cascade. The user confirmed the Windows build launches and runs.

### CLEAN04B — collision responsibility split — USER VALIDATED (2026-08-10)

The monolithic collision implementation is split into system orchestration, queries, narrowphase,
solver, CCD and island/sleep ownership. The user confirmed build, launch, scene collision and driving.

### CLEAN04A — shared quaternion foundation + collision scaffold — USER VALIDATED (2026-08-10)

Quaternion representation/algebra previously duplicated across entity hierarchy, rigid-body physics,
collision and vehicle simulation is now owned by `Core/Math/Quaternion.hpp`. Subsystem wrappers keep
their previous normalization and coordinate-space semantics, so this step centralizes convention
without forcing unrelated policy into a generic math API. `TransformMath.hpp` is an intentionally
small future ownership point rather than a new dumping ground. CLEAN04A also creates non-compiled
Queries/Narrowphase/Solver/CCD/Islands collision destinations so CLEAN04B can perform a mechanical
file split against explicit boundaries. The complete portable native physics regression output was byte-identical to the user-validated
CLEAN03B baseline, and the subsequent Windows build/launch/drive validation also passed.

### CLEAN03B — wheel-substep phase partition — USER VALIDATED (2026-08-10)

The previously intact ~2,787-line `VehicleSystem::simulateWheelSubstep()` is now a small ordered
orchestrator backed by eleven named function-scope phase fragments under
`Vehicles/Simulation/WheelSubstep/`. This is intentionally a zero-equation, zero-reordering
cleanup: the fragments share the original lexical scope so existing cross-phase intermediates,
early-return behavior and floating-point evaluation order are preserved exactly. Provider logic
remains in the existing tire/surface modules rather than being copied into the orchestrator.

The phase contract explicitly remains per-wheel and arbitrary-wheel-count. Cars, karts, ATVs,
multi-axle trucks, motorcycles and future trikes reuse common wheel/tire mechanisms where physically
valid; lean/balance or other topology-specific whole-vehicle coupling remains under
`Vehicles/Topology/`. The portable native regression suite passes and its complete output is
byte-identical to the user-validated CLEAN03A baseline. The Windows build launches and the Peugeot drives normally; CLEAN03B is user validated. See `CODE_MODULARIZATION_ROADMAP.md` and
`Vehicles/Simulation/WheelSubstep/README.md`.

### CLEAN03A — subsystem ownership and topology scaffold — USER VALIDATED (2026-08-10)

CLEAN03A removes the remaining 1,176-line catch-all `VehicleConfiguration.cpp` from active
compilation and redistributes its existing member-function definitions without changing their
bodies. Suspension, fitment, alignment, anti-roll bars, chassis compliance, unsprung mass,
runtime inputs/tuning, steering, brakes, driver aids, drivetrain and tire/surface configuration now
live beside the subsystem they configure. The old root file remains only as a non-compiled signpost.

The same step adds explicit future topology destinations for common arbitrary-wheel-count vehicles,
two-wheel vehicles, three-wheel vehicles and four-plus-wheel/multi-axle vehicles, plus matching Lua
authoring scaffolds. This is architecture only: no motorcycle/trike/truck behavior is enabled and no
current Peugeot physics is changed. See `VEHICLE_TOPOLOGY_ARCHITECTURE.md` and
`CODE_MODULARIZATION_ROADMAP.md`.

### CLEAN02 — physical VehicleSystem responsibility split — USER VALIDATED (2026-08-10)

The former roughly 5,974-line `VehicleSystem.cpp` is split into lifetime/plumbing, configuration,
telemetry, vehicle-level simulation and authoritative wheel simulation translation units. The user
confirmed the Windows build launches and the Peugeot drives normally after the validator hotfix.
CLEAN03A now refines the broad configuration bucket before CLEAN03B decomposes the giant wheel
substep.

### CLEAN01 — named wheel telemetry table — USER VALIDATED (2026-08-10)

TIRE15 exposed two separate scaling failures in the historical positional telemetry API: the Lua
consumer exceeded Lua 5.4's local-variable limit, and the native binding later required an explicit
169-slot C-stack reservation. CLEAN01 adds `Vehicle.GetWheelTelemetry(vehicle, wheelIndex)`, which
returns one named table containing the complete wheel state plus contact-query diagnostics and
upright pose. Racing United's `Vehicles/Telemetry.lua` now consumes that table directly. Existing
field names used by UI and wheel presentation are preserved; no tire/vehicle physics equation is
changed.

`Vehicle.GetWheelState`, `Vehicle.GetWheelContactDiagnostic` and `Vehicle.GetWheelUprightPose`
remain registered as legacy compatibility APIs. Future first-party tire/surface diagnostics extend
the named table instead of increasing the positional return count. See ADR-050 and
`CODE_MODULARIZATION_ROADMAP.md`.

### TIRE15 — persistent deformable terrain / SurfaceField terramechanics (2026-08-10)

**Windows Lua hotfix history:** TIRE15 expanded `Vehicle.GetWheelState` to 169 positional values.
Direct unpacking first exceeded Lua 5.4's 200-local-per-function compiler limit; after that was
packed into one Lua table, the native C binding still needed `lua_checkstack(state, 169)` to avoid a
hard runtime crash while pushing the payload. Those hotfixes remain on the legacy API. CLEAN01 now
removes first-party dependence on that positional path entirely through `Vehicle.GetWheelTelemetry`.

TIRE15 originally introduced persistent `SurfaceField` state together with
`Vehicles/Tires/TireDeformableTerrainInteraction.*` as the dedicated ground-reaction provider for
mud, sand, soft soil and deep snow. CLEAN10 promotes the field into the world-owned
`Physics/Surfaces/SurfaceWorld` + `Physics/Surfaces/SurfaceField.*` architecture. The tire still retains MF6.2/SWIFT geometry, pressure, thermal,
wear and transient state, but on fully deformable ground MF's direct hard-interface contribution is
reduced and pressure-sinkage/shear/plowing mechanics become primary. The clean-room provider uses a
Bekker-style pressure-sinkage relation, Mohr-Coulomb-style shear strength, Janosi/Hanamoto-style
shear mobilization, passive-wedge bulldozing and bounded plowing/compaction resistance.

The CLEAN10 SurfaceField quantizes stable FP64 global X/Z into 0.25 m cells grouped inside sparse 16 m chunks, with a coarse 2 m global-Y layer so vertically stacked roads do not alias. Each persistent cell remains keyed by surface material and stores loose depth, compaction, moisture, rut depth, longitudinal/lateral shear history, displaced volume and approximate wheel-pass count. Resident cells/chunks are bounded and chunk-LRU eviction avoids full-field scans; chunk snapshot/restore callbacks form the streaming/persistence seam. Wheel contact updates the
shared field at the high-rate tire cadence with timestep-stable bounded increments; following wheels
and vehicles query the modified state. Rut depth also changes the real support datum, so persistence
is physical rather than telemetry-only. Current material constants are synthetic compatibility
presets until authored SurfaceMaterial/SurfaceField data replaces them. Visible mesh rut deformation,
weather recovery/erosion and network replication remain separate future world/presentation layers.
See ADR-049.

### TIRE14 — shallow granular gravel and hard dirt (2026-08-10)

TIRE14 promotes `Vehicles/Tires/TireShallowGranularInteraction.*` into the active wheel path for
rally-style gravel and hard dirt where a shallow loose layer sits above a load-bearing base. MF6.2/SWIFT
remains the pneumatic tire core; the surface provider contributes physical loose-layer interaction around
that one tire evaluation rather than replacing the road with a constant low-friction number.

The current clean-room reduced-order model combines contact-pressure-dependent sinkage, bounded by the
loose-layer depth, with Mohr-Coulomb-style available shear strength and Janosi/Hanamoto-style
shear-displacement mobilization. Tire-authored tread aggressiveness, biting-edge density, open-void ratio,
remaining tread depth and granular coupling determine how much of that terrain strength the tire can use.
Lateral slip additionally produces passive-wedge bulldozing/plowing resistance; longitudinal motion pays
plowing/compaction drag and energy. The existing TIRE06 footprint fractions allow partial gravel/dirt
contact, and calculated sinkage changes the actual hub/contact support datum so the tire physically settles
into the loose layer.

The gravel/dirt material numbers are synthetic compatibility presets until `SurfaceMaterial` /
`SurfaceField` owns loose-layer depth, density, cohesion, friction angle, shear modulus, moisture and
compaction. `.tir` files intentionally contain only tire/tread traits. TIRE14 does not store permanent ruts
or terrain deformation; persistent height/compaction/moisture/shear history is TIRE15. See ADR-048.

### TIRE13 — compacted snow, hard ice and winter tire mechanisms (2026-08-10)

TIRE13 adds compiled `Vehicles/Tires/TireWinterSurfaceInteraction.*` around the existing one-MF6.2-
evaluation hard-surface path. Adaptive footprint samples preserve independent snow and ice fractions
rather than folding them into one friction scalar. Hard ice uses a bounded temperature/slip response:
near-melt conditions and slip/flash heating can grow a thin interface melt film that reduces rubber
traction, while tire-authored winter compound and siping provide separate contributions. Optional
studs are explicit count/protrusion data and add a bounded mechanical ice contribution instead of a
generic `studded=true` grip multiplier.

Compacted snow retains a load-bearing base and adds tread-block/sipe mechanical interlock. The TIRE08
16x3 tread state now also stores packed-snow fraction in material-fixed cells; contacting snow packs
those cells, while rotation/speed/slip and tire-authored self-cleaning shed it. Deep snow remains out
of scope for this provider and belongs to TIRE15 terramechanics.

The provider already accepts local surface temperature, but current static scenes use an explicit
-5 C compatibility value until the future scene `SurfaceField` supplies dynamic local temperature.
Prototype Peugeot winter coefficients are synthetic low-capability road-tire placeholders, not
measured Pirelli winter-tire data. See ADR-046.

### Tire + surface authoring decision (2026-08-10)

Tire render meshes and engineering behavior are separate. High-poly tread may be baked to low-poly
geometry while `.tir` / Heritage metadata remains authoritative for drainage, tread depth/voids,
siping, winter compound, studs and future loose-surface tread traits. These authoring values feed
mechanisms instead of acting as direct per-surface grip multipliers.

Scene authoring will preserve one local `SurfaceMaterial` identity from Blender vertex/material/
metadata authoring into render/collision/surface queries, audio, particles and weather. A future
spatial `SurfaceField` layers dynamic water depth, temperature, rubber/debris, loose depth, snow,
moisture, compaction/ruts and shear history over that identity. Collider simplification must not
erase precise physical surface boundaries. See ADR-047 and `TIRE_SURFACE_ROADMAP.md`.


### Brake-held parked steering wake hotfix (2026-08-10)

A controller-reported steering freeze was traced to the parked-rest optimization in `VehicleSystem::simulate`. Once the chassis entered sleep while the service brake was held, the function skipped all high-rate vehicle substeps unless throttle or an incline-required brake release requested a wake. Steering input therefore remained visible at the input layer but `updateSteeringSubstep`, TIRE03 parking torsion and upright presentation stopped advancing until another wake condition occurred.

The rest gate now treats steering as an active high-rate tire/suspension operation: a sleeping chassis wakes whenever commanded road-wheel center angle differs from the current steering state, and a vehicle cannot enter parked sleep while steering is still travelling toward its command. A native regression reproduces the exact case (parked + sleeping + service brake held + steer right then left) and requires the body to wake and both steering directions to track without releasing the brake. This is an optimization/wake fix, not a brake-force or steering-force model change.

### TIRE12 — wet pavement, tread drainage and progressive hydroplaning (2026-08-10)

TIRE12 adds a compiled `Vehicles/Tires/TireWetSurfaceInteraction.*` provider around the existing
hard-surface MF6.2/SWIFT stack. It is a Heritage clean-room hydrodynamic layer, not a claim of
proprietary Simcenter Tire 2512 wet-model parity. The current scene/collider `surfaceWetness` value
acts as a compatibility bridge to authored water-film depth (the synthetic Peugeot development
`.tir` files currently map wetness 1.0 to 3 mm), while the provider itself works in physical metres.

The provider combines adaptive-footprint wetness, speed/slip, normal load, dynamic inflation
pressure, contact-patch area/width/length, remaining tread depth and tire-authored void/drainage
capacity. Water inflow is compared with bounded groove evacuation capacity to form drainage-demand
and water-wedge state. Hydrodynamic lift then progressively unloads the pavement contact instead of
switching grip off at one threshold; thin-film lubrication begins earlier, and water-plowing drag
remains a fluid force outside the dry pavement friction circle. A classical pressure-based
hydroplaning-speed estimate is exposed as telemetry/diagnostic only, not used as the force switch.

TIRE12 also extends every TIRE08 16x3 tread cell with retained-water film state. The currently
contacting material-fixed cells pick up water and shed it progressively with time/rotation/speed,
without creating 48 MF evaluations. For hard surfaces, `VehicleSystem` restores the dry base
material profile before applying TIRE12 so the historical scalar wet-friction multiplier is not
double-counted. Non-hard materials retain their previous wet behavior until their dedicated
TIRE13-TIRE15 providers supersede it. Native regression covers flooded versus thin-film contact,
tread-depth drainage sensitivity, inflation-pressure sensitivity and wet-to-dry retained-water
release. See ADR-045.

### TIRE11 — 48-cell tread contamination, pickup and self-cleaning (2026-08-10)

TIRE11 promotes `Vehicles/Tires/TireSurfaceInteraction.*` from an empty architecture scaffold into
a compiled tire/surface state mechanism. Each existing TIRE08 tread cell can now retain independent
organic/grass contamination, mineral dirt/dust, gravel fines, rubber pickup/marbles and mud-film
fractions. Material is deposited only into the currently contacting material-fixed sectors and
lateral bands, so contamination rotates with the tread rather than existing as one global dirty-tire
percentage. The same shared 16x3 contact-weight helper is used by wear and contamination to prevent
future subsystems from disagreeing about which tread cells are physically in contact.

Grass, dirt and gravel collision materials expose bounded pickup sources; TIRE06 adaptive-footprint
material fractions feed those sources, so an outer footprint probe can begin pickup before the centre
ray crosses the boundary. Wet grass/dirt/gravel can also seed the mud-film groundwork. Clean asphalt/kerb/painted-line/default hard surfaces progressively
clean the exact sectors passing through contact. Forward speed, contact slip and hot tread increase
mechanical release. A separate `surfaceRubberDebrisFraction` input is already available for future
dynamic-track marbles/rubber without inventing a new collision material. Snow/ice pickup is deferred
to TIRE13 rather than being hidden inside this mechanism.

The active spatial contamination blend feeds the existing one-MF-per-tire path through a bounded
friction scale, rolling-resistance scale and tread-to-road thermal-contact scale. It does not create
48 MF6.2 evaluations. Racing United's current `[HERITAGE_CONTAMINATION]` coefficients are explicitly
synthetic development data, not measured Peugeot/Pirelli values. Native regression covers wet-grass
pickup, spatial retention, asphalt self-cleaning and independent rubber-debris pickup. See ADR-044.

### TIRE10 / VIS02 — physical flat-spot radius and authoritative visual contact plane (2026-08-10)

TIRE10 makes the TIRE08 wear field geometric. Average tread loss and the current material-fixed
contact-sector loss are derived from the 16x3 state. The active support radius used to convert
the road ray into the wheel hub datum is reduced by the current contact tread loss, and TIRE04's
unloaded/effective-radius calculation receives the same worn-radius datum. A localized flat spot
therefore produces real periodic tire-deflection/normal-load/unsprung/suspension excitation as it
rotates; no synthetic vibration force is added. Uniform wear also reduces rolling radius.

VIS02 passes the native road-contact normal and measured wheel-center-to-contact-plane distance to
the existing GPU tire presentation bridge. Main and shadow shaders flatten the road-facing tread
against that physical plane rather than deriving the contact direction only from world gravity.
This is the correct basis for banking, crossfall and irregular contact and remains downstream of
physics. See ADR-043.

The tire program is now intentionally followed by the complete driven-surface sequence documented
in `TIRE_SURFACE_ROADMAP.md`: contamination/cleaning, wet-film/hydroplaning, ice/compacted snow,
shallow granular gravel/dirt, deformable terramechanics, specialty tire families and a final
calibration/performance gate before unrelated vehicle-domain expansion resumes.

### TIRE09 / VIS01 — physics-driven GPU tire visual deformation (2026-08-09)

Heritage now has a presentation bridge from authoritative native tire state to the actual
rendered GLB tire mesh. The current Peugeot 206 RC asset contains separate `WH_FL_Tire`,
`WH_FR_Tire`, `WH_RL_Tire` and `WH_RR_Tire` nodes, so no asset rewrite or tire-specific rig is
required. `Mesh.cpp` detects tire/tyre nodes and derives their centre, axle axis, section width,
inner/bead radius and outer radius directly from indexed geometry at asset upload. Inspection of
the current provisional Peugeot mesh yields roughly 205 mm width and 595 mm diameter, close
enough to the authored 205/40 R17 envelope for this visual prototype. Explicit engineering
metadata remains authoritative over the detailed mesh shape.

`Entity.SetMeshNodeTireDeformation` carries the compact per-wheel presentation state. The main
and shadow vertex paths use live radial deflection/contact-patch size to flatten only the
road-facing tread and bulge the lower sidewalls while keeping the bead/rim relationship nearly
rigid. SWIFT-like ring longitudinal/lateral/radial displacement, yaw and wind-up provide bounded
outer-carcass movement. TIRE08 now reports the deepest material-fixed circumferential wear sector
as well as depth, allowing the corresponding sector of the rotating mesh to show a genuine local
flat-spot dent. The GLB itself is untouched and wheel rims/brakes remain rigid.

This is deliberately a GPU presentation mechanism rather than a soft-body physics mesh, so
rendered vertex count does not multiply the high-rate tire solver cost. The underlying tire,
contact patch, ring, temperature and wear state remains authoritative. At the TIRE09/VIS01 checkpoint, physical flat-spot radius/vibration coupling was still deferred
and the first visual contact direction used gravity projected into tire-local space. TIRE10/VIS02
now supersedes both limitations with physical tread-radius coupling and the native road-contact
plane. See ADR-042 and ADR-043.


### TIRE08 — 48-cell spatial tread temperature, wear and flat-spot groundwork (2026-08-09)

Heritage now promotes the previous `TireWear.cpp` architecture scaffold into a compiled
spatial tread-state mechanism. Each tire owns 16 circumferential sectors x 3 lateral bands
(inside/center/outside), for 48 cheap local state cells. These cells do not run independent
Magic Formula solvers: the tire still performs one MF6.2/SWIFT force evaluation while the
cell field retains local surface-temperature deviation and remaining tread depth. This keeps
the architecture suitable for large race grids while preserving the information needed for
local heating, shoulder wear and flat spotting.

TIRE07 remains authoritative for mean tread/carcass/gas energy and inflation pressure. TIRE08
stores surface-temperature offsets relative to that bulk tread state, deposits a bounded share
of slip energy into the currently contacting circumferential sector and three pressure/camber
weighted lateral bands, then diffuses/relaxes the local deviations. The mean offset is kept
energy-neutral so the spatial overlay does not double-count the bulk heat already integrated
by TIRE07. Early wear has deliberately small force effect; severe tread depletion and local
flat spotting progressively reduce the contact friction multiplier. Physical flat-spot radius
and vibration coupling are intentionally deferred rather than hidden in the wear model.

A locked/sliding wheel naturally keeps dissipating energy into nearly the same sector and can
therefore create localized wear. A rotating tire moves the contact sector with wheel rotation
and spreads wear around the circumference. Inflation pressure shifts the three-band load
distribution between centre and shoulders, while camber shifts load from one shoulder toward
the other. Racing United's synthetic prototype `.tir` files author the independent
`[HERITAGE_TREAD_STATE]` section; these are development values, not measured historical
Pirelli data and not proprietary Live for Speed/Simcenter coefficients. Aggregate telemetry
exposes inside/center/outside surface temperature, tread depths, minimum depth, overall wear,
flat-spot depth, active/hottest sector and the spatial friction scale. Native regression covers
localized lock-up wear, rotating wear distribution, pressure/camber band weighting and
timestep stability.


### TIRE07 — tire thermal energy state and inflation-pressure feedback (2026-08-09)

Heritage now owns a separate compiled `TireThermal.*` mechanism rather than treating tire
temperature as a static coefficient. The current clean-room model uses three lumped energy
states: tread, carcass and contained gas. Slip work supplies tread/carcass heat; radial tire
damping and rolling-resistance loss supply carcass heat; configurable conduction exchanges
energy among tread, carcass, road, gas and ambient air. Air cooling increases with vehicle
speed, and airborne wheels continue to cool instead of freezing their thermal state when the
main road contact is absent. The thermal state advances in the normal 1000 Hz tire loop.

Contained-gas pressure evolves from gas temperature with an ideal-gas absolute-pressure
relation, reported externally as gauge pressure. Dynamic inflation pressure feeds the existing
MF6.2 operating-point input and TIRE04 contact-geometry calculation. Tread temperature applies
a bounded grip multiplier around an authored optimum while carcass temperature applies a
bounded stiffness multiplier. Both modifiers are normalized to the tire's reference
temperature, so enabling TIRE07 does not introduce an arbitrary force discontinuity at the
authored reference condition. Thermal feedback is causal: energy generated during one high-rate
step updates the state used by the following step.

Racing United's prototype `.tir` files carry a `[HERITAGE_THERMAL]` section with explicitly
synthetic heat capacities, conductances and response curves. This is Heritage-owned authoring
data, not a claim that proprietary Simcenter Temperature & Velocity equations have been
reproduced and not measured Pirelli data. Imported FITTYP70 property files still preserve their
identity without silently mapping unknown proprietary T&V coefficients onto this clean-room
network. Live tire telemetry now exposes tread/carcass/gas temperature, dynamic pressure,
thermal grip/stiffness scales, slip/carcass dissipation and road/air heat flow. Native
regression verifies heating, pressure rise, thermal grip/stiffness response and timestep
stability while retaining every previous vehicle/tire regression. See ADR-040.


### TIRE06 — adaptive 2D footprint and rotational rigid-ring modes (2026-08-09)

Heritage now expands the TIRE05 longitudinal road envelope into a bounded adaptive 2D
footprint. `TireRoadEnveloping` builds a low-cost centre cross during smooth homogeneous
contact and refines to the complete requested lattice when height, support, material or wetness
discontinuities appear. The current Racing United development `.tir` files request a 3x3
lattice, so ordinary contact uses five footprint locations and complex contact uses nine.
Sample axes are projected into the suspension/support-ray plane and the smooth local road
plane is removed in both longitudinal and lateral directions, preventing normal grade or
crossfall from being interpreted as tire roughness.

Additional road queries are intentionally decoupled from the 1000 Hz structural state: quiet
contact defaults to 125 Hz sampling and complex/refined contact to 250 Hz. The footprint
reports support fraction, roughness range, cross-slope and surface spread. Supported sample
materials/wetness are converted through the existing surface-profile system and aggregated
into friction, stiffness, rolling-resistance and relaxation multipliers before the single
MF6.2 tire evaluation. TIRE06 therefore supports split-surface influence without multiplying
Magic Formula evaluations per footprint cell. Distributed per-cell shear/force integration
remains an optional higher-cost future provider rather than the baseline for large grids.

`TireRigidRing` now activates the previously preserved yaw and wind-up rotational modes in
addition to longitudinal/lateral/radial translation. Belt diametral/polar inertia, yaw
stiffness, structural frequencies and damping are read from the existing `.tir` data. The
previous aligning moment excites ring yaw; longitudinal tire reaction torque excites belt
wind-up. Ring yaw and wind-up rate feed the next high-rate slip kinematics so these states
are dynamically connected rather than telemetry-only. Live tire diagnostics expose footprint
sample/refinement/surface state together with ring yaw/wind-up angle and rate. Native
regressions cover 2D pattern/refinement geometry, cross-slope/support behavior and rotational
ring timestep stability while retaining every previous vehicle stability contract.

This remains a clean-room implementation of public MF-Swift architectural concepts and
parameter vocabulary. It does not claim proprietary Simcenter numerical parity. See ADR-039.



### TIRE05 — SWIFT-like rigid-ring dynamics and road enveloping (2026-08-09)

Heritage now has an explicit tire-structure layer between the rim/road geometry and the
steady-state MF6.2 force law. `TireRigidRing` carries independent longitudinal, lateral and
radial belt/ring displacement and velocity states using identified structural stiffness,
modal-frequency and damping data imported from public MF-Swift-style `[STRUCTURAL]`
parameters. The active TIRE05 production branch intentionally limits structural DOFs to
translation; yaw and wind-up parameters are parsed/preserved for later rotational coupling.

`TireRoadEnveloping` is a clean-room tandem-cam-inspired finite-footprint road filter. Two
auxiliary front/rear support probes are evaluated relative to the local center-contact road
plane so an ordinary smooth grade does not masquerade as a bump. Short obstacles feed an
effective road height/slope into the radial ring mode, while ring longitudinal/lateral
velocities alter the slip kinematics seen by MF6.2. The structural state remains at the
normal 1000 Hz vehicle rate; extra road probes are cached at 250 Hz to bound collision-query
cost for eventual large grids. This is an independently implemented public-architecture
branch, not a claim of proprietary Simcenter Tire 2512 numerical parity.

Racing United's synthetic prototype `.tir` datasets now carry explicit belt mass, structural
stiffness/frequencies/damping and enveloping seed parameters. Live tire telemetry exposes
enveloped road offset/slope/sample count together with radial/longitudinal/lateral ring
offsets and velocities. Native regression coverage verifies timestep-stable rigid-ring modes,
short-obstacle enveloping, the complete previous tire stack and all established vehicle/
suspension stability contracts.

### TIRE04 — loaded/effective rolling radius and finite contact geometry (2026-08-09)

Heritage now separates quasi-static tire geometry from both the MF steady-state force law
and TIRE03's stateful tread-torsion mechanism. `TireContactGeometry.*` is a compiled
provider that evaluates unloaded/free, loaded and effective rolling radii per wheel in the
high-rate vehicle path. When imported MF data supplies `BREFF`, `DREFF`, `FREFF`,
`Q_RE0` and `Q_V1`, the provider uses the public MF load/velocity-dependent effective-
radius relation. The unsprung-mass path supplies its authoritative radial deflection; the
massless compatibility path may infer quasi-static deflection from `Fz/Cz` strictly for
tire geometry without changing suspension support geometry or ride height.

The `.tir` layer now maps `[CONTACT_PATCH]` `Q_RA1/Q_RA2/Q_RB1/Q_RB2` and the
rolling-radius coefficients. TIRE04 activates the documented square-root + linear contact-
length relation from `Q_RA1/Q_RA2`. `Q_RB1/Q_RB2` are preserved but deliberately not
evaluated until an authoritative public width equation is available. Instead, Heritage
constructs a bounded finite width/area from the first-order pneumatic footprint
`area ~= Fz / inflationPressure`, represented as an ellipse with the calculated length.
This is an explicit Heritage clean-room approximation, not claimed MF-Swift width parity.

Effective rolling radius now participates in wheel circumferential speed/slip-ratio
kinematics and in the longitudinal tire reaction lever arm. The MF force provider still
receives unloaded radius as its reference `R0`. Current suspension ray/support geometry
remains unchanged; the finite footprint is the data boundary for the next structural/road-
enveloping milestone rather than a disguised multi-ray contact hack. Racing United's tire
live panel exposes free/loaded/effective radii plus footprint length, width and area. Native
regression coverage verifies load/speed trends, explicit radial-deflection authority, finite
footprint behavior, `.tir` parameter transfer and all previous vehicle stability contracts.

### TIRE03 — MF6.2 turn-slip, parking torsion and transient slip (2026-08-09)

Heritage now carries MF6.2 turn-slip as an explicit per-wheel input rather than treating
all yawing/parking behavior as ordinary lateral slip. `TireContactPatch` is promoted from
scaffold into a stateful low-speed mechanism: steering/chassis yaw winds elastic tread
torsion at standstill, rolling distance releases that torsion, and a regularized turn-slip
quantity bridges through zero longitudinal speed without dividing by zero. The zero-speed
turning-moment coefficient (`QCRP1`) together with `LMP` drives a bounded parking aligning moment; rolling turn-slip
feeds the MF force/moment provider separately.

The MF6.2 data layer now maps the public `[TURNSLIP_COEFFICIENTS]` family. Heritage
currently activates peak Fx/Fy spin reduction, cornering/camber-stiffness reduction,
pneumatic-trail reduction, residual spin-torque reduction and rolling turn-slip moment.
`PHYP*` lateral-shift coefficients are preserved in the parameter model but remain reserved
for equation-parity validation rather than being assigned an invented interpretation.
This remains a clean-room public-equation branch, not a claim of proprietary Simcenter
MF-Swift parity.

Imported `PTX1..3`, `PTY1..2` and `LSGKP/LSGAL` now participate in the high-rate transient
path. Longitudinal/lateral relaxation lengths are derived from load, nominal load, radius,
and camber where the public coefficient set is sufficient; malformed or absent data falls
back to the existing per-wheel engineering relaxation lengths. The state integration
remains exponential/rate-stable in the normal 1000 Hz vehicle loop.

Racing United exposes turn slip, normalized spin, tread-twist angle, parking/rolling turn
moments and the active turn-slip reduction factors in wheel telemetry and the Tires live
panel. The old low-speed translational lateral damper is retained only as a standstill/creep
stability bridge until a later contact-mass/brush translational state replaces it; it no
longer has to impersonate torsional parking behavior. Native regressions verify MF turn-slip
force/trail reduction, 1000-vs-120 Hz parking-torsion agreement, transient coefficient use,
property-file mapping, and all previous vehicle/suspension stability contracts.

### TIRE02 — MF6.2 `.tir` parameter/data layer (2026-08-09)

Heritage can now load human-readable MF-Tyre/MF-Swift-style tire property files
through a clean native `TirePropertyFile` layer. The importer accepts `FITTYP=62`
and the MF6.2 steady-state subset of `FITTYP=70`, performs declared unit conversion,
maps the MF6.2 coefficients already evaluated by TIRE01, imports validity ranges and
scaling factors, recognizes `MC_CONTOUR_A/B` motorcycle crown data, and preserves
transient coefficients required by later tire milestones. Imported files do not inherit
Heritage's synthetic seed coefficients for omitted entries, and the active core force
terms must be explicit. Obfuscated/proprietary TIR data is explicitly rejected rather
than guessed. `TYRESIDE` measurement metadata is preserved; mounted-side asymmetric
mirroring remains a later explicit mechanism.

Imported tires carry source path, provenance, confidence, mapped-assignment count and
unsupported-assignment count. Unknown or not-yet-active sections are reported as
diagnostics instead of being silently treated as implemented. `VehicleDefinitionV2`
can carry a safe module-relative tire parameter path, provenance and confidence, and
Lua exposes `Vehicle.LoadWheelTirePropertyFile` for per-wheel runtime tuning.

Racing United now includes synthetic front/rear MF6.2 `.tir` files under `Data/Tires/`
that reproduce the existing prototype tire baseline closely enough to exercise the real
import path in-game. They are deliberately marked confidence 0.10 and are **not**
measured Pirelli/Peugeot data. The old numeric preset values remain as a fallback/debug
path. Native regression coverage includes unit conversion, coefficient/range/scaling
mapping, motorcycle contour auto-selection, unsupported-parameter diagnostics and
provenance transfer.

TIRE02 does not yet activate turn-slip, rigid-ring/SWIFT structural dynamics, road
enveloping, Temperature & Velocity coefficients, wear or wet-film behavior. Those stay
as separate mechanisms so imported data cannot imply physics Heritage has not actually
implemented. See `TIRE_MODEL.md` and ADR-036.

### TIRE01 — public MF6.2-compatible road + motorcycle tire branch (2026-08-09)

The default native road tire provider now uses a clean-room implementation of the
public MF-Tyre 6.x force/moment equations with MF6.2-compatible coefficient
vocabulary. Pure/combined longitudinal and lateral force, load/pressure terms,
camber stiffness/thrust, overturning moment, rolling-resistance moment, pneumatic
trail, residual aligning moment and total Mz are available at the existing 1000 Hz
vehicle boundary. Fx/Fy are active contact forces; Mx/My/Mz are currently telemetry
outputs. The legacy calibrated rolling-resistance force remains active because a
direct My-to-wheel-torque switch tripped existing stability regressions and was not
allowed to become an implicit vehicle retune. Existing Heritage tire controls seed a compatible nominal
coefficient set until identified tire data is available; the old generalized curve
remains an explicit fallback provider.

`MotorcycleTireProfile` is now compiled and implements the documented MF-Swift 6.2
`MC_CONTOUR_A/B` elliptical crown convention, producing lean-dependent contact
offset/support geometry. `TireSlipDynamics` is also promoted from scaffold and owns
first-order relaxation state previously embedded in `VehicleSystem.cpp`. Vehicle
definitions can select `mf62_road` or `mf62_motorcycle`; old `advanced_road` and
`motorcycle_profile` names remain compatibility aliases. Full motorcycle chassis
lean/fork/swingarm dynamics are still a separate vehicle-provider milestone.

This milestone deliberately does **not** claim the unpublished Siemens Simcenter
Tire 2512 wet-road implementation or complete proprietary MF-Swift rigid-ring/
enveloping internals. Current Simcenter 2512 capability is treated as a target class
of behavior; Heritage only implements public equations or independently specified
physics. Native regressions cover road force/moment behavior, 40-degree motorcycle
camber/contour symmetry and rate-stable tire relaxation. See `TIRE_MODEL.md` and
ADR-035.

### Architecture consolidation checkpoint (2026-08-09)

This maintenance pass does not renumber gameplay/physics milestones. It removes
the obsolete Racing United-specific built-in boot scene from reusable engine
code, ratifies the existing BVH-backed static-triangle rigid-body path with direct
sphere/box settling regressions, fixes stale/duplicate architecture records, and
restores documentation to the implementation's current state.

The Lua runtime split that was deliberately deferred at this checkpoint is now
completed by ARCH05 together with the validator/manifest changes required to keep
the safety contract authoritative.


### MASS01 — explicit vehicle mass distribution and rotational inertia (2026-08-09)

Vehicle handling mass properties are now explicit simulation data instead of an
accidental consequence of collision-proxy geometry. VehicleDefinitionV2 can carry
total mass, body-local COM, diagonal pitch/yaw/roll inertia, static front/rear and
left/right load evidence, provenance and confidence. `RigidBodySystem` can own an
explicit inertia override and `CollisionSystem` will not overwrite that tensor while
rebuilding collider mass properties.

The Racing United prototype uses a deliberately low-confidence road-car estimate
(`estimated_mass_properties_road_car_v1`, confidence 0.20) calibrated close to the
previous compact-hatch collider-derived response so MASS01 does not hide a dramatic
handling retune inside an architecture change. The current estimate is approximately
1212.9 / 1511.4 / 564.3 kg*m^2 for pitch/yaw/roll with the existing 1100 kg reference
mass and COM. These are engineering priors, not claimed Peugeot measurements.

A reusable installed-component accumulator is also established and regression-tested
with the parallel-axis theorem. Future wheels, tires, batteries, bumpers, wings,
cages, cargo and similar parts can therefore update total mass, COM and inertia from
their own mass/location evidence without moving chassis suspension hardpoints. Live
telemetry exposes the active tensor and whether it is explicit. See ADR-031.

### FLEX01 — chassis torsional compliance (2026-08-09)

Heritage now has a reusable first structural torsion mode in addition to the gross
6-DOF rigid chassis. `chassis_torsional_mode_v1` integrates a small front-to-rear
relative twist from the mismatch between front and rear suspension roll reactions.
The main body still owns pitch, roll and yaw; flex instead rotates virtual suspension
pickup frames continuously along the chassis so diagonal loading can perturb contact
geometry and wheel loads without turning every vehicle shell into a many-body model.

The high-rate structural state uses FP64 `VehicleScalar` stiffness/damping/inertia
arithmetic. Racing United currently enables a deliberately low-confidence 2003
closed-unibody estimate (`estimated_chassis_flex_closed_unibody_v1`, confidence 0.18)
for the Peugeot-oriented prototype; this is a generic engineering estimate, not a
claimed factory torsional-rigidity measurement. VehicleDefinitionV2, native compiler/
loader, Lua authoring, live telemetry and regression coverage all carry the mechanism.
See ADR-030.

The rendered body mesh remains rigid in FLEX01. The physical effect is suspension
pickup-frame compliance; cosmetic mesh deformation can be added later without being
the source of the physics.

### ROLL02 — combined pitch/roll/yaw and four-corner load transfer (2026-08-09)

The six-degree-of-freedom chassis path is now explicitly regression-locked as a
combined system rather than only as separate steering/braking/roll features. A
1000 Hz mixed-mechanism vehicle (MacPherson front, trailing-arm/torsion-bar rear,
independent front/rear anti-roll bars) settles, accelerates, then brakes and turns
simultaneously. The regression requires non-zero pitch, roll and yaw response,
front/rear and left/right load transfer, four-corner compression spread, damper
force and anti-roll-bar torque while keeping at least three tires supported.

There is intentionally no separate "diagonal rotation" degree of freedom: arbitrary
chassis attitude emerges from simultaneous pitch/roll/yaw, while diagonal wheel
loading is measured directly from the four independent suspension corners. The
live Vehicle telemetry panel now labels chassis pitch/yaw/roll explicitly and
shows axle, side and diagonal wheel loads, compression spread and front/rear ARB
torque. See ADR-029.

### ROLL01 — physical chassis COM and body-roll dynamics (2026-08-09)

The authored rigid-body/entity origin is no longer assumed to be the physical
center of mass. `RigidBodySystem` now carries a body-local COM offset and uses the
world COM for impulse/contact/constraint lever arms, collider inertia and
rotational integration while preserving creator-facing body, collider, wheel and
suspension coordinates. Lua exposes local/world COM readback and local COM
authoring. See ADR-028.

The Peugeot-oriented prototype now applies a deliberately low-confidence
`estimated_compact_fwd_hatch_v1` COM at `{0.0, 0.52, 0.20}` m. This fixes the
previous road-datum-as-COM failure that suppressed roll torque and inflated roll
inertia. A dedicated chassis-dynamics regression proves off-COM impulses create
torque and that the complete 1000 Hz vehicle loop produces bounded body roll and
left/right load transfer in a moderate turn. No visual/camera-only roll is used.

### SUS04 — reusable suspension anti-roll bars (2026-08-09)

Anti-roll coupling is now a separate native mechanism rather than hidden inside
any suspension kinematics provider. `SuspensionAntiRollBar` couples an explicit
left/right contact-unit pair using torsional stiffness, damping, lever-arm
geometry and link motion ratios, returning equal-and-opposite wheel-side forces.
The same mechanism can therefore serve MacPherson, trailing-arm, wishbone, live
axle and future suspension layouts.

VehicleDefinitionV2 now owns anti-roll bars as top-level components, the native
compiler resolves stable contact-unit references, and the loader instantiates
them automatically. Lua exposes set/get/count APIs and Racing United configures
low-confidence estimated front/rear bars for the Peugeot-oriented prototype.
Regression coverage validates isolated coupling, left/right symmetry, invalid
definitions and live operation in the 1000 Hz vehicle loop. See ADR-027.

### SUS03B — trailing-arm/torsion-bar rear suspension (2026-08-09)

`trailing_arm_torsion_bar_v1` is now a runnable native rear suspension mechanism.
A rigid trailing arm rotates about its authored pivot axis, the wheel centre and
separate damper eye follow that arc, the damper receives geometry-derived
instantaneous leverage, and arm rotation drives a real rotational torsion-bar
spring coordinate. The current creator-facing wheel-rate inputs are converted to
equivalent torsional stiffness until better direct torsion-bar data exists.

`estimated_trailing_arm_torsion_bar_road_v1` supplies a deterministic 0.30-confidence
five-point starting package when stronger rear evidence is unavailable. Assisted
MacPherson and trailing-arm estimates now use immutable chassis suspension-package
scales rather than installed tire radius, so wheel/tire customization cannot
silently rewrite chassis suspension geometry. Front MacPherson + rear trailing-arm
vehicle stability is covered at the native 1000 Hz vehicle rate. SUS04 now adds
front/rear anti-roll coupling as an independent reusable mechanism.

### SUS02 — reusable MacPherson hardpoint kinematics (2026-08-09)

`macpherson_strut_v1` is now a runnable native suspension provider rather than a
future scaffold. It consumes the eight SUS01 hardpoints, solves lower-arm travel,
steering-axis migration, passive tie-rod bump steer, commanded steering,
hardpoint-derived camber/toe, strut compression and instantaneous spring motion
ratio. The existing nonlinear spring/damper provider consumes that instantaneous
motion ratio in the 1000 Hz wheel loop.

The Visual Studio engine/test projects compile the promoted
`Suspension/Geometry/MacPherson/MacPhersonKinematics.cpp`. Definition compilation
requires a complete, non-degenerate hardpoint set whenever the MacPherson
provider is selected, and mixed-provider four-wheel cars remain supported.
Regression coverage verifies rest geometry, bump travel, mirror symmetry,
steering response, provider compilation/loading and rejection of incomplete
hardpoint sets. See ADR-023.

SUS03A now allows the Peugeot-oriented player definition to promote its front
wheels from the compatibility provider to `macpherson_strut_v1` using the
versioned low-confidence `estimated_macpherson_road_v1` hardpoint package when
better coordinates are unavailable. These points are explicitly estimates, not
Peugeot measurements, and asset-authored/measured hardpoints outrank them.

### SUS01 — suspension hardpoint authoring boundary (2026-08-09)

The Vehicle Suspension panel now exposes a non-physical AUTHORING page with
reference wheel-centre, bump/droop, steering-axis and authored-hardpoint gizmos.
VehicleDefinitionV2 can carry validated, stable suspension hardpoint IDs and
chassis-local positions through Lua parsing and the native compiler even while
`linear_raycast_v1` remains the active regression-locked provider.

The Peugeot-oriented prototype records separate mechanism composition for its
front strut/coil-spring assembly and rear trailing-arm/transverse-torsion-bar
assembly. Explicit hardpoints remain optional authoring evidence; SUS03A may fill
missing front points with labeled estimates until GLB-authored or measured data
replaces them. See `SUSPENSION_AUTHORING.md`, ADR-022 and ADR-024.

### ARCH05 — domain-split Lua bindings (2026-08-09)

The previous approximately 10,000-line `LuaModuleRuntime.cpp` is now about 1,700
lines and owns runtime lifecycle/state, registration order, sandboxing, hot reload,
API introspection and safety orchestration. The 376 registered handler implementations
live in 27 domain translation units below `Core/Modules/LuaBindings`, with Physics,
Vehicle and Entity subdivided again by responsibility. The large vehicle-definition
Lua table parser is also isolated from the public binding handlers.

All 377 Lua-facing member-function definitions were compared before/after; their
function bodies are byte-identical. The centralized registration table still contains
388 unique public API names, including intentional aliases that share handlers.
`GenerateLuaApiManifest.ps1` now resolves each public API to its exact implementation
file and hashes the complete binding source set. `ValidateProject.ps1` scans the split
source tree and prevents any individual binding implementation file from silently
growing into a new 1200+ line monolith. See `Docs/LUA_BINDING_ARCHITECTURE.md`.


### ARCH04 — scaffold-first native vehicle layout (2026-08-09)

The future native vehicle mechanism tree is now created ahead of implementation.
Project-visible scaffold `.cpp` files are intentionally non-compiled until their
mechanism is implemented, avoiding build-time cost while making future ownership
explicit. Broad categories live in directories and concrete mechanisms/responsibilities
live in files; vehicle categories do not receive duplicate solvers unless the mechanics
actually differ. See `Docs/VEHICLE_ARCHITECTURE.md`.

### ARCH03 — domain-split native physics regressions (2026-08-09)

The previous ~1,700-line `PhysicsRegression.cpp` is now an intentionally small
runner. Shared prototype-world utilities live in `PhysicsRegressionSupport.cpp`,
while vehicle dynamics, collision/terrain, suspension, and vehicle-definition
coverage live in separate regression translation units. The Visual Studio test
project and `ValidateProject.ps1` both treat the files as one safety suite.

This is a structural-only change: the Linux-native regression executable emits
byte-for-byte identical output before and after the split. Future physics tests
should be added to their owning domain instead of rebuilding a monolithic test
file.


### ARCH02 — vehicle high-rate substep decomposition (2026-08-09)

The previous ~1,100-line `VehicleSystem::simulateVehicleSubstep()` has been
reduced to an explicit orchestration layer. Steering/Ackermann state, driveline
state, and per-wheel contact/suspension/tire work now have named private stages:
`updateSteeringSubstep`, `updateDrivelineSubstep`, and `simulateWheelSubstep`.
The numerical evaluation order is intentionally unchanged.

Drive-share storage is now retained per vehicle as reusable scratch memory, so
the 1000 Hz path no longer constructs a fresh `std::vector` every substep. The
Linux-native regression executable produced byte-for-byte identical output before
and after this refactor, including parked stability, 1000 Hz timing, slope hold,
terrain contact, suspension geometry, tire behavior, and vehicle-definition
coverage. Further subdivision of the per-wheel stage should happen only when a
new suspension/tire feature creates a natural provider boundary; do not split it
merely to chase a line-count target.


### Parallel graphics capability: GFX1 — OBJ/MTL texture maps

A renderer-only side milestone now adds UV-aware OBJ import, multi-material MTL
draw ranges, module-sandboxed WIC texture loading, tangent-space normal maps,
base-colour/roughness/metallic/specular/AO/emissive/opacity inputs, correct
sRGB-vs-linear handling, mipmaps and the existing texture-filter setting
including anisotropy. OBJ, MTL and texture sources hot-reload independently.
This does not replace, complete or renumber Step 29Q terrain-contact work.
See `Docs/MATERIALS_AND_TEXTURES.md`.

### Parallel graphics capability: GFX5 — GLB specular + vertex colors

The production GLB path now consumes `KHR_materials_specular` factor/color
inputs and textures, and imports standard glTF `COLOR_0` RGB/RGBA vertex
colors. Vertex colors multiply Diffuse / Base Color (and vertex alpha multiplies
opacity), intentionally supporting the common baked vertex-AO workflow while
retaining normal/roughness/metallic/specular PBR response. This graphics work
does not modify or renumber the active physics roadmap. See
`Docs/GLB_SPECULAR_VERTEX_COLORS.md`.

### Parallel graphics capability: GFX6 — Environment IBL + reflections

The material renderer now owns a floating-point cubemap environment and uses it
for diffuse image-based lighting plus roughness-aware specular reflections. The
first environment is deterministic/procedural so no new module asset is
required. Direct material lighting is upgraded to a GGX/Cook-Torrance response,
while existing diffuse/base-color baked shading, texture AO and broad vertex AO
remain valid together. The generated cubemap mip chain is an initial reflection
prefilter approximation; HDR environments, true GGX prefiltering and local
reflection probes remain future extensions behind the same resource boundary.
The user confirmed GFX6 builds, runs and visibly adds environment reflections.
See `Docs/ENVIRONMENT_IBL.md`.

### Parallel graphics capability: GFX7 — Visible sky + day/night cycle

The procedural environment is now visible as the world sky and is driven by a
renderer-independent 24-hour `EnvironmentSystem`. Sun direction, color and
intensity move through daylight, warm sunrise/sunset and a dark blue starry
night while the same evolving cubemap continues to drive material IBL and
reflections. The default development preview runs at 240x (a six-minute day),
with F6 pause/resume, F7 speed cycling and Lua `Environment.*` time controls.
Cubemap regeneration is rate-limited so accelerated previews do not rebuild the
reflection source every frame. GFX7 remains parallel to the Step 29Q physics
roadmap. See `Docs/SKY_DAY_NIGHT.md`.

### Parallel world capability: VEG01 — vegetation / biome foundation

Heritage now has one native vegetation registry for trees, shrubs, grass, reeds,
flowers, crops and future plant families. Species own LOD policy, optional
octahedral-cluster / whole-plant-impostor capabilities and hierarchical wind
response. Large-world placements use signed 64 m chunks plus 16-bit local
coordinates (~0.98 mm local steps) and the registry has zero instance-loop cost
when empty. The actual octahedral baker/shader remains VEG02 and therefore no
vegetation asset is required by VEG01. See `Docs/VEGETATION_ARCHITECTURE.md`.

### Parallel diagnostics capability: PERF01 — performance monitor

F8 toggles a native rolling performance overlay with complete frame/FPS, CPU
section timings, asynchronous OpenGL GPU frame timing, entity-mesh draw calls /
triangles / instances, debug draw counts, loaded assets, physics-step/overload
state and vegetation registry counts. This makes later vegetation/AI/audio/world
work measurable instead of judging regressions from FPS alone. See
`Docs/PERFORMANCE_MONITORING.md`.

### Parallel vehicle-asset capability: VA01 — GLB metadata + modular part discovery

Blender Custom Properties exported through glTF node `extras` are now preserved
by the GLB importer and exposed through a renderer-independent
`VehicleAssetMetadata` service. Racing United can discover stable wheel/tire/
brake slots, preserve authored part IDs and technical properties, validate
duplicate/incomplete metadata, derive nominal tire diameter and perform the
first tire-to-wheel compatibility checks. Lua receives
`Vehicle.InspectAssetMetadata` and `Vehicle.CheckTireWheelCompatibility`; the
Vehicle Visual UI gains an `ASSET DATA` tab plus an OBJ/GLB asset picker. VA01
reads and validates semantic data; VA02 now owns embedded wheel-node presentation
binding, while automatic replacement-part loading and arbitrary metadata-to-physics
mutation remain deliberately separate. See
`Docs/VEHICLE_ASSET_METADATA.md` and ADR-019.

### Parallel vehicle-asset capability: VA02 — embedded GLB wheel-node binding

Complete vehicle GLBs can now drive their authored four-corner wheel hierarchy
from the existing native vehicle telemetry. Generic Entity mesh-node overrides
let Racing United bind each semantic `WH_*_Root` to the authoritative native
wheel-center/upright world pose and apply wheel spin only at `WH_*_Pivot`. Thus
brake calipers follow the upright but do not spin, while brake discs, wheels and
tires inherit the pivot and spin together. VA02 activates only when all four
Root+Pivot semantic corners are present and hides the older separate wheel-mesh
entities while active. Runtime replacement-part loading and automatic simulation
mutation remain later bridges. See `Docs/VEHICLE_ASSET_NODE_BINDING.md`.

### Parallel module-asset capability: AS01 — automatic module asset discovery

The active module now owns a renderer-independent `ModuleAssetRegistry` that
rescans its `Assets` tree at most once per second and increments a revision only
when files are added, removed, renamed or rewritten. Lua can query filtered
asset counts/paths, the latest matching asset and force a refresh. Racing United
uses the generic registry as a development convenience: while the vehicle visual
slot is still on its legacy default, the latest `Vehicle_*.glb` beneath
`Assets/Vehicles` is detected and selected automatically, while manually chosen
assets are not overridden. VA01 GLB metadata is refreshed when indexed content
changes. Discovery intentionally catalogs assets rather than instantiating every
GLB; modular wheel/tire/body-part binding remains a later vehicle-asset layer.
See `Docs/MODULE_ASSET_DISCOVERY.md`.


### Parallel scene-asset capability: SC01 — single-GLB world + collision authoring

Racing United now prefers `Scene_*.glb` beneath `Assets/Scenes` as the creator
world container. The same GLB may carry visible geometry, materials/textures,
SPAWN_PLAYER, and explicitly marked static collision nodes. Collision authoring
uses Blender Custom Properties (`heritage.role=collision_mesh`,
`heritage.collision_type=static_triangle_mesh`) with `_Collision` / `Collision_`
name fallback. Collision nodes are hidden automatically by the visual renderer.
The physics query importer extracts only marked collision triangles, preserves
node transforms, reads optional `heritage.surface` / `heritage.wetness`, and
uses a Blender Empty/node as SPAWN_PLAYER metadata. Racing United discovers the
latest `Scene_*.glb` lazily after module startup and auto-loads it when the asset
index becomes available. Module-facing filesystem paths are now converted from
UTF-8 explicitly in mesh/entity/Lua asset paths so Central-European filenames
remain usable beyond discovery. Legacy OBJ import remains engine-compatible,
but the Racing United Player World and validator no longer depend on the old
`PlayerScene.obj` / `PlayerScene_Collision.obj` slots. See
`Docs/SCENE_GLB_AUTHORING.md`.

Step 29F.1 was interactively confirmed in the prototype scene. Step 29G established the advanced road-tire provider. Step 29H moved tire descriptions to independent per-wheel data. Step 29I added the creator-owned player-car OBJ slot; Step 29I.1 hardened window-size recovery and the user confirmed an authored car renders and drives. Step 29J added optional independently animated wheel meshes. The user confirmed the wheels render and rotate, which exposed two presentation problems: temporary 2.10 m track / 2.60 m wheelbase mounts were far too wide/long for the imported Peugeot, and all four wheel meshes used the same side orientation. Step 29J.1 uses published 2003 Peugeot 206 RC wheelbase/track/tire dimensions as a visual/reference geometry baseline and places every rendered wheel at the exact native `WheelState.worldCenter`. Step 29J.2 established Blender-native content authoring coordinates (X left/right, Y forward/backward, Z height), authored 1:1 creator geometry and exact numeric entry. Steps 29J.3/29J.3a exposed limitations of the temporary OBJ box-proxy/spawn bridge on a real hilly scene. Step 29J.4 uses the user-supplied scene files as a regression fixture, corrects Blender default OBJ axis conversion, accepts SPAWN_PLAYER from either visual or collision OBJ, snaps spawn height to the actual terrain, and makes exact scene triangles participate in suspension/tire raycasts. Step 29J.4B adds a headless native vehicle regression suite, corrects handbrake wheel-torque overshoot at 1000 Hz, and adds a physically capacity-checked parked rest state that wakes on throttle or brake release. Step 29J.4C removes the repeated low-speed rear oscillation observed during turn-then-brake testing. Step 29J.5 adds the first native high-rate Vehicle Dynamics Laboratory. Step 29J.6 adds the first versioned topology-first Vehicle Workshop contract and module-isolated authoring/export workflow. Step 29J.6A makes its choices responsive in two-column rows so narrower debug panels retain every control. The user confirmed that layout works. Step 29J.6B promotes the topology chooser to three columns at the demonstrated panel width while retaining the two-column fallback. The user confirmed the corrected layout. Step 29K adds native definition compilation, stable-reference resolution, component-driven provider selection and the first runtime loader adapter. Step 29L adds resolved suspension components and the first native suspension force-provider contract. The current candidate includes:

Step 29M keeps the work suspension-only: healthy suspension components now
support preload, progression, digressive damping and travel stops, while native
telemetry exposes force components and damper energy rate for later thermal,
wear and damage models.

Step 29N makes that model active on the ordinary prototype path and adds exact
native per-wheel set/readback APIs plus a focused live tuning panel. Existing
modules using the shorter historical `Vehicle.AddWheel` signature remain
compatible.

Step 29O adds optional constrained wheel/upright inertia plus radial tire
stiffness and damping per contact unit. It produces native wheel hop, tire
deflection and authoritative radial contact load without a free rigid body per
wheel. Effective mass zero preserves the massless compatibility/scalability
path. Live tuning, definitions, telemetry, Dynamics Lab, CSV and regressions all
use the same native state.

Step 29P adds the native suspension-geometry boundary and one authoritative
upright pose shared by tire direction, telemetry and articulated wheel visuals.
The current provider evaluates per-contact 3D steering axes plus signed
quadratic camber and toe curves from suspension travel. Existing definitions
retain a vertical axis and zero curves until measured alignment data is
authored.

Step 29P.1 starts Racing United directly in the creator-authored Player World,
enables the four authored wheel meshes by default and seeds the Peugeot-oriented
prototype with explicitly mirrored, provisional 206-family workshop alignment.
Only static alignment and steering-axis orientation are populated: dynamic
camber and bump-steer curves remain zero until measured traces or hardpoints
exist. Restoring the definition now preserves those per-corner values.

SUS01 established per-corner suspension hardpoint authoring and the stable
MacPherson/trailing-arm ID contracts. SUS02 promoted the MacPherson scaffold to
the runnable `macpherson_strut_v1` provider, deriving wheel alignment, bump
steer, steering axis, strut travel and instantaneous motion ratio from linkage
geometry. SUS03A adds epistemically explicit assisted authoring: versioned
low-confidence MacPherson estimates may activate real front kinematics when
factory coordinates are unavailable, while measured/GLB-authored points always
outrank estimates. Vehicle GLB metadata can now expose named suspension
hardpoint nodes and upgrade the active geometry without changing the provider.
The Peugeot rear remains on the compatibility path pending its dedicated
trailing-arm + transverse-torsion-bar milestone.

Step 29Q makes the temporary terrain-contact limit observable before replacing
it. Every wheel now classifies support, bottom-out, road-without-load,
surface-behind-origin tunnelling, static scene boundaries, missing candidates,
exact-test misses and support beyond droop. Static raycasts expose bounded query
evidence, a transition-only reverse probe identifies crossed surfaces, Lua and
the Suspension Live panel present the native state, and deterministic tests
cover seams, reversed winding, steep slopes, tiny gaps, rapid descent,
bottom-out, support beyond droop, scene bounds and airborne landing. This
milestone originally diagnosed the query-only triangle path. The current
consolidated implementation now also resolves dynamic sphere/box primitives
against the same BVH-backed static triangle world; broader dynamic/concave mesh
contact remains later work.

- Deterministic 120 Hz default general physics world with bounded catch-up; runtime tick rate remains independently configurable without changing the 1000 Hz vehicle tire/suspension clock.
- Generation-checked entities, rigid bodies, colliders, constraints, and vehicles.
- Collision detection, angular response, sleeping, islands, queries, CCD, and springs.
- A driveable arbitrary-wheel vehicle with a 1000 Hz tire/suspension loop.
- Deterministic headless physics regressions for flat rest, sleep/wake, high-rate timing, braked slope hold, and unbraked slope roll.
- An opt-in bounded native vehicle recorder sampled from the high-rate solver, with summary statistics, peak-preserving plots and complete CSV export.
- Repeatable parked-settle, 250 mm drop, straight-braking and turn-then-brake experiments plus manual driving capture in the Vehicle `LAB` tab.
- A `VehicleDefinitionV2` component graph for bodies, power units, transmissions, contact units and explicit drive connections, with category templates that do not select duplicated solvers.
- A native `VehicleDefinitionCompiler` and `VehicleDefinitionLoader`; Workshop preview now uses resolved component indices and the `raycast_wheel_v1` provider instead of reconstructing drive layout in Lua.
- A native `SuspensionModel` provider boundary; contact units resolve stable suspension IDs and the current `linear_raycast_v1` implementation owns spring, damper, motion-ratio and force-limit evaluation.
- Non-linear healthy suspension forces with separate low/high-speed bump and rebound damping, progressive springs/stops, droop stops, live force breakdown and damper-dissipation telemetry.
- Atomic per-wheel nonlinear suspension tuning/readback and a Vehicle `SUSP.` tab with spring, damper, travel-stop and live-force pages.
- Optional scalar unsprung mass and radial tire compliance per contact, with bounded wheel-hop integration, a massless fallback and live `UNSPRUNG` tuning.
- Native wheel-hop/tire-deflection telemetry and Dynamics Lab plots, CSV channels and summary extrema.
- Native per-contact steering-axis and camber/toe curves with an authoritative upright basis evaluated in the high-rate solver.
- Live `GEOMETRY` tuning, upright telemetry, articulated-wheel pose consumption, and Dynamics Lab camber/toe plots and CSV.
- Reusable `macpherson_strut_v1` hardpoint kinematics with geometric camber/bump-steer/steering-axis motion, strut travel and instantaneous spring motion ratio.
- Versioned assisted MacPherson hardpoint estimation with provenance/confidence; estimates are explicitly low-confidence and can be replaced point-by-point by GLB-authored or measured data.
- Reusable `trailing_arm_torsion_bar_v1` rear kinematics with rigid arm arc, torsion-bar angular spring travel and geometry-derived separate-damper leverage.
- Versioned assisted trailing-arm/torsion-bar estimation with provenance/confidence; both front and rear estimators are locked to chassis reference-package scales rather than current wheel/tire fitment.
- GLB suspension hardpoint discovery via `SUS_FL/SUS_FR/SUS_RL/SUS_RR` node names or semantic extras, exposed through `Vehicle.InspectAssetMetadata`.
- A persistent Vehicle `WORKSHOP` tab with Windows module-asset selection, structural validation, honest current-solver capability reporting, supported live preview and module-private definition export.
- An adaptive three-column Workshop topology grid with a two-column fallback; longer actions retain the proven two-column layout without horizontal scrolling.
- Capacity-checked parked-vehicle sleep plus non-overshooting service/parking-brake wheel constraints.
- Ackermann steering, drivetrain, reverse/neutral/gears, differential modes.
- Dedicated native `Vehicles/TireModel.*` provider boundary.
- Independent native tire description per wheel/contact unit, with named Lua presets and exact per-wheel API readback.
- Public MF6.2-compatible road-tire force/moment core with pure/combined slip, load/pressure dependence, camber thrust, pneumatic trail, Mx/My/Mz telemetry, ABS/TCS integration, plus an explicit legacy generalized-curve fallback.
- Generic collider surface identity plus wetness propagated through ray/sphere queries.
- Independent per-wheel surface detection: different tires can simultaneously contact asphalt, grass, gravel, snow, ice, kerbs, or painted lines.
- Modular Racing United Lua scripts with native simulation in C++.
- Build identity, generated Lua binding manifests, runtime API dump, and lifecycle smoke tests.
- Tabbed prototype lab: Vehicle, Physics, Entity, Module, Scene, and Safety.
- Vehicle sub-tabs for Drive, Visual, Surfaces, Tires, Drivetrain, ABS/TCS, and Telemetry.
- Creator-owned `Assets/Vehicles/Player/PlayerCar.obj` slot with live mesh hot reload. Step 29J.2 treats creator geometry as authored 1:1/identity by default rather than exposing routine visual offset/scale correction.
- Optional `PlayerWheel.obj` articulated visual slot with per-wheel asset paths; wheel meshes use exact native wheel-center telemetry, per-side facing/spin conventions, Ackermann steering and simulated rotation without a Lua-side wheel-position approximation.
- Permanent Racing United content-authoring convention: Blender X left/right, Y forward/backward, Z height, 1 unit = 1 metre; importers convert to native engine coordinates.
- Creator geometry defaults to authored identity/1:1 instead of runtime body/wheel convenience scaling.
- `Scene_*.glb` creator-world container discovered beneath `Assets/Scenes`; visible geometry, hidden collision authoring and SPAWN_PLAYER may live in the same GLB.
- Collision nodes use `heritage.role=collision_mesh` / `heritage.collision_type=static_triangle_mesh` (or `_Collision` naming fallback) and populate an immutable BVH-backed static triangle world used by queries plus dynamic sphere/box rigid-body contacts. General dynamic/concave triangle-mesh collision remains later work.
- `UI.SliderFloat` supports double-click keyboard entry for exact values in addition to mouse dragging.
- Current prototype mount reference: 2442 mm wheelbase, 1437 mm front track, 1428 mm rear track and 205/40 ZR17 geometry. These dimensions are a reference/alignment step, not the final measured 206 RC physics package.
- Physics sub-tabs for World, Suspension, Queries/CCD, and Body diagnostics.
- Scene visibility presets that hide unrelated debug geometry without disabling simulation.
- Lua UI files split by subsystem so future AI/human edits stay localized.

## Authoritative executable source

Visual Studio compiles:

`Engine/HeritageEngine/HeritageEngine/main.cpp`

The older file at:

`Engine/HeritageEngine/main.cpp`

is obsolete and should remain absent. Never modify a similarly named file without checking `HeritageEngine.vcxproj` first.

## Current primary game scope

Racing United: The Virtual Heritage of Racing runs on Heritage Engine and is intended to support:

- Single-player, multiplayer, and MMO-style operation.
- Cars, motorcycles, ATVs, go-karts, trucks, trailers, and unusual ground vehicles.
- Large races with 150 or more vehicles, including Nürburgring 24h-style fields.
- Free-roam maps, traffic, lane graphs, and jurisdiction-aware traffic rules.
- High-fidelity simulation with scalable physics, AI, networking, and world streaming.

Heritage Engine must also load unrelated modules with their own gameplay and physics character.

## Planned early reference vehicles

- 2003 Peugeot 206 RC.
- 2003 Ducati Monster S4R.

These are future content definitions, not hard-coded assumptions in the vehicle solver.

## ALIGN01 — Peugeot 206 RC factory alignment envelope + exact setup input

A user-supplied Peugeot 206 RC alignment table now lives in a dedicated vehicle
evidence file rather than being embedded as an undocumented provisional constant.
The MIN/MAX ranges are preserved separately from the current setup. Because the
source table did not provide a populated standard/nominal column, midpoint values
are used only as labelled workshop defaults. Existing untouched FITMENT01 defaults
are migrated to the new midpoint reference while genuinely customized saves are
preserved.

The fitment Workshop now shows per-corner factory camber/toe/caster ranges, axle
total-toe range and front steering-axis-inclination evidence. Factory ranges are
reference information only: the engine still permits broad positive/negative
camber, toe and caster values. Alignment controls now pair a 0.01-degree slider
with a visible exact numeric input that accepts finer values such as 0.82 degrees.
See `WHEEL_FITMENT_AND_ALIGNMENT.md` and ADR-033.


## FITMENT02 — explicit hub datums and steering-ground geometry

FITMENT02 promotes the hub-reference and scrub-radius scaffolds into compiled
reusable mechanisms. Wheel fitment now resolves distinct reference wheel center,
reference hub mounting face, spacer-shifted installed mounting plane, installed
wheel centerline and nominal inboard/outboard tire envelope. Positive ET and
spacer semantics are explicit; none of these setup operations move suspension
hardpoints or the reference Ackermann track.

Suspension geometry now exposes a point as well as a direction on the steering
axis. MacPherson supplies its current lower ball joint; the solver intersects the
live steering axis with the current road contact plane to report signed scrub
radius, scrub magnitude and mechanical trail. The FITMENT regression proves that
a 23 mm outward wheel-center shift on each front corner changes scrub by the same
23 mm per side while steering reference track remains fixed.

Vehicle GLB metadata now recognizes explicit `wheel_fitment_datum` nodes with
`hub_face_center`, `wheel_centerline` and `wheel_spin_axis` roles, plus stable
`FIT_FL/FIT_FR/FIT_RL/FIT_RR` name aliases. Arbitrary `WH_*` object origins remain
non-authoritative unless their semantic role is declared. The current Peugeot GLB
contains no such explicit datums, so its trusted technical metadata and established
reference transforms remain authoritative for now. See ADR-034 and
`WHEEL_FITMENT_AND_ALIGNMENT.md`.

## FITMENT01 — reference wheel/tire fitment and per-corner alignment

The neutral authored vehicle assembly is now explicitly separated from the live
vehicle setup. Per-corner fitment carries reference/installed ET, spacer thickness,
rim/tire technical dimensions and resolved nominal tire radius. Per-corner alignment
carries camber, toe and an optional caster override. Racing United exposes optional
front and rear L/R linking for ordinary symmetric setup work while retaining fully
independent corners for oval racing and other asymmetric setups.

Installed ET/spacers displace the wheel/tire centerline downstream of the upright;
they do **not** relocate suspension hardpoints, suspension-force attachment points or
the reference Ackermann steering track. A regression moves both front tire centerlines
23 mm outward and requires +46 mm installed track while the 1.437 m steering reference
track remains unchanged. Leaving setup at its factory/reference values preserves the
BASE02 native regression output byte-for-byte aside from the new FITMENT01 test lines.

The current Peugeot reference uses explicit 17x7 ET28 / 205/40 R17 technical metadata.
Its detailed wheel/tire mesh shape remains provisional. FITMENT01 deliberately does not
assume every `WH_*` node origin means wheel centerline. FITMENT02 now defines explicit
hub-face, wheel-centerline and spin-axis datum roles; arbitrary GLB transforms remain
non-authoritative unless one of those roles is declared. See
`WHEEL_FITMENT_AND_ALIGNMENT.md`, ADR-032 and ADR-034.

## BASE02 — canonical baseline architecture hygiene

After the Windows-verified MASS01C full-project snapshot, the project baseline was
reconciled before fitment work. `VehicleDefinitionV2.lua` is now a small schema/template
file with builder, validation and deterministic serialization in separate responsibility
files; the public Workshop functions and generated definition format are unchanged.
Misplaced aerodynamics scaffolds were moved to the `Vehicles/Aerodynamics/` paths already
declared by the Visual Studio project. The validator now checks every project-visible
vehicle scaffold path instead of a few examples, and the rolling build CMD delegates
detailed repository inventory to the validator rather than duplicating an ever-growing
file list. Vehicle asset documentation now reflects that VA02 already binds embedded
wheel presentation nodes. No intentional vehicle-physics behavior changes are part of BASE02.

## Immediate roadmap

The current user directive is to finish tire and driven-surface behavior as one continuous program
before resuming unrelated vehicle-domain expansion. `TIRE_SURFACE_ROADMAP.md` is authoritative for
the detailed mechanism list and fidelity/performance contract. TIRE14 is user-validated; TIRE15 is
the current candidate.

1. **CLEAN12-CLEAN13:** CLEAN11 is user-validated. Finish the final two post-CLEAN08 cleanup checkpoints in `CODE_MODULARIZATION_ROADMAP.md`, then stop architecture-only cleanup unless a concrete blocker is found.
2. **TIRE15:** user-validate persistent deformable-terrain terramechanics in the Windows/game build. It now implements mud/sand/soft-soil/deep-snow pressure-sinkage, shear mobilization, bulldozing/plowing and bounded shared `SurfaceField` rut/compaction/moisture/shear memory.
3. **TIRE15B:** move synthetic terrain presets into authored `SurfaceMaterial`, connect weather/local surface temperature/water state, and add a bounded visual/particle/audio consumer for the same authoritative `SurfaceField` ruts and displaced material.
4. **TIRE15C:** add authoritative dynamic rubber deposition/rubbering-in and loose tire-marble accumulation, tire pickup/cleaning coupling, rain/weather removal or redistribution, and bounded visual marbles driven from the same shared world state rather than thousands of persistent rigid bodies.
5. **TIRE16:** complete specialty tire-family authoring (summer/performance, slick, wet race, winter, studded, rally gravel, motorcycle, kart, truck/commercial, low-pressure ATV/off-road) by reusing common mechanisms only where physically valid.
6. **TIRE17:** complete tire/surface laboratory tooling, calibration/provenance workflow, split-surface/water/terrain regressions and large-grid fidelity/rate tiers. Distributed per-cell force/shear integration remains optional rather than the default 600-tire cost.
7. After the tire/surface completion gate, return to wheel-fitment clearance/mass/inertia/bearing loads, suspension topology expansion, full motorcycle vehicle dynamics, FFB, aero and other deferred domains.

## Recovery procedure for a new conversation or contributor

1. Obtain the newest complete project ZIP, not only an old base archive.
2. Read this file and `AI_WORKFLOW.md`.
3. Run `Tools/GenerateLuaApiManifest.ps1` and inspect `Build/Reports/LuaAPI.md`.
4. Run `Tools/ValidateProject.ps1`.
5. Inspect the exact source files involved before proposing code.
6. Build through `Tools/00_BuildAndRunCurrent.cmd` and record the resulting build identity.

## CLEAN02 vehicle-system responsibility split — implementation candidate

Before TIRE16, the former ~5,974-line `Vehicles/VehicleSystem.cpp` has been physically split without
changing the public `VehicleSystem.hpp` contract or vehicle equations. Lifetime/handles/basic state
remain in `VehicleSystem.cpp`; configuration is in `VehicleConfiguration.cpp`; Dynamics Lab capture
and readback are in `VehicleTelemetry.cpp`; vehicle-level scheduling/steering/driveline/ARB/chassis
flex orchestration is in `VehicleSimulation.cpp`; and the authoritative high-rate per-wheel solver is
now isolated in `VehicleWheelSimulation.cpp`. `VehicleSystemInternal.hpp` remains the private
vehicle helper boundary, while CLEAN04A has now moved genuinely reusable quaternion algebra into
`Core/Math/Quaternion.hpp`.

Portable C++20 warning-as-error compilation and the complete headless native physics regression suite
passed with output byte-identical to the CLEAN01A unsplit baseline. The subsequent Windows build,
launch and drive validation passed, so CLEAN02 is user validated.
## CLEAN12 — Lua binding boundary + validator modularization

CLEAN12 is the current implementation candidate. The central Lua runtime header no longer
declares every native Lua handler or transitively includes the major engine service
implementations. Four private handler catalogues (Core, Physics, Vehicle and Entity) own
the 400 Lua C-handler declarations while `LuaModuleRuntime` remains the lifecycle/state
owner and keeps ordered domain registration. The generated manifest is taught to resolve
the domain-owned handler classes without changing the 410 public Lua API registrations.

The repository validator is also physically split under `Tools/Validation/`;
`Tools/ValidateProject.ps1` remains the single user/AI entry point and report owner. This
keeps validation extensive without letting the safety net itself become another 2,000-line
dumping ground. See `LUA_BINDING_ARCHITECTURE.md` and ADR-059.

