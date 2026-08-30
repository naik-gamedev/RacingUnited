# Heritage Studio

`HeritageStudio.exe` is the standalone authoring application for Heritage Engine and Racing United content.
It is deliberately separate from `HeritageEngine.exe` / the Racing United runtime. Opening an audio tool does not boot weather, clouds, hydrology, traffic or race simulation.

## Unified workspaces

- **Scene** — hierarchy / viewport / inspector shell and home for transforms, collisions and placement.
- **Race** — timing gates, sectors, grids, pits, track limits, AI lines, recovery and replay cameras.
- **Traffic** — explicit lane/road graph, junction connections, priority/rules, speed limits, traffic spawns and parking.
- **Gameplay** — circuit/street events, world activities, services, meet spots, police/speed-enforcement anchors and free-roam destinations.
- **Weather** — scene latitude/longitude/elevation and future opt-in sky/weather preview.
- **Vehicle** — vehicle-definition, wheel/suspension/tire/drivetrain/camera/acoustic authoring home.
- **Audio** — working Engine Sound Capture Laboratory.
- **Assets** — Racing United content browser and source-path workflow.

Scene, Race, Traffic, Gameplay, Weather and Vehicle now persist explicit Studio authoring assets. Selected data is also compiled/published into module-owned runtime data where a runtime bridge exists; Studio remains a separate authoring process rather than booting the game just to edit content.


## STUDIO03 Vehicle Character Designer

STUDIO03 corrects an important authoring boundary: the Audio Assistant removes repetitive Engine-Simulator cleanup work, but it must not collapse every vehicle toward one generic sound. The new **Vehicle Character Designer** is a persistent per-vehicle acoustic-DNA layer.

It adds directly editable character controls for:

- combustion punch and mechanical/metallic character;
- intake level, throat/growl, resonance and airbox damping;
- exhaust muffling, body, rasp, resonator/drone and tailpipe brightness;
- cabin trim absorption, firewall isolation, glass/body leakage, LF intrusion, cabin boom and window-open preview.

Component archetypes are starting points rather than locked presets:

- Intake: **Stock Airbox / Open Filter / ITB-Short**;
- Exhaust: **Stock Muffled / Sport Cat-back / Open-Race**;
- Cabin: **Light Hatch / GT-Insulated / Stripped-Race**.

Every underlying value remains editable and is serialized into `.hacoustic`. The same Engine Simulator source can therefore be shaped into genuinely different vehicle installations instead of merely receiving a universal cleanup preset.

The window-open audition is implemented as a frequency-dependent exterior acoustic leak toward the already-shaped source, not a simple volume boost. In Racing United the equivalent behavior is intended to be driven continuously at runtime from window/door/body state.

## STUDIO02 Engine Sound Assistant

STUDIO02 expands the Audio workspace specifically to reduce manual sound-design work.

### Raw capture analysis

Every completed loopback capture is analyzed offline without changing the raw WAV. Studio reports:

- peak and RMS dBFS;
- crest factor;
- short-term level stability;
- low/body/mid/presence/air spectral balance;
- a heuristic Engine-Simulator harshness score;
- dominant upper-mid/presence frequency;
- compact raw-envelope and log-frequency plots.

The analyzer is intentionally dependency-free and lightweight. It is an authoring assistant, not a mastering-grade measurement package.

### One-click profiles

The Audio Assistant exposes deterministic non-destructive starting points:

- **Engine-Sim Cleanup**;
- **Peugeot 206 RC Stock**;
- **Warm Road Car**;
- **Sport Exhaust**;
- **Neutral reset**.

`AUTO PEUGEOT 206 RC STOCK` starts from the Peugeot profile and adapts input headroom, presence-cut frequency/strength, top-end damping, pulse softening, body compensation and mild saturation from the current raw capture. `AUTO CLEAN ENGINE-SIM RAW` applies the same analysis to the more neutral cleanup seed.

Auto Tune does not claim to identify the real car from audio alone. It removes obvious source problems and produces a sane starting point that can then be auditioned or manually refined.

### Capture quality and bank resume

The 53-cell Peugeot EW10J4S capture bank now includes:

- existing-file completion progress;
- **Jump to first missing**;
- optional skipping of already-captured cells;
- automatic level/stability quality gate;
- auto-advance pausing when a capture is too quiet, too close to clipping, or too unstable;
- safe recapture/overwrite of an existing target.

This makes an interrupted authoring session resumable without manually tracking which RPM/load cells are finished.

## Engine Sound Capture Laboratory

Studio captures Engine Simulator Community Edition through Windows WASAPI loopback and stores untouched 48 kHz stereo float WAV files under:

`UserData/Modules/RacingUnited/EngineSoundLab/`

The Audio workspace provides:

- calibration capture;
- guided 53-cell Peugeot 206 RC EW10J4S RPM/throttle bank;
- automatic bank naming and manifest generation;
- non-destructive source-character DSP;
- Raw / Engine Bay / Rear-Exhaust / Driver-Cabin A/B audition;
- saved `.hacoustic` acoustic profiles;
- STUDIO02 Audio Assistant analysis / auto-tune / presets / bank quality workflow.

The advanced source-character controls include filtering, body resonance, presence/harshness removal, pulse softening, saturation, mechanical/intake character, exhaust muffling/resonance, cabin transmission, LF leakage, occlusion and reverb preview.

Listener-dependent behavior is intended to stay dynamic in the game: cabin/window state, distance, Doppler, environmental occlusion and shared reverb buses. Large grids will use audio LOD rather than full DSP for every distant vehicle.

## Build

Run:

`Tools/01_BuildAndRunHeritageStudio.cmd`

Output:

`Build/Studio/Release/HeritageStudio.exe`


## STUDIO05 — Core authoring foundation
Heritage Studio now persists scene, race, traffic, weather and vehicle authoring data in addition to the Engine Sound Lab. The Audio UI uses persistent descriptions/tooltips and Studio headings can load `Assets/Fonts/Orbitron-SemiBold.ttf`. See `Docs/STUDIO05_CORE_AUTHORING_FOUNDATION.md`.

## STUDIO06 Interactive 3D Authoring

Scene, Race and Traffic now share a real spatial editing camera inside Heritage Studio. The authoring viewport supports orbit/pan/zoom navigation, grid snapping, direct marker picking, XYZ move gizmos and direct placement on the ground plane.

Scene placement covers player/vehicle spawns, audio zones and triggers. Race authoring visualizes ordered AI race/wet lines and supports direct checkpoint/grid/pit/recovery placement. Traffic authoring visualizes lane nodes and headings and supports direct lane/intersection/light/parking/spawn/destination placement.

This is the spatial authoring layer, not a Racing United runtime boot. Full GLB/PBR scene rendering inside the Studio viewport is intentionally a later renderer-integration milestone; the editor data is already spatially authorable without starting weather, hydrology, traffic simulation or the game.

## STUDIO07 — Blender 5.2 Authoring UX

Heritage Studio deliberately follows the current Blender 5.2 LTS interaction model because Blender is the expected community 3D-authoring companion for Racing United content.

The Scene Layout now follows the familiar large 3D Viewport + Outliner + Properties composition, while a bottom Game Authoring strip takes the place of a traditional animation timeline for racing/free-roam authoring state. Workspaces are arranged across the top and can be cycled with Ctrl+PageUp / Ctrl+PageDown.

Viewport conventions now include:

- MMB orbit;
- Shift+MMB pan;
- Ctrl+MMB or wheel zoom/dolly;
- Numpad 1/3/7 front/right/top orthographic views;
- Numpad 5 perspective/orthographic toggle;
- Numpad period (and legacy F) frame selected;
- Home frame all;
- G/R/S modal move/rotate/scale for Scene objects;
- X/Y/Z transform constraints;
- Shift precision and Ctrl snapping while transforming;
- LMB/Enter confirm and RMB/Escape cancel;
- Shift+A game-object Add menu;
- Shift+D duplicate;
- X/Delete removal;
- T tool shelf and N Item sidebar toggles.

Heritage-specific additions remain first-class: player and vehicle spawns, triggers, audio/weather zones, race checkpoints/pits/grids/AI lines and free-roam traffic network authoring.

## STUDIO08 — GLB/PBR World Viewport

Scene, Race and Traffic authoring now render the newest `Scene_*.glb` from Racing United `Assets/Scenes` directly inside the Blender-style Studio viewport. Studio reuses Heritage's GLB parser, mesh uploader and texture cache and previews base-color, normal, roughness, metallic, AO, emissive and opacity material data without booting Racing United runtime systems.

Direct placement now raycasts against the visible scene geometry before falling back to the legacy Y=0 plane, so checkpoints, grids, pits, spawns and traffic nodes can be authored on banked roads, hills and bridges. The Scene toolbar provides visibility, wireframe, exposure, latest-scene discovery and GLB reload controls, while `Home` frame-all includes the imported world bounds.

See `Docs/STUDIO08_GLTF_PBR_WORLD_VIEWPORT.md`.


## STUDIO09 — Runtime Scene Spawn Bridge

Scene `Vehicle Spawn` authoring can publish directly into the module's real `entry_scene` while retaining the Studio safety copy and an automatic pre-overwrite runtime backup. Racing United uses the Studio-authored position and rotation when present and preserves the GLB-authored spawn fallback when absent. Orbitron SemiBold is the default Studio font.

See `Docs/STUDIO09_RUNTIME_SCENE_SPAWN_BRIDGE.md`.

## STUDIO10 — Gameplay / World / Road Graph

Race session settings are persistent HRACE v2 data and missing grid slots can be generated without overwriting existing grid authoring. Traffic is now an explicit HROAD v2 node+link graph with automatic and manual link authoring.

The new Gameplay workspace authors motorsport and clandestine events plus reusable free-roam world points such as garages, dealerships, fuel/repair, meets, safehouses, police, speed enforcement and fast travel. `gameplay.hgame` is compiled together with race/road data into `Scripts/Generated/StudioGameplay.lua`, which Racing United loads through a runtime query facade. Cross-layer validation detects broken references without silently editing the project.

See `Docs/STUDIO10_GAMEPLAY_WORLD_EVENT_AUTHORING.md` and `Docs/HERITAGE_STUDIO_GAME_PRODUCTION_ROADMAP.md`.

## STUDIO11 — Venue Routes / Timing / Race Director

Race authoring now treats a venue as more than a marker list. HRACE v3 adds named cubic-Bezier route splines for the main circuit, pit lane, Safety Car/formation paths and alternate layouts while preserving every existing marker-based timing, track-limit and AI-line tool.

Each route node can carry independent left/right legal-driving corridor widths, target speed, banking and overtaking hints. The 3D viewport draws both spline centerlines and their red/green corridor edges, and route nodes can be traced rapidly by clicking the imported scene surface. Timing/checkpoint objects now expose directional gate width/height, with timing-loop, speed-trap, Safety Car and formation-line marker types added.

The Race workspace is split into Markers + Grid, Routes + Corridors, Layouts, Sessions and Race Control. Layouts bind reusable race/pit routes to a Start/Finish marker; session chains persist practice, qualifying, warm-up, race, time-attack and test sessions; race-control data persists flag-system capability, pit-window and track-limit rules plus Safety-Car/restart references. Marshal/recovery/tow/medical/fire/race-control/Safety-Car/timing support points are spatially authorable.

Grid generation supports staggered 2-wide, 2-wide, 3-wide, single-file and endurance-angled templates with editable spacing while preserving existing slots. HGAME v2 lets gameplay events reference a named venue layout. Runtime StudioGameplay v2 exposes layout, route-node, session-chain, race-control and support-point queries to Racing United.

See `Docs/STUDIO11_VENUE_ROUTES_RACE_DIRECTOR.md` and `Docs/HERITAGE_STUDIO_GAME_PRODUCTION_ROADMAP.md`.

## STUDIO12 — Free-Roam Road Construction / Navigation

Traffic authoring now has a high-level HROAD v3 road-construction layer above the existing HROAD node/link graph. Road splines author multi-lane carriageways, road class, speed, shoulders, medians, sidewalks, parking flags, traffic density and ordered Bezier control nodes directly on the imported scene surface.

Junction authoring adds right-of-way policy, legal turn connectors, signal phases and pedestrian-crossing metadata. Parking strips, traffic population mixes and navigation-build parameters live in the same road-world asset. A non-destructive lane compiler synchronizes deterministic `AUTO_LANE_*` graph nodes/links from road splines while preserving hand-authored graph overrides.

Racing United receives StudioGameplay v3 road, lane, intersection, turn, signal, parking, population and navigation data plus runtime query helpers.

See `Docs/STUDIO12_FREE_ROAM_ROAD_CONSTRUCTION.md` and `Docs/HERITAGE_STUDIO_GAME_PRODUCTION_ROADMAP.md`.


## STUDIO13 — Operational Traffic / Navigation

HROAD v4 makes the compiled lane graph operational rather than purely topological. Graph edges now carry movement semantics and route cost, generated lane nodes identify road/lane/direction, and road compilation creates adjacent-lane change movements without overwriting manual graph overrides.

The Traffic workspace adds Operations / Routing controls for left/right driving policy, car-following and lane-change behavior, traffic streaming sectors, per-intersection fixed/actuated/adaptive signal controllers, conflict-group turn reservations and scheduled road restrictions/closures. Racing United loads `Runtime/TrafficOperations.lua` for restriction-aware route finding, live signal progression, dynamic closure overrides, intersection reservations, streaming-tier selection and emergency-yield decisions.

See `Docs/STUDIO13_OPERATIONAL_TRAFFIC_NAVIGATION.md`.

## STUDIO14 — Physical Traffic Agents

HROAD v5 adds reusable traffic-agent archetypes and the persistent simulation policy that sits above STUDIO13 routing/operations. Archetypes describe the spawned vehicle factory key, dimensions, population weight and driver character including following gap, reaction time, acceleration/braking character, lane-change aggression, courtesy, speed compliance, illegal-overtake tendency and parking skill. Existing HROAD v1-v4 assets remain loadable and receive safe default archetypes; live traffic-agent execution is disabled by default until enabled for a world.

The Traffic workspace adds **Agents / Simulation** authoring for population archetypes and simulation budgets. Racing United loads `Runtime/TrafficAgents.lua`, which owns persistent trips and streamed agents, IDM-style car-following, restriction-aware routing, lane-change/merge decisions, signal and stop-line behavior, intersection reservations, emergency yielding, weighted archetype spawning and debug telemetry. The vehicle representation is deliberately pluggable: the default backend can create inexpensive kinematic collision/debug proxies, while a later full Heritage vehicle factory can be attached without replacing the traffic decision/routing authority.
