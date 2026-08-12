# Current Project State

## Current checkpoint

**User-confirmed cleanup baseline:** CLEAN13 — the planned CLEAN01-CLEAN13 architecture program builds, launches and drives. The architecture-only cleanup stop rule is now active.

**User-confirmed tire checkpoint:** TIRE15B2 — authored/live surface conditions plus bounded driven-surface rut/sinkage/spray/dust/debris presentation build, launch and drive.

**User-confirmed rubber tuning checkpoint:** TIRE15C3 — stress/wear-driven shedding, traffic-driven marble maturity, 1000x lab controls, world-stable orientation, chunkier procedural debris and removal of the temporary per-cell rubber-mark presentation all build, launch and drive.

**User-confirmed rubber checkpoint:** TIRE15C5A1 — aerodynamic marble migration plus the calibrated two-triangle moving/resting flake presentation build, launch and visually satisfy the current marble/shedding target. The aggregate 0.5 m storage grid is no longer visibly exposed as a chessboard.

**Current tire candidate:** TIRE17C9/VIS12 — the exact creator-collider GPU stage is now a bounded elastic carcass constraint rather than an iterative hard-plane projector. C8 live testing proved that the exact path was active, but also exposed an algorithmic failure: one tire vertex could be projected sequentially by many nearby road/kerb/chamfer planes, producing polygon spikes, while the presence of any exact triangle disabled the smoother TIRE17C1 support-field curb response. VIS12 reconstructs/orients each triangle plane against the rendered tire centre, uses metric closest-point edge tests, chooses at most one dominant plus one independent corner constraint, moves rubber primarily inward toward the rim, caps travel by available sidewall span, anchors the bead, and keeps the smooth 3x3 support deformation active before the exact final non-penetration pass. C8 camera-relative triangle packing and exact closest-surface candidate ranking remain active.



**Historical milestone narrative:** archived verbatim at `Docs/History/PROJECT_STATE_MILESTONES_THROUGH_CLEAN12.md`. Keep this file focused on the current working state rather than appending every completed milestone forever.

## CLEAN13 validated — cleanup stop rule active

The planned architecture-only cleanup program is complete. Further restructuring must be driven by a concrete implementation blocker rather than line-count aesthetics. TIRE15B1 is the first post-cleanup feature checkpoint.

## TIRE15C1-C3 validated / TIRE15C5A candidate — dynamic track rubber, persistent debris and aerodynamic migration

TIRE15B1 and TIRE15B2 are user validated. Static collision GLB nodes may author deformable terrain mechanics and local temperature/wetness while `SurfaceWorld` owns live global wetness plus ambient/road temperature. Wet, winter, thermal and deformable tire paths consume the same resolved conditions.

TIRE15B2 adds presentation as a **one-way consumer** of authoritative state. `SurfacePresentation` stores bounded floating-origin-safe track marks and transient particles. Rut/sinkage marks are derived from actual rut depth/displaced volume; water spray, dust, mud, snow and loose debris are emitted from actual speed/slip/load/wetness/material contact state. The dedicated renderer draws the nearby bounded representation without modifying terrain collision or tire forces.

Lua exposes `Physics.GetSurfacePresentation()` with active mark/particle counts plus normalized rolling/spray/dust/debris audio mechanism intensities. These are hooks for future authored audio assets; this checkpoint does not fake tire sounds.

TIRE15C promotes the dedicated `Physics/Surfaces/Rubber/TrackRubberState` scaffold into compiled world state. Dry hard-surface tire contacts build a shared deposited-rubber racing line; abrasion/slip generates loose rubber that is swept laterally into marble-rich cells. Subsequent tires sample deposited/loose rubber before force evaluation, loose rubber feeds TIRE11 tread pickup, dry deposited rubber gives a bounded grip gain, wet rubber and marbles can reduce grip, and rain ages/washes loose rubber faster than bonded track rubber. The renderer darkens rubbered pavement and procedurally creates deterministic curled/torn rubber ribbons/clumps from the loose-rubber cells, so no authored marble mesh or persistent marble rigid bodies are required.

TIRE15C2 makes loose-rubber form explicit. Fresh shed material begins near zero maturity as flakes/shreds. **Elapsed time alone does not mature it**; repeated tire traffic, agitation/slip, tack/temperature and local concentration progressively increase a persistent `marbleMaturity` state. Mature rubber is more mobile when a tire runs through an off-line band and is rendered shorter/thicker/more clumped. Wear is a continuous susceptibility factor rather than a hard 70%-remaining threshold, while tire authoring gains an explicit neutral `rubberSheddingPropensity` parameter so softer/abrasion-prone constructions can shed differently without tying behavior to brand or final friction.

The Tire UI gains a development-only LAB tab with 0-1000x tire-wear, rubber-generation and marble-maturity controls plus reset tire state and reset track rubber buttons. These values are runtime test controls only and are never serialized into the vehicle/tire definition. Track rubber remains one shared world layer for all vehicles; the default active cell budget is raised to 524,288 cells (roughly enough for a several-metre-wide evolved band around a full Nordschleife-length circuit), and the renderer uses near 3D procedural pieces plus a medium/far aggregate dark band rather than persistent per-marble objects.

## Authoritative executable and build workflow

Visual Studio compiles:

`Engine/HeritageEngine/HeritageEngine/main.cpp`

The obsolete outer `Engine/HeritageEngine/main.cpp` must remain absent. `main.cpp` is a tiny entry point; `HeritageEngine` and the compiled runtime phase owners coordinate startup, frame timing, simulation, rendering and hotkeys.

Normal development uses:

`Tools/00_BuildAndRunCurrent.cmd`

This performs the content-hash freshness guard, repository validation, build identity generation, native physics regressions and an **incremental** MSBuild of the engine before launch. Use:

`Tools/00_BuildAndRunCurrent.cmd FULL`

for an explicit full rebuild checkpoint.

## Architecture rules that are currently authoritative

- Heavy reusable/high-rate simulation stays in native C++; Lua owns authoring/configuration/gameplay/UI orchestration.
- Files are split by durable subsystem ownership, not arbitrary line-count targets.
- Create known subsystem scaffolding early, but do not create speculative micro-files with no enduring owner.
- Shared vehicle mechanisms must not assume exactly four wheels. Common wheel/tire/suspension mechanisms are reused across cars, motorcycles, trucks, ATVs, karts and future trikes where physically valid; two-wheel lean/balance and other topology-specific whole-vehicle behavior live in dedicated topology layers.
- Blender-authored transforms and explicit GLB metadata are the authoritative asset/reference inputs when present. Physics may derive dynamic deviations; it must not silently rewrite authored reference geometry.
- Persistent driven-surface state is world-owned through `Physics/Surfaces/SurfaceWorld`, addressed in stable FP64 global coordinates and stored in bounded chunked fields.
- Tire marbles/rubber pickup are a specialized rubber subsystem even when they share world spatial infrastructure with other surface state.
- First-party wheel telemetry uses the named `Vehicle.GetWheelTelemetry()` table. The old large positional wheel-state API is compatibility-only and should not be extended.
- The rolling validator/build helpers are safety infrastructure for both human and AI development; architectural moves must update the safety net in the same checkpoint.

## Current tire / vehicle-parts direction

Tires are reusable vehicle parts. Engineering geometry/construction/pressure/load metadata establishes the physical baseline. Creator-facing Tire/Vehicle Parts Lab controls provide bounded biases for:

- Dry
- Wet
- Snow / Ice
- Mud
- Sand
- Gravel
- Wear / Endurance

These controls nudge coherent physical parameter generation/calibration; they are **not** direct final-force multipliers. Brand names never determine physics by themselves. Advanced authoring remains able to supply explicit engineering/fitted data.

Current native authoring ownership is under `Vehicles/Tires/Authoring/`; `TirePartDefinition` reserves the bias contract but the active tire solver does not consume those creator biases yet.

See `TIRE_PART_AUTHORING_ROADMAP.md`, `TIRE_SURFACE_ROADMAP.md`, ADR-055 and ADR-058.

## Current vehicle/world foundation

- VehicleSystem is split by lifecycle/configuration/telemetry/vehicle simulation/wheel simulation responsibility.
- The high-rate wheel substep is partitioned into named ordered phases while preserving the validated equation/order path.
- Vehicle configuration ownership is divided among suspension, alignment, anti-roll bar, chassis compliance, steering, brakes, driver aids, drivetrain and tire configuration owners.
- Topology scaffolding exists for Common, TwoWheel, ThreeWheel and FourPlusWheel behavior.
- Collision is divided among root orchestration, broadphase, queries, narrowphase, solver, CCD and islands/sleeping.
- Entity mesh rendering is divided among root rendering, asset cache, animation, shadows, renderer math and shaders.
- Input and glTF importers are responsibility-split behind stable public APIs.
- `main.cpp` is a tiny entry point; runtime phases and long-lived engine state have explicit owners.
- Lua registration is domain-owned and the central runtime header no longer carries the complete handler catalogue.
- SurfaceWorld is shared world state, floating-origin safe and chunked/streamable.

## Primary game scope

Racing United: The Virtual Heritage of Racing runs on Heritage Engine and is intended to support single-player, multiplayer and MMO-style operation; cars, motorcycles, ATVs, go-karts, trucks, trailers and unusual ground vehicles; large race grids; free-roam/traffic worlds; and scalable high-fidelity physics, networking, AI and world streaming.

Heritage Engine must also be able to load unrelated game modules with their own gameplay and simulation character.

### Early reference vehicles

- 2003 Peugeot 206 RC
- 2003 Ducati Monster S4R

They are content/reference definitions, not hard-coded assumptions in the generic vehicle solver.

The current Peugeot 206 RC wheel/tire mesh is not treated as manufacturing-accurate geometry; explicit technical metadata and authored reference transforms remain authoritative where applicable.

## Immediate roadmap after CLEAN13

1. **User-validate TIRE16 tire marks.** Check hard braking, wheelspin and sliding for continuous soft-edged marks whose darkness changes smoothly with physical slip and whose width shows subtle pressure/camber asymmetry without visible stamp boundaries.
2. **TIRE17:** specialty tire-family authoring by reusing common mechanisms where physically valid, including road/performance, slick/wet race, winter/studded, rally/gravel, motorcycle, kart, truck/commercial and low-pressure off-road families.
3. **TIRE18:** Tire/Surface Lab tooling, calibration/provenance workflow, split-surface/water/terrain regressions and large-grid fidelity/rate tiers, including endurance-scale tire-mark persistence/streaming profiling.
4. Then return to fitment clearance/mass/inertia/bearing loads, suspension topology expansion, full motorcycle dynamics, FFB, aero and other deferred domains.

`TIRE_SURFACE_ROADMAP.md` is authoritative for detailed tire/surface sequencing.

## Recovery procedure for a new conversation or contributor

1. Obtain the newest **complete** project ZIP/checkpoint rather than reconstructing from old deltas when possible.
2. Read this file, `AI_WORKFLOW.md`, `CODE_MODULARIZATION_ROADMAP.md` and the relevant subsystem roadmap.
3. Run `Tools/GenerateLuaApiManifest.ps1` and inspect `Build/Reports/LuaAPI.md` when Lua API work is involved.
4. Run `Tools/ValidateProject.ps1`.
5. Inspect the exact source/project files involved before editing.
6. Build through `Tools/00_BuildAndRunCurrent.cmd`; use `FULL` only when a clean rebuild is intentionally required.
7. After a substantial validated checkpoint, prefer a fresh full project ZIP before the next large change.

## TIRE15C3 candidate — stable world-space marble presentation

User testing of TIRE15C2 showed that the procedural shreds and the temporary deposited-rubber streaks appeared to rotate as steering changed. The cause was presentation ownership: a rubber cell stores the latest contact direction for physical migration, and the renderer was reusing that mutable tire direction as the basis for every existing visual piece in the cell. TIRE15C3 separates those concerns. Procedural marble positions/orientations now use a fixed world-axis basis projected onto the support plane, so deposited debris remains anchored to the track while the vehicle steers through it.

The temporary per-cell deposited-rubber rectangles are no longer rendered. Bonded rubber remains authoritative physics state and can later drive a smooth racing-line/material treatment without exposing 0.5 m storage cells as blocky tire marks. Near procedural fragments are widened/lifted and crossing strands occur more often so mature concentrations read as thicker irregular clumps rather than hair-thin strokes.


## TIRE15C4 superseded candidate — persistent 500k logical marble population

TIRE15C3 is user validated. TIRE15C4 fixes the remaining vertical anchoring bug by freezing each rubber cell's support frame after first valid contact, so existing pieces cannot rise/sink because a later steering direction rewrites presentation geometry.

Track rubber now carries a persistent logical piece population in addition to deposited/loose concentration and marble maturity. The default global budget is 500,000 logical pieces. This is intentionally aggregate cell state rather than 500,000 rigid bodies: nearby representatives are reconstructed procedurally, dense cells can stack several visible layers, and medium/far concentrations remain aggregated. Dry idle time does not age pieces out; tire pickup, sweeping/migration, rain/washing, reset or bounded world streaming are the ways state changes.

Fresh shedding records fragment severity from slip, dissipated power, wear and high-temperature abuse. More severe events bias the procedural distribution toward larger chunks. A new `RubberShred` transient presentation path gives actual fresh tear-off a modest tire-driven ballistic toss and a support-plane settle, while permanent resting debris remains authoritative in TrackRubberState.

The Tire LAB surface telemetry now reports persistent logical rubber-piece count against the 500,000 budget.


## TIRE15C5 candidate — authoritative moving flakes and aerodynamic marble migration

TIRE15C5 replaces the C4 presentation-only shred toss with authoritative lightweight moving rubber owned by `TrackRubberState`. Fresh tear-off now remains in an AIRBORNE state, transitions to a short MOBILE_GROUND slide after support-plane impact, and becomes RESTING cell state only after settlement. Logical quantity/piece population therefore follows the moving rubber instead of being painted onto the ground before the visible flight completes. High-rate shedding is merged into a bounded packet pool; it is not per-marble rigid-body physics.

Nearby moving packets render as a world-space quad split into exactly two triangles. The p0-p2 diagonal is fixed and the other two vertices can bend independently, producing cheap curl/flutter while the packet itself has full 3D position, orientation and angular motion. The rubber/track pass is two-sided.

VehicleSimulation now evaluates an analytical marble wake once per vehicle/world step. The field uses vehicle speed, footprint, supported load/reference weight, ride height and an explicit future aero factor to sweep aggregate loose-rubber cells outward/rearward and occasionally lift fresh/light material. Existing moving packets receive matching wake impulses. Cell-to-cell migration is conservative; the prior contact-sweep path that implicitly discarded roughly 8% of swept loose rubber is removed. Pickup, rain/washing, reset and bounded streaming remain explicit ways material can actually leave a location/state.

Portable C++20 warnings-as-errors compilation and the full 93-translation-unit HeritagePhysicsTests target pass. Windows/MSVC build/launch and the visual magnitude of wake/flutter remain the user validation gate.


## SHADOW03 candidate — live quality and filtered comparison shadows

Video Settings now has a dedicated **Shadows** divider in both the in-engine settings menu and Launcher. Low/Medium/High/Ultra select 1024/2048/3072/4096 per CSM layer, with Ultra/4096 the default when the GPU supports it. Shadow Filtering offers **Nearest**, **Poisson PCF**, and **PCSS + Poisson**; fresh defaults use PCSS + Poisson. Poisson PCF uses a stable 16-tap Poisson disk with hardware-linear comparison filtering. PCSS + Poisson adds a raw-depth Poisson blocker search, derives a clamped receiver/blocker penumbra, then performs Poisson PCF through the linear comparison sampler. The same shadow depth array is viewed through separate sampler objects (linear compare and raw nearest), avoiding a duplicate shadow texture. Resolution/filter changes apply live and persist in the per-module `settings_video.ini`. F8 reports the active resolution and filter mode. SHADOW02 layered cascade fan-out/batching remains unchanged.

## TIRE15C5A candidate — marble visual/calibration hotfix

User testing of TIRE15C5 exposed two presentation/calibration failures. First, normal 1x loose-rubber mass flux was far too high for a single road car. Second, the transient renderer drew one quad per aggregate moving packet regardless of whether that packet represented a fraction of one logical piece or hundreds of pieces; therefore 1x could visually overstate tiny packets while the 1000x Lab stress test still showed only one flake per packet.

TIRE15C5A applies a 0.040 production-world loose-rubber calibration factor while leaving the existing Lab generation multiplier on top. Moving packet presentation now derives a bounded stable representative count from authoritative `piecePopulation`, with deterministic fractional rounding at low populations and multiple independently phased/tumbled visual flakes for dense packets. The Lab multiplier changes quantity, not launch impulse. Fresh flakes also receive a somewhat more readable but still bounded tire-driven launch so chase-camera testing can actually see their airborne phase.

Resting debris no longer uses segmented ribbon geometry. Airborne, resting and far-LOD rubber all use the same two-triangle deformable flake primitive. Presentation uses deterministic pile anchors that may cross the owning cell boundary, allows true visual overlap/stacking, and computes each representative position in FP64 world space before camera-relative FP32 conversion. Rotation remains a local FP32 basis because centimetre-scale angular precision is not the source of large-world jitter. The authoritative 0.5 m aggregate grid and 500k logical-piece budget are retained for scale, but the renderer no longer exposes that grid as square/chessboard placement.

Portable HeritagePhysicsTests still compile as 93 C++20 translation units with warnings-as-errors and pass after the production-rate regression was updated to use the existing developer multiplier when it intentionally needs a dense marble field. SurfacePresentationRenderer also passes warnings-as-errors syntax validation with the local OpenGL compatibility header. Windows/MSVC/in-game appearance remains the user validation gate.


TIRE16G fixes tire-mark middle-distance starvation/popping by expanding per-band render populations, prioritizes freshly generated marks for 3 seconds so the vehicle cannot outrun presentation, uses horizontal FP64 distance for ground-history LOD, replaces physical mark lift with polygon-offset depth bias, breaks ribbons across abrupt support-plane/kerb transitions, suppresses opaque low-speed lateral-drag marks, and raises production marble generation from 0.12 to 0.36 (exactly 3x).

## SHADOW02 candidate — layered cascades and batched depth submission

The directional CSM pass no longer loops over four cascades and resubmits every accepted mesh range four times. The four 3072² depth maps remain a texture array, but the framebuffer now attaches the array as one layered target. A dedicated shadow geometry stage receives a per-draw cascade bit mask and writes primitives to `gl_Layer`, preserving the existing cascade matrices, culling policy, tire deformation, skinning, reflected winding, depth bias and main-view reversed-Z restoration while moving cascade fan-out to the GPU.

The CPU evaluates each mesh instance/node pose once, resolves tire-deformation overrides once, tests static range bounds against all four cascade frusta once, and then submits each accepted shadow batch once. Contiguous draw ranges that differ only by visible material split and share node transform + skin are coalesced because the depth-only shadow pass does not consume material state. The F8 `shadow draws` counter now measures actual OpenGL shadow draw submissions; `shadow triangles` continues to represent emitted cascade triangle work. This is the first large CPU-submission reduction step; fully GPU-built indirect command lists remain a later option if large-grid/vegetation profiling still justifies them.

