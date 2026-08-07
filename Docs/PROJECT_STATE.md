# Current Project State

## Milestone status

**User-confirmed baseline:** Step 29J.6B — Adaptive Three-Column Topology Grid
**Current candidate:** Step 29P — Authoritative Suspension Upright Pose

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

- Deterministic 240 Hz general physics world with bounded catch-up.
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
- A persistent Vehicle `WORKSHOP` tab with Windows module-asset selection, structural validation, honest current-solver capability reporting, supported live preview and module-private definition export.
- An adaptive three-column Workshop topology grid with a two-column fallback; longer actions retain the proven two-column layout without horizontal scrolling.
- Capacity-checked parked-vehicle sleep plus non-overshooting service/parking-brake wheel constraints.
- Ackermann steering, drivetrain, reverse/neutral/gears, differential modes.
- Dedicated native `Vehicles/TireModel.*` provider boundary.
- Independent native tire description per wheel/contact unit, with named Lua presets and exact per-wheel API readback.
- Advanced generalized sine/arctangent road-tire force curves with configurable shape/curvature, load-dependent stiffness, progressive post-peak behavior, combined-slip sharing, pneumatic-trail falloff, ABS, TCS, and handbrake.
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
- `Assets/Scenes/Player/PlayerScene.obj` creator environment slot with Blender-default OBJ axis conversion and hot reload.
- `PlayerScene_Collision.obj` exact triangle drive-surface query bridge for suspension/tire raycasts; `SPAWN_PLAYER` may live in either scene OBJ and is snapped to the actual terrain surface. Full rigid-body triangle-mesh contact remains later work.
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

## Immediate roadmap

1. Add suspension-only Workshop controls and gizmos for wheel centers, steering axes, linkage anchors, travel and motion ratio.
2. Implement MacPherson and double-wishbone hardpoint providers, then trailing-arm, live-axle, leaf-spring, pushrod/pullrod, kart-flex and motorcycle layouts.
3. Derive track change, caster, scrub, jacking, linkage loads and dynamic motion ratio from those providers.
4. Add anti-roll bars, third/heave springs and cross-linked/hydropneumatic systems.
5. Add camber thrust and motorcycle-profile tire behavior to the appropriate tire providers.
6. Use the Vehicle Dynamics Laboratory to establish measured suspension baselines and damper velocity/energy histograms.
7. Add damper oil/gas thermal state and physically derived fade/cavitation.
8. Add wear and damage only after the healthy load, temperature, travel-impact and geometry histories are authoritative.
9. Establish measured mass, wheel/tire data and suspension geometry for the first reference vehicle.

## Recovery procedure for a new conversation or contributor

1. Obtain the newest complete project ZIP, not only an old base archive.
2. Read this file and `AI_WORKFLOW.md`.
3. Run `Tools/GenerateLuaApiManifest.ps1` and inspect `Build/Reports/LuaAPI.md`.
4. Run `Tools/ValidateProject.ps1`.
5. Inspect the exact source files involved before proposing code.
6. Build through the milestone-specific command and record the resulting build identity.
