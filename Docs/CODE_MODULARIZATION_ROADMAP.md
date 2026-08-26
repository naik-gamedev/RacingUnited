# Heritage Engine Code Modularization Roadmap

> **2026-08-25 current cleanup authority:** the CLEAN01–CLEAN13 program below is retained as historical architecture context. A fresh full-project audit found concrete new blockers (retired hydrology generations, a 1,568-line uncompiled TireCarcass implementation, fake source scaffolds, new renderer/runtime gravity wells). The active continuation is `CODE_HEALTH_OPTIMIZATION_ROADMAP_2026_08_25.md`. The old post-CLEAN13 stop rule is therefore not being ignored; its “concrete blocker” exception has been met.

## Purpose

This cleanup program is the architecture gate after the user-validated TIRE15 runtime and before
continuing the tire program toward TIRE16. The goal is not to change vehicle behavior. The goal is
to stop several fast-growing implementation files and positional APIs from becoming permanent
architectural bottlenecks while the project is still small enough to reorganize safely.

The cleanup follows three rules:

1. **Behavior first.** A cleanup step must preserve the current driving/physics result unless the
   step explicitly fixes a defect discovered during the refactor.
2. **One boundary at a time.** Every step is independently buildable/testable. Do not combine a
   renderer split, collision split and tire change into one archive.
3. **Responsibilities, not milestone numbers.** Files are split by enduring subsystem purpose.
   Do not create `TIRE10.cpp`, `TIRE11.cpp`, etc.; those are historical milestones, not long-term
   architecture.

TIRE16 remains blocked until the core cleanup exit criteria at the end of this document are met.
TIRE15B/TIRE15C remain valid tire/surface roadmap work and are not deleted by this gate.

## CLEAN01 — named wheel telemetry API — USER VALIDATED

Problem discovered by TIRE15:

- `Vehicle.GetWheelState` grew to 169 positional Lua return values.
- Direct Lua unpacking exceeded Lua 5.4's 200-local limit.
- After that was fixed, the native binding could overflow Lua's C stack unless it explicitly
  reserved all 169 result slots.

Cleanup:

- Add `Vehicle.GetWheelTelemetry(vehicle, wheelIndex)` returning **one named Lua table**.
- Put the current wheel state, contact-query diagnostics and authoritative upright pose in that one
  table.
- Migrate Racing United's first-party `Vehicles/Telemetry.lua` to the named table.
- Keep `Vehicle.GetWheelState`, `GetWheelContactDiagnostic` and `GetWheelUprightPose` registered as
  legacy compatibility APIs for now.
- Future tire/surface telemetry extends the table by field name instead of extending a positional
  ABI.

Why first: this removes the exact failure mode TIRE15 exposed and gives later tire milestones a
safe telemetry destination.

Validation gate: build, launch, enter the driving scene, drive, and verify the existing vehicle/
tire telemetry panels still update.

## CLEAN02 — split `VehicleSystem.cpp` by stable responsibility — USER VALIDATED

Current hotspot: `Vehicles/VehicleSystem.cpp` is roughly six thousand lines and combines public
vehicle lifetime/configuration, telemetry capture, vehicle-level stepping and a very large
per-wheel high-rate solver.

Target files:

```text
Vehicles/
    VehicleSystem.cpp
    VehicleConfiguration.cpp
    VehicleTelemetry.cpp
    VehicleSimulation.cpp
    VehicleWheelSimulation.cpp
```

Responsibility boundary:

- `VehicleSystem.cpp`: handles, create/destroy, body ownership, wheel registration, basic state and
  error/clock plumbing.
- `VehicleConfiguration.cpp`: suspension/fitment/alignment, anti-roll, chassis compliance, driver
  aids, drivetrain and tire/provider configuration.
- `VehicleTelemetry.cpp`: Dynamics Lab capture/readback/export and lightweight diagnostics.
- `VehicleSimulation.cpp`: top-level `simulate`, steering/driveline substeps, anti-roll/chassis
  interaction and vehicle-level orchestration.
- `VehicleWheelSimulation.cpp`: authoritative high-rate wheel/contact/tire substep.

The public `VehicleSystem.hpp` API remains stable during this physical split. A private
`VehicleSystemInternal.hpp` temporarily owns the exact pre-split implementation helpers shared by
the new translation units; CLEAN04 is responsible for graduating reusable quaternion/transform
math into `Core/Math`.

Implementation result: the former ~5,974-line translation unit is now approximately 431 lines of
lifetime/plumbing, 1,176 lines of configuration, 208 lines of telemetry, 652 lines of
vehicle-level simulation and 2,787 lines of authoritative wheel simulation. The giant wheel
substep is intentionally still intact for CLEAN03B.

Portable validation result: all five split translation units compile individually with C++20
warning-as-error checks, the full native physics regression executable builds/runs, all tests pass,
and its textual regression output is byte-identical to the CLEAN01A unsplit baseline. Windows
build/launch/drive remains the user validation gate.

Validation gate: compile/link all affected projects, run physics regressions, launch, drive and
compare telemetry with the pre-split build.

## CLEAN03A — subsystem configuration ownership + vehicle-topology scaffold — USER VALIDATED

CLEAN02 removed the six-thousand-line `VehicleSystem.cpp`, but its first configuration bucket still
combined unrelated subsystem APIs. CLEAN03A makes the intended ownership visible before those files
grow again.

Active configuration destinations:

```text
Vehicles/
    Core/VehicleRuntimeConfiguration.cpp
    Suspension/VehicleSuspensionConfiguration.cpp
    Suspension/Common/VehicleAntiRollBarConfiguration.cpp
    Wheels/Fitment/VehicleWheelFitmentConfiguration.cpp
    Wheels/Alignment/VehicleWheelAlignmentConfiguration.cpp
    Dynamics/ChassisFlex/VehicleChassisComplianceConfiguration.cpp
    Dynamics/MassProperties/VehicleUnsprungMassConfiguration.cpp
    Steering/VehicleSteeringConfiguration.cpp
    Brakes/VehicleBrakeConfiguration.cpp
    DriverAids/VehicleDriverAidConfiguration.cpp
    Drivetrain/VehicleDrivetrainConfiguration.cpp
    Tires/VehicleTireConfiguration.cpp
```

The historical root `VehicleConfiguration.cpp` is retained as a non-compiled signpost only. New
subsystem configuration must not accumulate there again.

CLEAN03A also adds non-compiled native topology destinations plus Lua authoring scaffolds for common,
two-wheel, three-wheel and four-plus-wheel vehicles. The common core remains arbitrary-wheel-count;
category-specific layers are reserved only for mechanics that genuinely differ (for example
motorcycle lean/balance). Cars, karts, ATVs and multi-axle trucks must not force an exactly-four-wheel
assumption into shared APIs. See `VEHICLE_TOPOLOGY_ARCHITECTURE.md`.

Validation gate: all moved configuration translation units compile/link in both engine and physics
regression projects, regressions remain behavior-identical, all Lua scaffolds parse, Windows build
succeeds, and the current Peugeot launches/drives normally.

## CLEAN03B — wheel-substep phase partition — USER VALIDATED

CLEAN03B reduces the root `VehicleWheelSimulation.cpp` from roughly 2,787 lines to a small
orchestrator while preserving the exact validated statement order and floating-point evaluation
order. The authoritative per-wheel path is now physically partitioned under:

```text
Vehicles/Simulation/WheelSubstep/
    00_PrepareWheelAndSupportQuery.inl
    01_TelemetryAndAirbornePolicy.inl
    02_SteeringBrakingAndFreeWheel.inl
    03_RoadEnvelopeAndFootprintSampling.inl
    04_TireStructureAndTerrainSupport.inl
    05_SuspensionAndContactResolution.inl
    06_ContactKinematicsAndPatchGeometry.inl
    07_SurfaceProvidersAndContactPatch.inl
    08_TireForcesAndSurfaceReactions.inl
    09_TirePhysicalStateUpdate.inl
    10_ApplyForcesAndIntegrateWheel.inl
```

The phase files are deliberately function-scope implementation fragments included by
`VehicleSystem::simulateWheelSubstep()`. This is a behavioral-preservation technique: the old
solver has many cross-phase intermediates, so CLEAN03B does **not** simultaneously invent a giant
context object or rewrite every dependency just to satisfy a file split. The new boundaries make
ownership and execution order explicit without perturbing the physics. As individual phase
contracts stabilize, a later cleanup may graduate a phase to a private compiled helper with narrow
inputs/outputs.

Provider ownership remains unchanged. Wet, winter, granular, deformable-terrain, thermal, wear,
rigid-ring, contact-patch and Magic Formula mechanisms stay in their dedicated provider files; the
wheel-substep phases only orchestrate/couple them. The common per-wheel path remains arbitrary
wheel-count and must not acquire an exactly-four-wheel assumption. Topology-specific whole-vehicle
behavior stays under `Vehicles/Topology/`.

Portable validation result: the complete native physics regression executable builds/runs with
C++20 warnings-as-errors and its full textual output is byte-for-byte identical to the
user-validated CLEAN03A baseline. Windows build/launch/drive remains the user validation gate.

Validation gate: project validation, Windows compile/link, launch, drive, and unchanged telemetry/
handling behavior.

## CLEAN04 — shared transform/quaternion math + collision split

CLEAN04 is intentionally split into two validation checkpoints so math-convention consolidation and
physical collision-file movement are never debugged at the same time.

### CLEAN04A — shared quaternion foundation + collision ownership scaffold — USER VALIDATED

Repeated quaternion representation/algebra used by entity hierarchy, rigid bodies, collision and
vehicle simulation is centralized in `Core/Math/Quaternion.hpp`. Existing subsystem wrappers keep
their historical normalization/space policy so the cleanup does not silently change floating-point
behavior. `Core/Math/TransformMath.hpp` is created deliberately as a small ownership scaffold; it
will only gain helpers whose coordinate-space contracts are genuinely shared.

CLEAN04A also creates the non-compiled collision responsibility destinations before code is moved:

```text
Physics/Collision/
    Queries/CollisionQueries.cpp
    Narrowphase/CollisionNarrowphase.cpp
    Solver/CollisionSolver.cpp
    CCD/CollisionCCD.cpp
    Islands/CollisionIslands.cpp
```

Portable physics regression output is required to remain byte-identical to CLEAN03B. Windows
validation remains build, launch and drive.

### CLEAN04B — physical collision responsibility split — USER VALIDATED

Split the current collision implementation by enduring responsibility:

```text
Physics/
    CollisionSystem.cpp
    Collision/
        CollisionInternal.hpp
        Queries/CollisionQueries.cpp
        Narrowphase/CollisionNarrowphase.cpp
        Solver/CollisionSolver.cpp
        CCD/CollisionCCD.cpp
        Islands/CollisionIslands.cpp
```

- system: collider lifecycle, filters/materials, orchestration;
- queries: ray/sphere/overlap/static-scene queries;
- narrowphase: pair/triangle contact generation;
- solver: cache, warm start, position/velocity constraint solving;
- CCD: continuous collision detection;
- islands: island construction, sleep/wake.

The root `CollisionSystem.cpp` remains the coordinator for collider/static-scene lifetime, broadphase
and fixed-step orchestration rather than becoming another umbrella implementation. Low-level numerical
constants/helpers shared by the collision translation units live in the private `CollisionInternal.hpp`;
it is not a public collision API. CLEAN04B moves existing member-function bodies without intentionally
changing collision equations or solver order.

Portable validation requires all six collision translation units to compile with C++20
warnings-as-errors, the full HeritagePhysicsTests suite to pass, and complete regression stdout to be
byte-identical to the user-validated CLEAN04A archive. Windows validation remains build, launch, scene
collision and vehicle drive.

## CLEAN05 — renderer responsibility split — USER VALIDATED

The former ~3,861-line `EntityMeshRenderer.cpp` mixed five stable responsibilities: material draw
orchestration, GLB asset caching/hot reload, animation/node evaluation, cascaded-shadow resources,
and roughly 750 lines of embedded GLSL. CLEAN05 physically separates those responsibilities while
keeping the public `EntityMeshRenderer` interface stable.

Active ownership:

```text
Graphics/Renderer/
    EntityMeshRenderer.cpp             # lifecycle + material/environment draw orchestration
    EntityMeshAssetCache.cpp           # paths, GLB/OBJ cache, dependencies and hot reload
    EntityMeshAnimation.cpp            # clips, node state/overrides and skin palettes
    EntityMeshShadows.cpp              # shadow resources, cascades and shadow pass
    EntityMeshRenderMath.cpp           # private renderer transform/frustum math
    EntityMeshRendererInternal.hpp     # private cross-TU declarations only
    EntityMeshShaders.hpp              # embedded material + shadow GLSL
    EntityMeshShadowConfig.hpp         # shadow quality policy/preset scaffold
```

The shader programs remain embedded C++ on purpose. External shader files are deferred until
Heritage owns a real shader asset/deployment/hot-reload pipeline; CLEAN05 does not create a second
ad-hoc deployment mechanism.

Shadow quality is also centralized here instead of remaining a magic number in the renderer. The
current four-layer cascaded texture array now defaults to the **High** preset at **3072×3072 per
cascade**, up from 2048×2048. Low/Medium/High/Ultra preset constants (1024/2048/3072/4096) are
scaffolded in `EntityMeshShadowConfig.hpp` so a later graphics setting can select them without
another ownership refactor. Runtime allocation clamps the requested resolution to the GPU-reported
`GL_MAX_TEXTURE_SIZE`. The existing texture-array design still uses one resolution for every layer;
per-cascade resolution is a later shadow-system feature, not mixed into this cleanup.

Behavior-preservation rule: asset loading, animation sampling, node overrides, skinning, tire visual
deformation, environment rendering, cascade math, shadow draw order and material draw order are not
intentionally changed by the split. The only intentional visual/performance change is the higher
default shadow-map resolution.

Validation gate: project files compile all new translation units; the root renderer remains a small
orchestrator; shader source has one owner; vehicle/scene GLBs render; animations/node overrides and
tire deformation work; shadows/environment render; hot reload works; Windows launch/drive succeeds.

## CLEAN06 — input and glTF importer splits — USER VALIDATED

CLEAN06 physically separates two long-lived engine services while preserving their public interfaces.
The purpose is ownership and future build isolation, not behavioral change.

### Input

```text
Input/
    InputSystem.cpp             lifecycle, module action definitions, action updates
    InputBindings.cpp           action/binding editing, capture, parsing, analogue processing
    InputDevices.cpp            keyboard/mouse/gamepad/DirectInput hardware state
    InputProfiles.cpp           named profile snapshots and CRUD
    InputPersistence.cpp        live settings save/load
    InputSystemInternal.hpp     private shared helpers only
```

The public `InputSystem.hpp` contract remains stable. New device backends, profile formats, or binding
features should go to their owning unit rather than returning to the root coordinator.

### glTF

```text
Graphics/
    GltfBinary.hpp              stable public include/API
    GltfBinary.cpp              retired by OPT01; implementation lives in Graphics/Gltf/
    Gltf/
        GltfBinary.cpp          public facade implementations
        GltfJson.cpp            JSON parser
        GltfDocument.cpp        GLB container/accessor decoding and transform math
        GltfMeshImporter.cpp    images/materials/primitives/skins/animations
        GltfMetadata.cpp        extras/node hierarchy metadata
        GltfCollisionImporter.cpp collision/spawn authoring extraction
        GltfInternal.hpp        private importer vocabulary/contract
```

The public include path remains `Graphics/GltfBinary.hpp`; callers do not need to know how the
importer is partitioned. The old root implementation file is deliberately a non-compiled signpost so
future code does not drift back into a single importer dumping ground.

Validation gate: all new translation units are project-owned exactly once; the old glTF implementation
is not compiled; InputSystem retains the same member-definition inventory; glTF internal units compile
cleanly; input profiles/binding capture survive restart; current scene and vehicle GLBs load identically
with materials, metadata, animation and collision; Windows launch/drive succeeds.

## CLEAN07 — Heritage Engine shell and domain-owned Lua registration — USER VALIDATED

CLEAN07 removes process/runtime ownership from the executable entry point without pretending that the
whole frame loop can be safely decomposed in one rewrite. The executable `main.cpp` becomes a tiny
entry point which creates `heritage::engine::HeritageEngine` and calls `run(argc, argv)`.

Active ownership after this step:

```text
HeritageEngine/
    main.cpp                         # executable entry point only
    HeritageEngine.hpp/.cpp          # process + frame orchestration coordinator
    Runtime/
        EngineStartup.hpp/.cpp       # project-root discovery + launch diagnostics
        EngineUiStyle.hpp/.cpp       # global ImGui style policy
        EngineFrame.cpp              # non-compiled future phase destination
        EngineSimulation.cpp         # non-compiled future phase destination
        EngineRendering.cpp          # non-compiled future phase destination
        EngineHotkeys.cpp            # non-compiled future phase destination
    Display/
        DisplayModeController.hpp/.cpp # mode switching, confirmation/revert policy
Core/Diagnostics/
    PerformanceOverlay.hpp/.cpp       # F8 diagnostic UI
Platform/Windows/
    BackbufferClipboard.hpp/.cpp      # F12 exact-backbuffer clipboard bridge
```

The four runtime phase files are deliberate scaffolds, not fake abstraction. CLEAN07 does **not**
manufacture one enormous mutable `FrameContext` merely to move lines out of `HeritageEngine.cpp`.
As frame/simulation/render/hotkey contracts become narrow and stable, code can move into those
compiled units without changing the public executable/runtime boundary. New unrelated logic should
not accumulate in the coordinator simply because it is convenient.

Lua registration ownership also changes. `LuaModuleRuntime::registerBindings()` retains only the
runtime-owned `print` override and an ordered list of domain registration calls. UI, Engine,
Environment, Vegetation, Script, Scene, Save, Audio, Input, Prefab and Module registrations live in
their existing binding-domain files. Large Entity/Physics/Vehicle tables have dedicated registration
translation units beside their split handlers. The Lua manifest generator scans the distributed
registration source set and still verifies exact names, unique registrations and handler ownership.

The runtime therefore owns Lua state lifetime, script loading/sandboxing, protected calls, errors,
reload and lifecycle callbacks; API domains own their registration catalogues and implementations.

Validation gate: `main.cpp` remains a tiny entry point; all active CLEAN07 units are compiled exactly
once; runtime-phase scaffolds remain non-compiled; display mode apply/change/revert still works;
F8 diagnostics and F12 capture still work; generated Lua API count/names are unchanged; Windows
build launches and the current Racing United scene/Peugeot behave normally.

## CLEAN08 — Lua responsibility cleanup — IMPLEMENTED CANDIDATE

CLEAN08 applies the same ownership rule to Racing United Lua without moving deterministic physics out
of native C++. Existing root include paths remain as small compatibility coordinators where useful, so
creator scripts do not need a flag-day rename. New implementation belongs to the responsibility-owned
subdirectories.

```text
Vehicles/Suspension/
    HardpointSources.lua          # provenance/source priority + GLB metadata import
    HardpointEstimation.lua       # assisted low-confidence package estimates
    SuspensionAuthoring.lua       # native activation/facade
    HardpointGizmos.lua           # creator-only debug marker presentation

Vehicles/Visual/
    TransformMath.lua             # presentation-only quaternion/transform helpers
    ArticulatedWheels.lua         # separate/proxy wheel presentation
    EmbeddedWheelBinding.lua      # GLB WH_* semantic binding + tire deformation
    VisualWheels.lua              # per-frame presentation coordinator

Vehicles/Definitions/
    VehicleDefinitionV2Validation.lua          # schema/core report orchestration
    VehicleDefinitionV2DynamicsValidation.lua  # suspension/contact/ARB/flex/drive validity
    VehicleDefinitionV2Compatibility.lua       # valid definition vs current native preview support
```

`Runtime/Lifecycle.lua` now dispatches the fixed-step physics-demo update rather than implementing
raycasts, overlaps, spring diagnostics and CCD monitoring itself. Physics-demo destruction and legacy
probe cleanup move out of `Runtime/Common.lua` and into `Runtime/PhysicsDemo.lua`; truly shared entity
helpers remain common.

The split deliberately keeps `PrototypeCar.lua` and other coherent authored vehicle definitions intact.
Lua continues to configure/orchestrate while high-rate tire, suspension, drivetrain, collision and other
networking-critical simulation remains native.

Validation gate: all new responsibility files are present and loaded in deterministic order; the root
coordinators contain no returned implementation dumping grounds; VehicleDefinitionV2 semantic outcomes
remain unchanged; suspension authoring/import/gizmos and articulated/embedded wheel presentation behave
unchanged; lifecycle dispatches physics-demo work; Windows build launches and the Peugeot drives.

## Post-CLEAN08 inspection program — CLEAN09 through CLEAN13

A full-project inspection after CLEAN08 found several remaining boundaries worth establishing before
returning to TIRE16. These are not emergency bug fixes. They are the last deliberate future-proofing
pass so new tire, motorcycle, truck, trike, editor and large-grid work does not immediately recreate
the same central dumping grounds. The current directive is to complete CLEAN09-CLEAN13 one validated
checkpoint at a time, then stop architecture-only cleanup and resume tire/surface development.

### CLEAN09 — finish Heritage Engine runtime ownership + build-loop hygiene

**CLEAN09 user validated (2026-08-10):** the four runtime phase scaffolds are now compiled owners; process-lifetime settings/services are explicit `HeritageEngine` state; the rolling helper defaults to incremental MSBuild and accepts `FULL` for explicit `/t:Rebuild`. See ADR-056.

`main.cpp` is already a tiny entry point, but `HeritageEngine.cpp` remains a large process/frame
coordinator. Promote the existing non-compiled runtime scaffolds into real owners only where narrow
contracts are clear:

```text
HeritageEngine/Runtime/
    EngineFrame.cpp
    EngineSimulation.cpp
    EngineRendering.cpp
    EngineHotkeys.cpp
```

Persistent runtime state should become explicit `HeritageEngine` state rather than a collection of
implementation-file globals. Do not solve this by creating one enormous mutable frame context.

The normal developer helper should use incremental MSBuild (`/t:Build`) instead of forcing both the
engine and physics tests through `/t:Rebuild` on every run. Retain an explicit full clean-rebuild path
for checkpoints. Longer-term, shared native simulation code should become one reusable library linked
by both the engine and the regression executable so the same source is not compiled twice.

### CLEAN10 — world-owned driven-surface state

**CLEAN10 user validated (2026-08-10):** implemented as `Physics/Surfaces/SurfaceWorld.*` owning a chunked `SurfaceField.*`, with `PhysicsWorld` as the authoritative owner. Local tire contact points are converted through the current FP64 global origin before field addressing; floating-origin shifts therefore do not re-key driven history. Addressing also carries a coarse global-Y layer so a bridge does not share state with the road beneath it. The field uses bounded sparse LRU chunks plus chunk snapshot/restore/eviction callbacks for future streaming. `VehicleSystem` no longer owns `m_surfaceField`. TIRE15C originally received a dedicated non-compiled `Physics/Surfaces/Rubber/TrackRubberState.*` ownership scaffold; that scaffold is promoted to the compiled dynamic rubber/marble subsystem in TIRE15C (ADR-063). See ADR-057 for the original ownership decision.

`SurfaceField` is conceptually world/physics state but is currently owned by `VehicleSystem`. Move it
to a world surface subsystem under `Physics/Surfaces/` (or the final equivalent ownership path) so
weather, every vehicle, presentation, persistence and multiplayer can access one authoritative field.

The redesign must be floating-origin safe. Spatial keys may not silently depend on rebased local X/Z
coordinates. Use global/world-stable coordinates or an equivalent rebasing-aware addressing scheme.

Replace the current small flat sparse-cell budget with chunked/tiled storage, bounded active-tile
caching and efficient eviction/streaming hooks appropriate for long circuits, stages and large grids.
Do not allow an eviction policy that scans the entire field for every replacement once full.

Rubbering-in may reuse world spatial infrastructure, but **tire marbles remain a specialized rubber
subsystem**: bulk deposited-rubber and loose-rubber concentration can be represented efficiently as
world surface state, while visible marble clusters, migration/pickup presentation and any sparse
high-detail physical debris are owned by dedicated rubber/marble code rather than pretending marbles
are generic mud/snow terrain.

### CLEAN11 — tire-part authoring architecture

**CLEAN11 user validated (2026-08-10):** the former monolithic `MagicFormula/TirePropertyFile.cpp` is now a small public façade over compiled `Tires/Authoring/` owners for raw parsing/units, shared mapping, MF coefficients, common metadata, Heritage-owned extensions, structural/enveloping metadata and diagnostics/final validation. A compiled `TirePartDefinition` contract establishes bounded Dry/Wet/Snow-Ice/Mud/Sand/Gravel/Wear-Endurance creator biases as parameter-generation inputs, not final-force multipliers. Runtime vehicle/tire equations are intentionally unchanged. See ADR-058.

Split the fast-growing tire-property authoring/import path by stable responsibility before TIRE16
adds more tire families. Establish a `Tires/Authoring/` boundary for parsing/units, common physical
metadata, Magic Formula coefficients, structural/enveloping properties, Heritage extensions and
validation/diagnostics. Tire families remain data/presets selecting reusable mechanisms unless a
genuinely different physical model is required.

Also establish a reusable **tire part definition** that can be referenced by vehicles instead of
embedding every tire choice directly into one car. Its simple authoring layer includes bounded Dry, Wet,
Snow/Ice, Mud, Sand, Gravel and Wear/Endurance biases mapped onto coherent physical mechanisms rather
than final-force multipliers. These controls are topology- and vehicle-family-neutral: road cars and
motorcycles can leave pavement just as trucks/ATVs can. The intended creator workflow is described in
`TIRE_PART_AUTHORING_ROADMAP.md`.

### CLEAN12 — Lua runtime header boundary + validator modularization

**CLEAN12 user validated (2026-08-10):** `LuaModuleRuntime.hpp` is reduced from 644 lines in the pre-cleanup baseline to roughly 225 lines. The 400 Lua C-handler declarations are now private domain catalogues (Core/Physics/Vehicle/Entity), service implementation headers are no longer transitively dragged through the runtime header, and binding translation units include the subsystem they actually use. The public Lua registration set remains 410 functions. `ValidateProject.ps1` is now a small ordered runner over responsibility-owned modules under `Tools/Validation/`. See ADR-059.

Domain-owned Lua registration is complete, but the central runtime header still exposes a very large
static binding-handler catalogue. Introduce lightweight binding/domain interfaces so adding or
changing a Vehicle binding does not force unrelated binding domains to include/recompile the whole
Lua runtime declaration surface. Lua-state lifetime and sandbox policy remain runtime-owned.

`Tools/ValidateProject.ps1` has also grown large enough to become a failure source itself. Keep the
user-facing Tools folder lean: retain one top-level validator command, but move responsibility-owned
checks/manifests under `Tools/Validation/` and prefer structured/data-driven checks over fragile giant
string searches.

### CLEAN13 — remaining high-value ownership splits

**CLEAN13 USER VALIDATED (2026-08-10):** entity, input binding, input-settings UI and collision broadphase ownership are physically split; the standalone Launcher is retired from the active solution; deep project-state history is archived under `Docs/History/`. This is the final planned architecture-only cleanup checkpoint. See ADR-060.

Finish only boundaries with clear long-term ownership:

- split `EntityRegistry.cpp` into lifetime/hierarchy/transforms/component ownership;
- split `InputBindings.cpp` into action binding, analogue processing, capture and parser/name logic;
- split the large input-settings UI according to its existing Bindings/Analogue/Profiles concepts;
- add a dedicated collision broadphase owner so the collision root remains orchestration/lifetime;
- inspect and retire the old standalone Launcher project if no retained workflow still depends on it;
- keep `PROJECT_STATE.md` focused on current state and move deep historical milestone narrative into
  `Docs/History/` as it grows.

Do **not** split `RigidBodySystem.cpp` merely because of line count; it remains comparatively cohesive.
Do not create speculative micro-files without an enduring owner/responsibility.

### Post-CLEAN13 stop rule

After CLEAN13 is user-validated, architecture-only cleanup stops unless a concrete blocker is found.
Return to TIRE15B/TIRE15C/TIRE16 on the cleaned foundation. New scaffolding is added early when a
known subsystem boundary exists, but tidying must not become an endless substitute for simulation work.

## Files deliberately not targeted merely for line count

- Tire providers that already map cleanly to physical mechanisms.
- Cohesive per-vehicle Lua definition/config files.
- Test files whose large size is one coherent regression suite.
- `RigidBodySystem.cpp` until shared transform math has been extracted and an actual responsibility
  boundary justifies another split.

## TIRE16 unblock criteria

TIRE16 may resume when:

- CLEAN01 is user-validated;
- `VehicleSystem.cpp` no longer acts as the six-thousand-line vehicle dumping ground (CLEAN02);
- subsystem configuration/topology ownership is explicit (CLEAN03A);
- the giant wheel solver has clear internal phase boundaries (CLEAN03B);
- shared transform math and collision responsibilities are no longer duplicated/monolithic enough
  to threaten vehicle work (CLEAN04);
- the project validation script passes;
- Windows build succeeds, game launches, and the user confirms the current Peugeot still drives.

CLEAN05-CLEAN08 have now been performed as part of the same tidying program. The post-CLEAN08
inspection adds CLEAN09-CLEAN13 as the current voluntary pre-TIRE16 finish pass. After CLEAN13,
architecture-only cleanup stops unless a concrete blocker is discovered; tire/surface development
resumes. Any deviation should be documented in `PROJECT_STATE.md` rather than silently abandoning
the cleanup.

