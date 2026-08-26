# Heritage Engine Code Health & Optimization Roadmap — 2026-08-25

## Purpose

This roadmap is based on a fresh full-project inspection of the `CLOUDURP15BL` tree. It reopens architecture cleanup for a concrete reason: several files have again become multi-system gravity wells, the production water path still carries superseded live-solver generations, and validators now preserve some historical implementation details that are no longer runtime authority.

The program has two goals that must remain separate:

1. **Code-health optimization:** delete proven dead/retired code, remove fake source scaffolds, split active multi-responsibility files, reduce global Lua coupling, and make ownership obvious.
2. **Runtime optimization:** measure CPU/GPU pass cost first, then optimize the passes that are actually expensive. Refactoring must not be sold as an FPS improvement unless profiling proves it.

The guiding rule is **one authority, one owner, one purpose per file**. Small cohesive files are preferred, but arbitrary micro-file splitting is not.

## Execution status

- **OPT00 — IMPLEMENTED:** asynchronous GPU pass timing and repeatable static code-health snapshot are now in the tree. Runtime behavior is intentionally unchanged.
- **OPT01 — NEXT:** retire only the proven dead/unreachable sources and replace fake vehicle source scaffolds with an honest architecture manifest/document.

---

## Fresh-tree baseline

Static inspection of `Engine/HeritageEngine` + `Modules/RacingUnited` found approximately:

- **498 C/C++/INL files** / **135,764 lines**;
- **94 Lua files** / **11,723 lines**;
- **21 C/C++ files at or above 1,000 lines**;
- **one Lua file above 500 lines**;
- **54 `.cpp` files present on disk but not compiled by either active Visual Studio project**;
- of those 54, **50 are deliberate vehicle architecture scaffolds**, three are completed migration/signpost translation units, and one is a large abandoned implementation (`TireCarcass3D.cpp`);
- **five Lua files are unreachable from `Scripts/Main.lua` through the current include graph**: one old Cloud Lab implementation and four empty topology scaffolds.

The most important conclusion is that the tree is not generally chaotic. Most subsystems are already reasonably decomposed. The cleanup should therefore be surgical and evidence-driven rather than another broad rewrite.

---

# Evidence classes

## A. Proven dead / unreachable — remove, do not refactor

These have the strongest deletion evidence.

### `Vehicles/Tires/TireCarcass3D.cpp/.hpp`

- `TireCarcass3D.cpp`: **1,568 lines**.
- `TireCarcass3D.hpp`: **214 lines**.
- Neither file is compiled by the engine or regression project.
- No active C++ source includes the header or calls `evaluateTireCarcassContact3D()`.
- Current validators already assert that stale TIRE18–TIRE21 carcass state is absent from the live wheel substep/telemetry.

**Disposition:** delete both files. Move the TIRE20 design narrative in `Docs/TIRE_MODEL.md` to historical documentation if it is still useful as research context. Do not spend time modularizing 1,782 lines that the program never builds.

### `Modules/RacingUnited/Scripts/UI/Prototype/CloudLabPanel.lua`

- **234 lines**.
- Not included directly or indirectly from `Scripts/Main.lua`.
- Its exported `DrawPrototypeCloudLabPanel()` has no caller in the module.

**Disposition:** delete. The current Scene/Weather UI owns cloud/weather authoring.

### Root migration/signpost `.cpp` files

- `Graphics/GltfBinary.cpp` — 4-line CLEAN06 signpost; real implementation is under `Graphics/Gltf/`.
- `Physics/SurfaceField.cpp` — 2-line CLEAN10 compatibility signpost; real implementation is under `Physics/Surfaces/`.
- `Vehicles/VehicleConfiguration.cpp` — 4-line retired umbrella signpost; active configuration lives in subsystem files.

These contain no runtime implementation and are intentionally not compiled.

**Disposition:** remove the signpost-as-source pattern. Preserve the ownership rule in architecture documentation and validators instead of keeping fake translation units in the source tree. `VehicleConfiguration.cpp` currently has a validator that explicitly requires the signpost, so update that validator in the same checkpoint.

---

## B. Future scaffolds disguised as code — replace with architecture metadata/docs

The engine project contained a `VehicleArchitectureScaffolds` group with **51 non-compiled `.cpp` entries**: 50 future-mechanism placeholders plus the retired `VehicleConfiguration.cpp` signpost. Racing United also has four unreachable empty Lua topology modules:

- `Vehicles/Topology/Common.lua`
- `Vehicles/Topology/TwoWheel.lua`
- `Vehicles/Topology/ThreeWheel.lua`
- `Vehicles/Topology/FourPlusWheel.lua`

These are not legacy behavior, but they create false source inventory and make automated dead-code inspection noisy.

**Disposition:** replace placeholder source files with one maintained architecture document/manifest describing intended future owners and paths. Create a source file only when there is an implementation to compile. Update `30_VehicleAndContentArchitecture.ps1`, which currently requires at least 20 fake scaffold `.cpp` files to exist.

This keeps the useful architectural intent while making the source tree aesthetically honest: **code files mean code**.

---

## C. Active production dependencies containing superseded runtime generations — isolate, verify, then remove

These must not be deleted in one shot because the current production path still consumes a subset of their responsibilities.

### `Physics/Surfaces/Water/SurfaceHydrology.cpp` — 4,832 lines

This is the largest source file and the clearest legacy stack. It currently contains at least four generations/responsibilities:

1. immutable collision-derived support/bake data;
2. current `.hhyd v15` prebaked triangle topology + near/far tile payloads;
3. precipitation-cover queries;
4. retired WATER14–WATER17 adaptive live-water solver/presentation machinery.

The live renderer comments explicitly state that the old adaptive `SurfaceHydrology` solver is **no longer advanced by `SurfaceWorld`**. The current GPU runtime still legitimately consumes:

- `rasterPrebakedPuddleResponseTile()`;
- `prebakedFarPuddleResponseTile()`;
- `hasPrecipitationCoverAbove()`;
- bake/cache/stat metadata.

By contrast, the adaptive solver is retained mainly by historical regressions and `TireFleetBenchmark`:

- `AdaptiveCell` / `AdaptivePipe`;
- adaptive topology and virtual-pipe construction;
- presentation basins;
- cadence scheduling;
- `advance()` / `simulateStep()`;
- legacy `applyTireContact()` water solver path;
- `collectVisualCells*()` adaptive presentation/debug gathers;
- `VirtualPipeFlow.hpp`.

The architecture validator still contains WATER14 checks that explicitly require `AdaptiveCell`, `AdaptivePipe`, the old virtual-pipe kernel, and adaptive regression strings. Those checks now protect historical code rather than current production authority.

**Disposition:** extract the current prebaked authority first, migrate current tests, then delete the adaptive generation and its validator requirements.

### `Physics/Surfaces/DynamicSurface/DynamicSurfaceHydrology.cpp` — 1,180 lines

This is still compiled as a CPU fallback and is stepped only while the renderer-owned GPU authority is unavailable. Once GPU authority is ready, `SurfaceWorld` does not advance it.

Because the project now deliberately targets modern OpenGL 4.6-class GPU features for the advanced live surface path, maintaining a second water solver indefinitely is architectural duplication.

**Disposition:** do not delete until the current GPU path has explicit startup/failure behavior and tire-contact sampling is no longer dependent on a CPU fallback. Then retire CPU Hydro from production, leaving Dynamic Surface responsible for the mechanisms that still need it (Track/thermal/static support) rather than keeping two water authorities.

### `Graphics/DynamicSurface/DynamicSurfaceGpuRuntime.cpp` — 1,637 lines

Despite the name, this is no longer merely a prototype. It initializes every normal renderer startup and currently owns the LIVETRACK21 prebaked near/far water path plus optional snow/mud state and tire-event compute.

**Disposition:** rename it to reflect production ownership and split it by stable responsibility. Do not leave a production authority named `Prototype`.

---

# Active file hotspots and intended ownership splits

| Current file | Lines | Target ownership |
| --- | ---: | --- |
| `SurfaceHydrology.cpp` | 4,832 | prebaked bake/cache/topology/raster/cover; delete retired adaptive solver |
| `TireModelRegression.cpp` | 3,126 | split tests by MF/core/thermal/wear/wet/winter/granular/fleet |
| `SurfacePresentationRenderer.cpp` | 2,446 | coordinator + TireMarkRenderer + MarbleRenderer + SurfaceParticleRenderer + shaders |
| `TrackRubberState.cpp` | 2,187 | storage/LRU + contact/deposition + moving packets + presentation extraction |
| `EntityMeshShaders.hpp` | 2,112 | material/surface/shadow shader owners |
| `DynamicSurfaceGpuRuntime.cpp` | 1,637 | production GPU runtime + residency + water + optional states + tire events + timers |
| `RigidBodySystem.cpp` | 1,628 | lifetime/properties + forces/impulses + integration + private math if needed |
| `TireCarcass3D.cpp` | 1,568 | **delete; not compiled/referenced** |
| `SurfaceWorldRegression.cpp` | 1,425 | current prebaked water tests + dynamic surface + rubber/thermal; remove archived adaptive tests |
| `SkyRenderer.cpp` | 1,391 | sky/celestial + volumetric clouds + cloud shadows + shader sources |
| `LuaModuleRuntime.cpp` | 1,311 | lifecycle + script loader + safety + calls + hot reload + scene transitions |
| `DynamicSurfaceBake.cpp` | 1,309 | bake construction + cache serialization/deserialization |
| `EntityMeshRenderer.cpp` | 1,198 | frame coordinator + opaque material submission + post-opaque pipeline stages |
| `DynamicSurfaceHydrology.cpp` | 1,180 | retire CPU water fallback after GPU-only cutover |
| `LuaUiBindings.cpp` | 1,173 | layout + basic widgets + images + plots/resources |
| `WindowsDirectInputBackend.cpp` | 1,166 | device lifecycle/polling + capture + FFB + platform stub |
| `SurfacePresentation.cpp` | 1,155 | tire-mark state + particle/event state + audio event accumulation |
| `CollisionSystem.cpp` | 1,122 | keep mostly coordinator; move only if a clear remaining owner emerges |
| `VehicleSystem.hpp` | 1,035 | public system interface + separate public description/state/type headers |
| `HeritageEngine.cpp` | 1,017 | process/service startup + settings + frame loop orchestration |
| `EntitySceneDocument.cpp` | 1,012 | document IO/parsing + scene application/serialization if future growth continues |

Line count is a trigger for inspection, not an automatic mandate. A 1,000-line coherent data parser can be healthier than a 500-line file owning three systems.

---

# OPT00 — measurable baseline and anti-regression instrumentation

Before deleting or moving large code, establish a baseline that proves behavior and performance do not regress.

## Code-health baseline

Record on every architectural checkpoint:

- compiled translation units;
- project-visible but non-compiled `.cpp` files;
- top 30 C++/Lua files by line count;
- Lua runtime reachability from `Scripts/Main.lua`;
- validator count/pass result;
- physics regression pass result.

## Runtime performance baseline

Add non-blocking GPU timestamp/query timing around durable rendering passes instead of inferring GPU cost from CPU submit time:

- directional shadow generation;
- opaque entity/material pass;
- environment-map update;
- sky/celestial background;
- volumetric cloud raymarch;
- cloud upscale/combine;
- cloud TAA;
- cloud shadow pass;
- Dynamic Surface compute/tire events;
- surface wetness/water material pass;
- tire marks;
- marbles/moving rubber;
- weather/rain presentation;
- vegetation;
- UI/final present where measurable.

Do not add `glFinish()`/blocking query reads to the steady-state measurement path. Read query results asynchronously several frames later.

**Exit gate:** one repeatable visual/performance baseline before structural changes.

---

# OPT01 — proven dead-code retirement ✅ COMPLETE

This checkpoint should be intentionally boring and low risk.

Delete:

- `Vehicles/Tires/TireCarcass3D.cpp`;
- `Vehicles/Tires/TireCarcass3D.hpp`;
- `Scripts/UI/Prototype/CloudLabPanel.lua`;
- root migration signposts (`Graphics/GltfBinary.cpp`, `Physics/SurfaceField.cpp`, `Vehicles/VehicleConfiguration.cpp`) after their validator/doc ownership checks are updated;
- empty topology Lua source scaffolds after topology intent is represented in architecture documentation.

Then replace the 51 non-compiled vehicle `.cpp` placeholders with an architecture manifest/document rather than fake translation units.

**Expected cleanup:** more than **2,000 lines of genuinely dead/unreachable code** immediately, plus roughly **60 misleading source files** when scaffolds/signposts are retired.

**Exit gate:** project validation, engine build, physics regressions, Lua startup, current scene launch/drive; source/project search proves removed symbols have no callers.

**OPT01 completion note:** 54 uncompiled `.cpp` files were retired, including the orphan TireCarcass3D implementation and completed migration signposts; its orphan header and all 5 unreachable Lua files were also removed. The 50 planned vehicle implementation seams now live in `VEHICLE_SUBSYSTEM_ARCHITECTURE_MANIFEST.md` rather than fake translation units.

---

# OPT02 — one prebaked hydrology owner

This is the highest-value structural checkpoint.

## Target layout

```text
Physics/Surfaces/Water/
    PrebakedHydrology.hpp
    PrebakedHydrology.cpp                 # small public facade/state
    PrebakedHydrologyBake.cpp             # collision -> welded/static hydrology data
    PrebakedHydrologyTopology.cpp         # spill/catchment/MFD triangle topology
    PrebakedHydrologyTileRaster.cpp       # 10m near 256x256 reconstruction
    PrebakedHydrologyFarCache.cpp         # 32x32 world-tile payload/index/decode
    PrebakedHydrologyCache.cpp            # .hhyd fingerprint/read/write/versioning
    PrecipitationCover.cpp                # same-column cover query
```

Names may be adjusted, but the responsibility boundary should remain.

## Remove after extraction

Once the current `.hhyd v15` path has equivalent regressions:

- adaptive live `AdaptiveCell` / `AdaptivePipe` state;
- presentation-basin state;
- adaptive cadence scheduler;
- `advance()` / `simulateStep()` legacy water stepping;
- old adaptive tire-contact mutation;
- adaptive debug/presentation cell gathers no production renderer uses;
- `VirtualPipeFlow.hpp`;
- WATER14/WATER16/WATER17 regressions that only defend the retired solver.

Keep tests for the **current** authority: exact depth ladder, spill levels, MFD runoff, flow direction, far-cache encoding, layered surfaces/curbs, precipitation cover, cache compatibility and near/far tile reconstruction.

## Validator correction

Replace exact WATER14 implementation-string checks with current authority invariants. A validator must prevent architectural regression, not force dead algorithms to remain in source.

**Exit gate:** same `.hhyd` output for fixed fixture scenes, same runtime prebaked tile output, same precipitation-cover behavior, no adaptive solver compiled into production.

**OPT02 completion note:** the 4,832-line hydrology monolith is now a five-unit immutable topology/cache service. The WATER14–WATER17 adaptive CPU solver, virtual-pipe state, cadence scheduler, adaptive tire mutation and presentation-cell gathers are removed. `.hhyd v15`, priority-flood/MFD topology, near/far reconstruction and triangle-space precipitation cover remain intact; `TireFleetBenchmark` now exercises Dynamic Surface Hydro.

---

# OPT03 — production Dynamic Surface GPU runtime and CPU-water fallback retirement

Rename the historical `DynamicSurfaceGpuLodPrototype` to a production name such as `DynamicSurfaceGpuRuntime` or `PrebakedSurfaceGpuRuntime`.

## Target split

```text
Graphics/DynamicSurface/
    DynamicSurfaceGpuRuntime.cpp/.hpp     # lifecycle/update coordinator
    DynamicSurfaceGpuResources.cpp        # atlas/buffer/program allocation
    DynamicSurfaceGpuResidency.cpp        # near/far tile residency/indirection
    DynamicSurfaceGpuTopology.cpp         # prebaked tile upload/streaming
    DynamicSurfaceGpuWater.cpp            # wetting/retained standing/runoff presentation state
    DynamicSurfaceGpuTireEvents.cpp       # localized tire-clearing/event compute
    DynamicSurfaceGpuOptionalStates.cpp   # temporary home for snow/mud until they warrant own owners
    DynamicSurfaceGpuTimers.cpp
    DynamicSurfaceGpuShaders.hpp
```

Do not duplicate state during the split; move existing statements and preserve order first.

## CPU Hydro retirement gate

After the GPU runtime has deterministic startup/failure policy and the tire model has the intended live water sample bridge:

- remove production `DynamicSurfaceHydrology.cpp/.hpp`;
- remove CPU Hydro page channels/statistics that exist only for fallback;
- keep thermal, Track/rubber and static support data under Dynamic Surface where still authoritative;
- if a scalar/reference CPU water implementation remains useful for tests, move it under `Tests/Reference/` so it cannot accidentally become a second runtime authority.

**Exit gate:** one production water authority from startup onward on the supported hardware baseline.

**OPT03 completion note:** the historical `DynamicSurfaceGpuLodPrototype` is now `DynamicSurfaceGpuRuntime` and its 1,637-line implementation is split into lifecycle, resources, residency, topology, exact geometry, dispatch, tire events, timers and shader ownership. The unrelated renderer-side `DynamicSurfaceGpuPagePool` duplicate was proven unused and retired. CPU `DynamicSurfaceHydrology` was audited but intentionally retained for now: it remains the explicit GPU-unavailable path and regression oracle, and tire physics still needs the planned live GPU water-sample bridge before the CPU implementation can be removed without reducing failure safety or test coverage.

---

# OPT04 — renderer decomposition and real GPU timing

## Sky/cloud

`SkyRenderer.cpp` currently owns atmosphere, Sun, Moon, stars, cloud raymarch, cloud lighting, distance LOD, upscale, temporal reprojection, cloud shadows and GPU target management.

Target:

```text
Graphics/Renderer/Sky/
    SkyRenderer.cpp/.hpp                  # facade/orchestration
    CelestialRenderer.cpp/.hpp            # atmosphere + Sun/Moon/stars
    VolumetricCloudRenderer.cpp/.hpp       # cloud resources/raymarch/upscale/TAA
    CloudShadowRenderer.cpp/.hpp
    SkyShaders.hpp
    VolumetricCloudShaders.hpp
    CloudShadowShaders.hpp
```

Embedded GLSL remains acceptable until Heritage has a real shader-asset pipeline, but shader source should not make orchestration files unreadable.

## Entity mesh

`EntityMeshRenderer.cpp` is **1,198 lines**, essentially on top of its existing `<1200` validator guard. `draw()` is the real issue.

Keep the public renderer, but move durable draw stages into private responsibility units:

```text
EntityMeshRenderer.cpp              # short frame coordinator
EntityMeshOpaquePass.cpp            # material/texture/range submission
EntityMeshTirePresentation.cpp      # tire deformation/failure uniform state
EntityMeshPostOpaque.cpp            # sky/cloud/wet-surface ordering
```

Do not create a giant generic `RenderContext`. Pass narrow existing data structures.

## Shader ownership

Split the 2,112-line `EntityMeshShaders.hpp` by pass/domain:

- material/PBR shader;
- surface/wetness integration;
- shadow shader;
- shared snippets only where genuinely shared.

## Surface presentation

Split `SurfacePresentationRenderer.cpp` into:

- `TireMarkRenderer`;
- `MarbleRenderer`;
- `MovingRubberRenderer` (or share with MarbleRenderer if the ownership remains cohesive);
- `SurfaceParticleRenderer`;
- small coordinator + dedicated shader headers.

**Exit gate:** byte/visual-equivalent output for fixed captures where practical; pass-level GPU timings available independently.

---

# OPT05 — rubber and surface-state ownership

Split `TrackRubberState.cpp` around its already-visible responsibilities:

```text
Physics/Surfaces/Rubber/
    TrackRubberState.cpp/.hpp       # facade, description, high-level API
    TrackRubberStorage.cpp          # addressing, chunk/LRU, snapshot/restore
    TrackRubberContact.cpp          # deposit/pickup/transfer/wake
    TrackRubberTransient.cpp        # airborne/mobile packet integration
    TrackRubberPresentation.cpp     # bounded presentation extraction only
    TrackRubberInternal.hpp
```

Split `SurfacePresentation.cpp` if the event/state ownership remains large:

- tire-mark history/state;
- particles/transient presentation events;
- audio-contact event accumulation.

Split `DynamicSurfaceBake.cpp` into bake construction and cache IO. Serialization/versioning should not be mixed through the geometry bake algorithm.

**Exit gate:** rubber mass/piece conservation tests unchanged, chunk persistence unchanged, no presentation split alters authoritative rubber state.

---

# OPT06 — runtime, bindings and core implementation boundaries

## `LuaModuleRuntime.cpp`

Target owners:

- Lua state lifecycle;
- entry/include/reload loader;
- sandbox + safety smoke tests;
- protected-call/callback helpers;
- script file watch/hot reload;
- scene transition queue;
- argument/handle decoding helpers.

`LuaModuleRuntime.cpp` should become lifecycle orchestration rather than every runtime utility.

## `LuaUiBindings.cpp`

It is only ~27 lines below the current 1,200-line dumping-ground guard. Split before the next widget lands:

```text
LuaUiLayoutBindings.cpp
LuaUiWidgetBindings.cpp
LuaUiImageBindings.cpp
LuaUiPlotBindings.cpp
LuaUiBindingRegistration.cpp
```

## `HeritageEngine.cpp`

`HeritageEngine::run()` still owns too much startup/settings/service/frame-loop work. Continue the existing runtime ownership direction: startup/settings, frame stepping, simulation, rendering and hotkeys should live in the already-established runtime owners. The engine shell should coordinate lifetime and error propagation.

## `VehicleSystem.hpp`

Move large public data contracts into stable domain headers (handles, wheel/tire state, chassis/suspension descriptions, powertrain state). Leave `VehicleSystem.hpp` as the actual system interface. Avoid one universal `VehicleTypes.hpp` dumping ground; use domain names.

## `RigidBodySystem.cpp`

Lower priority than water/rendering. If split, preserve one class and divide only clear responsibilities:

- properties/lifetime;
- force/impulse API;
- integration/interpolation;
- private math/record helpers.

No physics equation/order changes during the physical split.

---

# OPT07 — Racing United Lua: reduce global coupling before reducing line count

Lua is not suffering from giant files as severely as C++, but it has a more dangerous issue: shared global state.

Current examples:

- `Runtime/State.lua`: roughly **107 top-level global assignments**;
- `Vehicles/State.lua`: roughly **69 top-level global assignments**;
- many subsystem files export global functions directly into the shared environment.

## Stage 1 — namespace without changing loader semantics

Keep `Script.Include` but establish one first-party namespace:

```lua
RU = RU or {}
RU.Runtime = RU.Runtime or {}
RU.Vehicle = RU.Vehicle or {}
RU.UI = RU.UI or {}
```

Move mutable state behind `RU.Runtime.State`, `RU.Vehicle.State`, etc. Move callable APIs behind narrow subsystem tables (`RU.Vehicle.Fitment`, `RU.Vehicle.Visual`, `RU.Vehicle.Suspension`).

Temporary compatibility aliases may bridge old global names during migration, then be deleted once first-party callers are clean.

## Stage 2 — split only genuinely mixed Lua files

High-value candidates:

- `UI/Vehicle/Tires/LabPanel.lua` (593): split scenario/calibration/fleet/failure sections;
- `Runtime/PhysicsDemo.lua` (457): collision, suspension and CCD dev demos;
- `Vehicles/Fitment.lua` (367): persisted setup/default migration vs geometry/application;
- `Vehicles/Factory.lua` (355): spawning/lifecycle vs native definition construction;
- `VehicleDefinitionV2Builder.lua` (385): domain builders if it continues growing.

Keep coherent authored data files such as `PrototypeCar.lua` intact merely because they are long.

## Development-only content

Prototype/debug lab code should be explicitly grouped as development tooling and not loaded into a shipping/runtime module unless the current screen needs it. This makes future unreachable-code audits meaningful.

---

# OPT08 — regression, validator and documentation hygiene

## Tests

Split large regression translation units by current authority, not milestone history:

- tire core/MF;
- tire thermal/wear/failure;
- wet/winter/granular/deformable interaction;
- prebaked hydrology;
- Dynamic Surface thermal/track;
- rubber/presentation.

Historical tests for deleted algorithms should move to history/reference fixtures or disappear with the algorithm. A regression suite should defend current behavior, not prevent retired code from being removed.

## Validators

The BK Sun optimization already exposed a recurring problem: exact source-string checks can reject a valid optimization because a loop count or implementation detail changed.

Refactor validators toward:

- ownership/path/project membership;
- public contract existence;
- forbidden duplicate authority;
- generated API uniqueness;
- line-count/code-health bounds;
- regression behavior;
- cache/file-format invariants.

Avoid exact implementation strings for shader sample counts, loop syntax, local variable names, or historical algorithm internals unless those literals are themselves a required contract.

Remove checks whose only purpose is forcing placeholder/signpost source files to exist.

## Documentation

The repository currently has hundreds of milestone documents/reports. Keep authoritative current architecture docs easy to find and archive milestone tuning documents under history folders. Do not delete useful history, but do not make the root `Docs/` directory the historical timeline.

Also resolve duplicate ADR numbers going forward. Existing links can remain stable through an index rather than silently renaming old history.

---

# Code-health policy after the cleanup

These are review thresholds, not automatic failure rules:

- **C++ coordinator target:** 200–600 lines; review at 800; hard justification above 1,000.
- **C++ implementation target:** usually 300–900 lines; mandatory ownership review above 1,200.
- **Lua implementation target:** usually 80–350 lines; ownership review above 450; strong justification above 600.
- **Public headers:** split by domain once unrelated state/API families accumulate; do not use one giant generic `Types.hpp` as a hiding place.
- **No non-compiled fake `.cpp` scaffolds.** Future architecture lives in docs/manifests until implementation exists.
- **No new legacy compatibility path without an explicit removal condition/date/milestone.**
- **One runtime authority per physical state.** Reference/test implementations belong under tests, not production.

A large file is acceptable when its contents are cohesive and splitting would create worse coupling. A small file is not automatically good architecture.

---

# Recommended execution order

1. **OPT00** — profiling + code-health baseline.
2. **OPT01** — delete proven dead code and remove fake-source scaffolds/signposts.
3. **OPT02** — extract `.hhyd v15` prebaked authority and delete retired adaptive hydrology.
4. **OPT03** — rename/split the production GPU surface runtime; then retire CPU-water fallback when its replacement gates are satisfied.
5. **OPT04** — renderer/cloud/surface-presentation decomposition + per-pass GPU timers.
6. **OPT05** — TrackRubber / SurfacePresentation / DynamicSurfaceBake split.
7. **OPT06** — Lua runtime/UI bindings/engine shell/public type boundaries.
8. **OPT07** — first-party Lua namespace/state cleanup and selective file splits.
9. **OPT08** — regression/validator/docs consolidation and final architecture guards.

The most important rule is that each checkpoint remains independently buildable and testable. Do not mix a water-authority deletion, renderer move and Lua namespace migration in one ZIP.

---

# Expected outcome

After this program the engine should have:

- no large abandoned TireCarcass implementation sitting outside the build;
- no unreachable Cloud Lab code;
- no fake `.cpp` files whose only purpose is saying “future code goes here”;
- one clear production water authority rather than generations of fallback/retired solvers stacked together;
- the 4,832-line hydrology monolith reduced to current prebaked responsibilities only;
- production GPU Dynamic Surface code named and organized as production code;
- sky/cloud, surface presentation and rubber systems with visible subsystem ownership;
- fewer giant C++ entry points and headers;
- Lua state organized by namespace rather than hundreds of ambient globals;
- validators that protect behavior/architecture without fossilizing implementation details;
- GPU timing that makes future FPS work evidence-based rather than guesswork.

This is the roadmap I would use as the new cleanup authority for the current project tree.
