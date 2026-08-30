# Heritage Studio — Racing United Game Production Strategy

The editor strategy is additive: keep every useful existing tool and progressively turn Heritage Studio into the authoring front-end for the complete Racing United simulation/game stack.

## STUDIO10 — Gameplay foundations — implemented

Persistent race rules, explicit road links, motorsport/clandestine event definitions, free-roam world points, validation and runtime Lua publication.

## STUDIO11 — Venue routes + race director foundations — implemented

- HRACE v3 named venue routes: main circuit, pit lane, safety-car, formation, alternate, sprint, hillclimb and drag;
- ordered cubic Bezier route nodes with editable/automatic tangent handles;
- per-node left/right legal-driving corridor widths, target speed, banking and overtaking hints;
- route/corridor 3D visualization plus rapid surface-click route tracing;
- directional checkpoint/timing gates with authored width and height;
- Start/Finish, checkpoint, sector, timing-loop and speed-trap timing objects;
- safety-car and formation lines;
- configurable staggered/two-wide/three-wide/single-file/endurance grid generators;
- named venue layouts referencing race route, pit route and Start/Finish;
- reverse-layout duplication while retaining reverse-running permission validation;
- persistent practice/qualifying/warm-up/race/time-attack/test session chains;
- race-control configuration for local yellow, FCY, VSC, Safety Car, red flag, blue flags, pit-window rules and track-limit warning thresholds;
- marshal, recovery, tow, medical, fire, race-control, Safety Car standby and timing-equipment support points;
- gameplay events can bind to a named venue layout;
- HGAME v2 adds layout references while HGAME v1 remains loadable;
- runtime Lua facade can resolve layouts, ordered route nodes, session chains, race control and support infrastructure.

## STUDIO12 — Free-roam road construction — implemented

- HROAD v3 road splines layered above the existing node/link graph;
- motorway/arterial/collector/local/residential/service/mountain/gravel/dirt classes;
- one-way/two-way multi-lane carriageways, lane width, shoulders, median, sidewalk and curb-parking metadata;
- surface-click spline tracing, control-node width scaling/banking and automatic/manual Bezier tangents;
- generated lane center offsets and deterministic `AUTO_LANE_*` graph synchronization;
- authored intersections with priority/yield/stop/signalized/roundabout/uncontrolled right-of-way;
- explicit legal turn connectors and U-turn/yield/speed metadata;
- traffic-light phases with green/yellow/all-red timing and connector groups;
- parking-strip generators;
- traffic density/time/population mix and active-vehicle budgets;
- navigation slope/turn-radius/lane-change/junction/merge parameters;
- runtime road/lane/intersection/turn/signal/parking query APIs.

## STUDIO13 — Operational traffic/navigation — implemented

- HROAD v4 semantic graph movements for travel, lane change, merge, junction turns, parking access and spawn access;
- lane metadata on generated graph nodes and automatic adjacent-lane change edges;
- route-cost multipliers and enabled/generated state per graph link;
- left/right driving-side, following-gap, acceleration/braking and lane-change policy;
- full/simplified/dormant traffic streaming radii, sector sizing and spawn/despawn budgets;
- fixed-time, actuated and adaptive intersection-controller metadata with emergency preemption;
- conflict-group reservations and occupancy durations on turn connectors;
- scheduled closures/incidents/construction/event closures/tolls/low-emission/mass/height restrictions;
- RacingTraffic runtime with restriction-aware routing, signal progression, dynamic restriction overrides, reservations, streaming-tier selection and emergency-yield decisions.

## STUDIO14 — Physical traffic agents — implemented

- HROAD v5 reusable traffic-agent archetypes layered above HROAD v4 operations;
- per-archetype vehicle class/preset, dimensions, spawn weighting and driver-character parameters;
- persistent physical-agent simulation policy with full/simplified update budgets and spawn/perception/stuck/despawn controls;
- pluggable traffic-vehicle factory so the AI authority is independent of kinematic debug proxies versus later full Heritage vehicle bodies;
- IDM-style car-following using authored time gap, minimum gap, acceleration and braking character;
- semantic lane-change/merge selection using the HROAD v4 movement graph and archetype aggression/courtesy;
- restriction-aware trip routing, signal/stop-line response and conflict-group intersection reservations;
- emergency-yield response;
- full/simplified/dormant agent streaming with proxy creation/removal independent of persistent agent state;
- weighted traffic archetype spawning and per-agent/runtime telemetry;
- live traffic remains disabled by default so upgrading an existing world does not unexpectedly populate it.

## STUDIO15 — Traffic integration and debugging — implemented

- HROAD v6 spawn/despawn portals with graph anchors, mode, weighting, active-hour windows, player-distance gates, class filters and per-portal concurrency caps;
- spatial traffic-density regions with density, speed, lane-change-aggression and parking multipliers;
- authored breakdown/collision/roadworks/police-stop/debris/flooding incidents with road/link/area influence, blockage, speed, route cost, response and clearing metadata;
- wet/heavy-rain/sub-zero/night/poor-visibility traffic response plus wet following/braking behavior and weather-aware lane-change gaps;
- persistent live traffic-debug policy;
- opt-in FULL-tier Heritage Vehicle backend using dynamic chassis bodies, suspension/wheel contacts, steering, drive/brake inputs, driver aids and an authored high-rate traffic-vehicle solver cadence;
- portal-driven population spawning and despawn destinations with backward-compatible random-lane fallback for older worlds;
- incident-aware route evaluation plus runtime incident reporting/clearing/aging;
- live queue/emergency demand feeding STUDIO13 actuated/adaptive signal controllers;
- Racing United TRAFFIC prototype tab with population/streaming/portal/incident/backend telemetry, per-agent route/wait/weather/density information, manual spawning and runtime-breakdown injection.

## Next: traffic behavior depth and world simulation preview

Keep the STUDIO12–15 stack and deepen behavior rather than replacing it: authoring-time live simulation preview inside Heritage Studio; multi-lane anticipation before exits/turns; MOBIL-style lane-change incentive/safety scoring; zipper-merge arbitration; roundabout gap acceptance; stop/yield creep and queue discharge; parking-space selection plus multi-phase pull-in/pull-out/reverse maneuvers; breakdown recovery/towing and incident responders; density demand by time/day/weather/world region; spawn/despawn portal flow counters and saturation visualization; traffic route heatmaps; agent route/intention/perception overlays; physical traffic vehicle preset/prefab binding and LOD/full-physics promotion diagnostics.

## Road / free-roam network — next depth

- lane-change and merge connectors between generated lane centerlines;
- shoulder/bus/HOV/service-lane restrictions and vehicle-class permissions;
- automatic junction connector geometry and conflict zones;
- runtime traffic-light controllers and adaptive signal timing;
- parking-lot circulation graphs and pull-out behavior;
- country/region driving-side, signage and speed-rule sets;
- navigation route-cost preview, closures, tolls and avoidance policies;
- streaming traffic sectors, spawn/despawn portals and density regions;
- incidents, blockage rerouting and emergency yielding.

## Motorsport event/race director

Implemented foundation: persistent practice/qualifying/warm-up/race/time-attack/test session chains plus venue-level yellow/FCY/VSC/Safety-Car/red-flag/blue-flag capability switches, pit-window fields, track-limit warning thresholds and Safety-Car/restart references. Next depth:

- timed races, lap races, endurance duration + finish-lap logic;
- class/category grids and multi-class scoring;
- rolling/standing starts, formation laps and restart procedures;
- yellow/local-yellow/FCY/VSC/safety-car/red-flag states;
- blue flags, black flags, drive-through/stop-go/time penalties;
- pit windows, mandatory stops, tire/fuel requirements;
- championships, points tables, calendars and weekend formats;
- AI skill/aggression/consistency fields by entrant;
- weather/time schedules per session.

## Clandestine / street-racing gameplay

- street circuit and point-to-point event staging;
- meet spots and spontaneous challenge locations;
- wagers / entry fees / rewards;
- reputation and heat/notoriety curves;
- police response tiers and pursuit enable regions;
- traffic-on race rules;
- roadblock/spike-strip/pursuit spawn authoring hooks where game design later requires them;
- escape / cooldown / safehouse zones;
- night/time/weather availability rules;
- opponent pools and vehicle-class restrictions.

## Free-roam world gameplay

- garages, safehouses and owned-property anchors;
- dealerships and vehicle inventories;
- fuel, repair, tire, tuning and car-wash services;
- parking, meets, landmarks and discoveries;
- fast travel and spawn selection;
- delivery/test-drive/cruise route activities;
- speed cameras, average-speed zones and speed traps;
- world regions with unlock/discovery metadata;
- economy/reward tables and save-game persistence hooks.

## Racing AI

- racing line spline with speed target, curvature and braking metadata;
- wet line / defensive line / overtake line variants;
- pit entry/exit and pit-box behavior;
- grid launch and formation behavior;
- track-limit awareness and recovery logic;
- per-corner aggression / passing hints only where needed;
- offline line-generation/optimization tools plus hand-edit override.

## Traffic AI

- lane-following graph compiler;
- intersection reservation and signal response;
- parking and pull-out behavior;
- vehicle class restrictions by road;
- density/time/weather profiles;
- incidents, blockage rerouting and emergency yielding;
- streaming-aware traffic population for very large free-roam maps.

## World / scene construction

- GLB node/material Outliner inspection;
- object/prefab placement and instancing;
- collision type/layer authoring;
- trigger/volume editor;
- LOD and HLOD authoring;
- occluders and streaming cells;
- terrain/road surface classification;
- vegetation scatter regions;
- decals, signs, barriers, cones and trackside prop painting;
- lighting/reflection/audio/weather volumes.

## Surface and live-track authoring

- material -> tire surface mapping;
- drainage/hydrology bake controls and visualization;
- puddle/runoff diagnostic overlays;
- rubbering/marbles parameters;
- racing-line dry/wet preview;
- surface temperature / grip region metadata;
- curb, grass, gravel, mud, snow and ice behaviors.

## Vehicle content editor

- GLB hierarchy / socket inspection;
- wheel/hub/brake/caliper binding;
- mass, CoM and inertia visualization;
- suspension hardpoints and kinematic plots;
- alignment, spring/damper/ARB setup;
- steering geometry and Ackermann;
- engine/motor, gearbox, clutch and differential authoring;
- brakes and driver aids;
- tire/compound assignment;
- fuel/battery systems;
- lights, instruments, animation and damage bindings;
- cockpit/chase/replay camera rigs;
- audio profile/bank linkage.

## Presentation and media

- replay-camera spline/trigger authoring;
- broadcast camera sets;
- replay timeline metadata;
- photo-mode spawn/constraints;
- event intro/outro cameras;
- UI/HUD layout authoring;
- minimap/radar generation from world/race/road data.

## Multiplayer / online world

- grid/server capacity metadata;
- replication relevance/streaming regions;
- pit/team slot mapping;
- session/server rulesets;
- join/spawn/spectator points;
- MMO/free-roam activity regions;
- authoritative race-control configuration.

## Production safety and automation

- dependency validation and broken-reference browser;
- one-click compile/publish of editor assets to runtime assets;
- source/runtime diff preview;
- automatic timestamped backups and restore browser;
- batch asset validation;
- world/race/traffic graph statistics;
- playtest-launch buttons that start the exact authored scene/event;
- profiling overlays for editor preview and compiled runtime content.


## STUDIO16 implemented — advanced traffic behavior and recovery

- HROAD v7 advanced traffic behavior policy with HROAD v1–v6 backward loading.
- zipper merge alternation and courtesy-gap negotiation;
- roundabout gap acceptance layered over intersection reservations;
- explicit stop-sign dwell and controlled yield creep;
- opportunistic overtaking plus return-to-driving-lane behavior;
- queue-discharge reaction spread instead of synchronized robotic launches;
- staged reverse parking and parking-exit checks;
- multi-stage stuck recovery: lane escape, reroute, reverse recovery and last-resort relocation;
- traffic-agent collision detection creates runtime collision incidents and rerouting pressure;
- emergency traffic can be dispatched toward incidents requesting response;
- per-agent decision, lane-change score/reason, merge wait, stop dwell, queue reaction, parking/recovery and incident-response telemetry.

Next traffic work should deepen physical interaction rather than replace this stack: lane-level intersection geometry, true vehicle-body collision aftermath, police-specific pursuit/pull-over logic, pedestrians/cyclists, public transport, road-event closures, and scalable spatial-neighbor acceleration structures for very dense populations.

## STUDIO17 — Police / Clandestine Free-Roam Gameplay (implemented)

STUDIO17 keeps police and underground gameplay in HGAME rather than folding it into HROAD. HROAD remains transportation/traffic infrastructure; HGAME now consumes that network for pursuit and clandestine gameplay.

Implemented authoring/runtime foundation:
- HGAME v3 with HGAME v1/v2 backward loading.
- Global police gameplay/pursuit policy (opt-in by default).
- Patrol/enforcement zones with schedules, response weighting, unit caps, speed tolerance and traffic-portal response anchors.
- Authored roadblock sites with heat thresholds, spike-strip/escape-gap policy and graph anchors.
- Escape/cooldown zones with search-time and heat-decay modifiers plus safehouse semantics.
- Clandestine meet locations with schedules, capacity, police risk, heat multiplier and optional event linkage.
- Surface placement, viewport markers and move gizmos for the new spatial gameplay data.
- Runtime heat/notoriety state machine: Idle, Pursuit, Search, Cooldown and Busted.
- Witnessing and automatic speed enforcement using compiled traffic graph limits plus existing Speed Camera/Speed Trap world points.
- Emergency traffic-agent police dispatch using the same semantic navigation/closures/signals as civilian traffic.
- Pursuit retargeting to the player's nearest compiled traffic graph node.
- Pursuit roadblocks feed the existing runtime incident system so civilian traffic can react/reroute.
- Search/escape-zone influence, bust proximity/hold logic and heat decay.
- Clandestine-event heat entry point for future event/session execution.
- Dedicated Racing United POLICE debug tab with deterministic heat/infraction/search/reset controls.

Next useful gameplay layers after this foundation:
- Full police vehicle archetype/fleet authoring and station garages.
- Pursuit tactics (rolling roadblocks, boxing, PIT policy, helicopter observation, intercept prediction).
- Police radio/dispatch event queue and district coverage balancing.
- Fines, citations, arrest/impound/recovery and economy consequences.
- Clandestine organizer/contact progression, invitations, reputation, entry stakes and meet population staging.
- Event start/finish execution, route closures vs live-traffic street racing, spectators and police raid behavior.
- Player/AI damage and traffic-collision reporting into police heat.
- Persistent notoriety by region and vehicle identity.

## STUDIO18 — Event Execution & Portal-Style Section Practice (implemented)

STUDIO18 connects the venue/session/gameplay authoring stack to an actual Racing United race-event runtime and adds a free-form section-practice loop that works on circuit or public-road content.

Implemented:
- HGAME v4 event-execution/practice policy with HGAME v1–v3 backward loading.
- HRACE v4 optional per-layout marker scope with HRACE v1–v3 backward loading; layoutId 0 remains Global/all-layouts.
- Runtime event lifecycle: staging, countdown, green/running, finish/results and session advancement.
- Circuit laps, point-to-point finishes, ordered checkpoints, sectors, timing loops and directional timing gates.
- Layout-scoped checkpoints, grids, pit gates and Start/Finish references so GP/club/reverse/street layouts can coexist in one scene without consuming one another's race objects.
- Race-control flag state: Green, Local Yellow, FCY, VSC, Safety Car, Red and Chequered.
- False-start, track-limit, controlled-speed and pit-speed penalty foundations plus drive-through state.
- Pit entry/exit, stop counting, mandatory-stop telemetry and persistent personal-best lap storage.
- Participant progress/standings interface so future Racing AI can register against the same event authority.
- Clandestine event lifecycle integration with STUDIO17 police heat; being Busted can DNF an underground event.
- Dedicated Racing United RACE runtime/debug panel.
- Portal-style practice looping: F5 captures start state, F6 captures end gate and enables looping, F3 restarts immediately, F4 pauses/resumes.
- Practice-loop snapshots use global coordinates and restore rigid-body rotation, exact linear velocity vector, optional angular velocity and gear so a corner-entry state can be repeated instead of merely teleporting at a scalar speed.
- Loop attempts track current/last/best section time and automatically restart after the captured end gate when enabled.

Next motorsport/gameplay depth should build on this authority rather than create parallel race systems: entrant rosters and Racing AI spawning, class/multi-class scoring, qualifying-to-grid transfer, rolling/formation starts, endurance timed-finish logic, pit-service/tire/fuel rules, stewarding/penalty serving, championship calendars/points, event rewards/progression, spectators/meet staging, street-race live-traffic/closure policies, and replay/results persistence.


## STUDIO19 — Competitors, Grids & Complete Motorsport Weekends (implemented)

STUDIO19 turns STUDIO18 event execution into a competition structure that can exist before final vehicle/opponent assets are available. Venue/session rules remain in HRACE while entrants/classes/championships remain in HGAME.

- HRACE v5 adds timed/endurance race format, time-plus-one-lap, stint limits, refuelling/tire-service policy, minimum pit-service time, classification percentage and grid-source policy.
- HGAME v5 adds reusable motorsport classes, entrant/team/vehicle definitions, AI driving traits, championships, calendar rounds and points schemes.
- Gameplay gains COMPETITORS / SERIES authoring rather than hiding entrant data inside events.
- RacingMotorsport builds event grids from event order, previous-session results, qualifying, championship order or reverse-grid rules.
- Qualifying order can be generated for logical AI from authored pace/consistency traits even before final opponent assets exist.
- Nearby competitors can use replaceable kinematic race proxies following the authored venue route; the timing/grid/championship layer is independent of representation.
- Formation laps and rolling-start phases are consumed by RacingEvents. Timed races transition to chequered-at-line or time-plus-one-lap behavior.
- Mandatory pit stops require the authored minimum stationary service time; drive-through penalties do not incorrectly count as service stops.
- Championship results use classification percentage, points multipliers and persistent points storage.
- Clandestine entrants use the same competition stack while remaining event/filter aware, so underground racing does not need a duplicate opponent system.
- STUDIO18 Portal-style practice looping and every earlier authoring/runtime layer remain intact.

## STUDIO20 — Racing AI Intelligence & Strategy (implemented)

STUDIO20 adds a representation-independent race-intelligence layer above STUDIO19's entrant/grid/weekend authority. It deliberately does not replace routes, race timing, championships, event execution, or the future full vehicle controller.

- HGAME v6 adds persistent global Racing AI tuning plus per-entrant racecraft/awareness/defending/tire/fuel/strategy/mistake/reaction/line-bias traits; HGAME v1–v5 remain readable.
- Racing AI consumes authored route target speeds, AI Race Line markers and AI Wet Line markers, with speed-dependent lookahead and braking-lookahead behavior.
- Opponent awareness supports slipstream decisions, overtaking, defending, blue-flag yielding and multi-class negotiation.
- Current lightweight race proxies visibly consume dry/wet line selection and lateral overtake/defend/yield offsets.
- A stable `GetControlIntent()` interface separates intelligence from representation so the future full Heritage race-car physics controller can consume the same decision authority.
- Continuous fuel/tire/stint state supports pit calls for fuel windows, tire wear, weather crossover, maximum stint and mandatory stops; AI pit service is integrated with MotorsportWeekend.
- Race-control states alter AI decisions for FCY/VSC/Safety Car/Red conditions.
- Deterministic personality-driven mistakes produce recoverable pace/line errors suitable for replayable testing.
- Racing United's RACE panel exposes live decision/reason, current/target speed, braking demand, lateral line offset, wet-line blend, nearby opponents, overtake/defend/yield/slipstream state, fuel range, tire state, pit request and mistake telemetry.

Next Racing AI work should connect this intent layer to full Heritage race-car dynamics and deepen interaction: steering/throttle/brake control of real tire/suspension cars, race-start launch behavior, side-by-side collision avoidance, track-limit-aware passing, pit-lane path following, tire compound inventory and fuel loads, damage/mechanical-state strategy, team strategy, dynamic weather prediction, caution-wave-by behavior, and learning/reference-lap tooling without creating a parallel AI stack.

## STUDIO21 — Full-Physics Racing AI & Racecraft (implemented)

STUDIO21 connects Racing AI control intent to native Heritage Vehicle dynamics for the physical competitor budget while retaining scalable logical competitors. Physical chassis speed and route projection feed timing/classification; tire slip/contact data feeds grip-aware control; spatial/track-limit margins constrain racecraft; collision-derived mechanical health feeds pit/DNF strategy; formation/rolling start control and short-horizon wetness forecasting are authorable and debuggable. Full-physics competitors can also follow the authored pit spline, obey the AI pit-speed cap, stop for real service timing, continue to pit exit and rejoin the main route; logical/off-budget competitors retain the cheap service path.

Next racing-AI depth should build on this authority with asset-backed race-vehicle factories, authored pit boxes/team garages and service crews, component-level damage, tire temperature/pressure strategy, spatial collision prediction, steward-aware attacking/defending, race-start launch optimization, dynamic line learning and replay/debug capture.

## STUDIO22 — Collider-Aware Close Racecraft & Stewarding (implemented)

STUDIO22 makes the physics collision representation the primary Racing AI chassis-size authority and adds a separate close-quarters racecraft/stewarding layer above STUDIO20 intelligence and STUDIO21 native vehicle control.

- HGAME v8 persists close-racing, stewarding, thermal/fuel and component-strategy policy while retaining HGAME v1–v7 loading.
- `Physics.GetBodyCollisionBounds` returns aggregate body-local bounds across solid compound colliders and excludes trigger/sensor volumes.
- Racing AI physical footprints therefore come from the actual collision body rather than a second manually maintained AI-width model; logical dimensions remain fallback only.
- Swept-envelope prediction uses route progress, velocity, lateral position and both vehicles' collider footprints for pre-contact collision avoidance.
- Side-by-side cornering, divebomb commit/abort judgement, switchback/crossover behavior and early multiclass pass planning are layered onto existing STUDIO20 race decisions.
- Defensive moves are counted per pressured straight; authored blocking rules can create real participant time penalties.
- Unsafe pit releases are judged against approaching competitors and can create real participant time penalties.
- Native Racing AI feeds tire temperature/thermal grip, fuel mass and aero/suspension/powertrain condition back into strategy and debugging.
- The RACE prototype panel exposes live collider footprint, swept margins, divebomb/defensive state, steward penalties, thermal state, vehicle/fuel mass and component health.

Next racecraft depth should continue from this physical authority: curved swept hulls rather than scalar route envelopes, wheel-to-wheel contact classification, incident blame/steward review, drafting/aero wake physics, tire-pressure/carcass strategy, component-specific failure modes, team orders, race-start launch/clutch optimization, pit-lane queueing/unsafe-release arbitration across multiple team cars, and replayable steward evidence capture.

## STUDIO23 — Solver-Contact Steward Evidence (implemented)

STUDIO23 connects the racecraft/stewarding layer to what the native collision solver actually resolved, instead of judging incidents from proximity alone.

- HGAME v9 persists physical-contact evidence thresholds, severe-contact thresholds, steward penalties, duplicate-contact cooldown and bounded evidence retention while HGAME v1–v8 remain readable.
- `Physics.GetBodyContact(body,index)` exposes body/collider identity, contact point, requested-body-oriented normal, penetration, normal/tangent solver impulse, trigger state and warm-start state from the most recently completed fixed step.
- Full-physics Racing AI samples the strongest solid solver contact and derives relative closing speed, chassis contact zone and floating-origin-safe global evidence position.
- MotorsportWeekend resolves contacted body handles back to physical race entrants and feeds the evidence into the existing RacingAIRacecraft steward layer.
- Conservative incident review distinguishes rear-end, side-to-side, front/crossing and static-world contacts; ambiguous cases remain racing incidents rather than manufacturing fault.
- Avoidable/severe contact penalties use `RacingEvents.AddParticipantPenalty`, preserving one timing authority.
- Contact cooldown is applied to both involved competitors so a persistent manifold or reciprocal body query cannot spam penalties.
- A bounded newest-first steward evidence ledger is reset per event and exposed through `RacingAIRacecraft.GetIncidentLog` for future replay/post-race review.
- The RACE debug panel exposes raw solver contact evidence plus latest/recent steward verdicts.

Next racecraft depth should continue from the same physical evidence: multi-point/wheel-to-wheel contact classification, curved swept hulls, aero wake/drafting forces, race-start clutch/launch optimization, tire-pressure/carcass strategy, pit-lane team queue arbitration and replay-timeline persistence of steward evidence.

## STUDIO24 — Incident Replay & Steward Ghost Review (implemented)

STUDIO24 turns STUDIO23 physical-contact evidence into reviewable spatial history without attempting an unbounded full-endurance replay in Lua.

- HGAME v10 adds a separate motorsport replay configuration while HGAME v1–v9 keep loading with safe defaults.
- A bounded 12 Hz rolling pre-impact buffer captures floating-origin-safe player and competitor state.
- Accepted STUDIO23 incidents seal authored pre/post-impact clips linked to the existing incident id, classification and verdict.
- Native, kinematic and logical competitors feed one representation-independent replay snapshot API.
- Clip count and per-frame participant count are explicitly capped for large grids.
- The RACE panel can browse clips, scrub/step the timeline, jump to impact and play/pause review.
- Non-physical `ReplayGhost` vehicles interpolate recorded global poses and cannot influence physics, AI or timing.
- Replay capture stops at event end while clips remain reviewable until the prototype scene is left or a new event starts.
- Generated StudioGameplay schema advances to v15.

Next replay/race-control depth should move large/full-session capture into a compact native binary stream, add flag/lap/pit timeline bookmarks, wheel/contact sub-classification, telemetry graphs and server-authoritative multiplayer replay capture. Racing-AI depth can continue in parallel with curved swept hulls, real aero wake/drafting forces, launch/clutch optimization, tire-pressure/carcass strategy and team pit-lane queue arbitration.

## STUDIO25 — Replay / Broadcast Camera Director + Dark Studio Chrome (implemented)

STUDIO25 turns STUDIO24 incident clips into camera-directed review and adds the native window polish requested for Heritage Studio.

- Heritage Studio requests Windows immersive dark non-client rendering, with a black caption/title bar, light caption text/icons and near-black border where the OS supports those DWM attributes.
- A native detached world-camera API accepts global FP64 position plus pitch/yaw/roll and converts through the current floating origin at render time.
- Detached/world camera authority no longer requires a live player chassis, making it reusable for post-race replay, spectators and future network observers.
- Lua gains `Camera.SetWorldPose`, `Camera.GetWorldPose`, `Camera.SetWorldViewActive` and `Camera.IsWorldViewActive`.
- HGAME v11 adds broadcast-director enable/auto-camera policy plus incident distance/height, trackside lead, helicopter height and smoothing; HGAME v1-v10 remain readable.
- STUDIO24 replay review gains Incident, Trackside, Chase, Helicopter and Off camera modes driven from recorded global car poses and the STUDIO23 global incident position.
- Camera interpolation uses shortest-path angles and authored exponential smoothing; selecting clips resets smoothing history.
- Review disable, scene clear, `CAM OFF` and retention eviction of the selected clip all explicitly release replay camera ownership.
- The RACE panel exposes the five review camera choices and live camera-active state.
- Generated StudioGameplay schema advances to v16.

Next broadcast/replay depth should add native compact full-session replay files, authored trackside camera banks/trigger volumes, automatic live-TV shot selection, onboard camera definitions, slow motion, flag/lap/pit timeline bookmarks, telemetry graphs, replay import/export and server-authoritative multiplayer replay capture.


## STUDIO26 — Borderless Studio, Blender Fly Navigation & Authored Static Broadcast Cameras (implemented)

STUDIO26 replaces the unreliable native Windows caption recoloring with the same undecorated-window architecture used by Heritage Engine, draws the dark draggable/minimize/maximize/close chrome in ImGui, adds Blender-style Shift+` modal fly navigation to shared 3D authoring viewports, and lets existing Replay Camera markers capture the current Studio camera location for fixed trackside replay stations.

## STUDIO27 — Moving Broadcast Camera Paths (implemented)

STUDIO27 corrects the fly-camera pitch direction from live Windows feedback and extends the replay/broadcast layer with authored moving camera rails.

- HRACE v6 persists layout-scoped Dolly/Crane/Cable/Drone camera paths plus ordered control points while HRACE v1-v5 stay readable.
- TV CAMERAS authoring captures control points directly from the current Blender-style fly view, previews Catmull-Rom motion in the venue viewport and supports ordinary selection/framing/move-gizmo editing.
- Generated StudioGameplay schema v17 exposes camera paths and sorted path nodes to runtime.
- Replay adds a MOVING TV / Spline mode that chooses a nearby applicable path, centers traversal around the incident time, pans continuously toward the reviewed cars, and falls back to procedural trackside framing if no authored rail applies.
- The architecture stays layered: STUDIO23 incident truth, STUDIO24 replay history, STUDIO25 native FP64 camera authority, STUDIO26 static stations and STUDIO27 moving presentation paths.

Next broadcast depth should add automatic shot scoring/cutting between static and moving cameras, onboard camera banks, slow motion, replay bookmarks/telemetry overlays and compact native full-session replay storage.

## STUDIO28 — Cone Courses, Traffic Control, Autoslalom & Gymkhana (implemented)

STUDIO28 turns cones into a reusable gameplay layer shared by ordinary free roam and temporary motorsport overlays.

- HRACE v7 persists global cone-course policy, physical/semantic cone instances and ordered invisible course elements while HRACE v1-v6 remain readable.
- HGAME v12 appends Autoslalom and Gymkhana event types without renumbering the older event enum; HGAME v1-v11 remain readable.
- Generated StudioGameplay schema v18 exposes cone policy, cone instances and sorted course gates to runtime.
- RACE gains a CONE COURSES authoring tab with direct scene-surface placement, Blender-style selection/framing/move gizmos, per-cone event/physics/penalty/traffic semantics, left/right visual-cone references and gate fitting.
- Persistent cones can Guide/Discourage, Slow, Close Lane or Close Road through the existing operational traffic router, including road/link/lane targeting and emergency-vehicle closure exemption.
- Event-scoped physical cones use lightweight sleeping rigid bodies, restore authored transforms at run start and stay logically separate from invisible course authority.
- Autoslalom/gymkhana supports ordered Gate, left/right Slalom, left/right Turnaround, Stop Box and Finish elements, wrong-side penalties, skipped-element penalty/DNF and existing false-start/event-result infrastructure.
- Cone penalties reuse STUDIO23 solver contact evidence and can trigger on Contact, Displaced or Knocked Down state; optional hits become STUDIO24 replay bookmarks.
- Adjusted course time (elapsed + penalties) participates in existing personal-best persistence, while DNF runs do not overwrite records. Per-element adjusted splits are saved as one coherent reference set only when a new overall PB is established, enabling live signed gate deltas on subsequent attempts.
- Quick builders generate Start/Gate/Finish pairs, alternating slaloms, Stop Boxes, 180° turnarounds and signed 360° gymkhana circles. A selected-cone line/taper duplicator copies complete cone physics/traffic semantics for persistent roadworks or event overlays.

Next event-authoring depth can add course-template/preset libraries, marshals/spectators/start-light overlays, competitor classes and multi-run competition formats, while the same temporary-road-control layer can later drive roadworks, police diversions and live-world incidents.
