# STUDIO10 — Gameplay, World Activity and Explicit Road-Graph Authoring

## Principle

Heritage Studio is expanded as a production editor. Existing authoring systems remain available; this milestone adds data, mechanisms, GUI and runtime publication rather than replacing earlier tools.

## Race authoring upgrades

`race.hrace` advances to HRACE v2 while retaining HRACE v1 loading compatibility.

Persistent race configuration now includes:

- lap count;
- grid-slot target;
- pit speed limit;
- formation-lap toggle;
- standing-start toggle;
- false-start penalties;
- track limits;
- general penalties.

The Race workspace can generate only missing grid slots from the Start / Finish heading. Existing grid markers are never overwritten or deleted by the generator.

## Explicit free-roam road graph

`traffic.hroad` advances to HROAD v2 while retaining HROAD v1 loading compatibility.

Traffic nodes are joined by persistent `TrafficLink` records with:

- source and destination node IDs;
- lane count;
- speed limit;
- one-way / bidirectional behavior;
- overtaking permission;
- traffic density.

The Traffic viewport renders the serialized links instead of a temporary list-order preview.

Authoring tools include:

- `AUTO LINK LANES`, which adds only missing adjacent lane links and preserves existing links;
- manual selected-node -> target-node connection creation;
- per-link inspector editing;
- selected-link deletion;
- automatic removal of attached links when the user explicitly deletes a road node.

## Gameplay workspace

A new GAMEPLAY workspace authors `gameplay.hgame`.

### Event types

- Circuit Race
- Sprint
- Time Trial
- Time Attack
- Drag
- Drift
- Touge
- Clandestine Circuit
- Clandestine Sprint
- Cruise
- Test Drive

Event rules currently include enabled state, start/finish race-marker references, laps, maximum entrants, rolling start, live traffic, police response, night-only restriction, entry fee, reward and heat/notoriety.

Clandestine event presets start with traffic, police response, night-only and a non-zero heat value enabled, but every value remains editable.

### Free-roam world point types

- Garage
- Dealership
- Fuel Station
- Repair Shop
- Car Wash
- Meet Spot
- Event Hub
- Safehouse
- Police Station
- Speed Camera
- Speed Trap
- Fast Travel
- Landmark
- Parking Area

World points have 3D position, heading, interaction radius, map discovery, fast-travel and service-price metadata. Common point types can be placed directly onto the GLB world surface from the Gameplay viewport.

## Validation

`File > Validate Authoring` and the Gameplay `VALIDATE` command check cross-layer data without silently changing it.

Current checks cover:

- multiple enabled Vehicle Spawn objects;
- events without a Start / Finish marker;
- configured grids with too few authored grid slots;
- event references to missing start/finish markers;
- impossible entrant counts;
- self-referencing or dangling road links;
- non-positive world-point interaction radii.

## Runtime publication

Studio source assets remain under the existing UserData project directory as safety/editing data.

When Race, Traffic, Gameplay or global Ctrl+S is saved, Studio additionally compiles their current state to:

`Modules/RacingUnited/Scripts/Generated/StudioGameplay.lua`

Before overwriting an existing generated asset, the previous version is copied to:

`UserData/Modules/RacingUnited/HeritageStudio/Projects/RacingUnited/RuntimeBackups/StudioGameplay_before_last_publish.lua`

Racing United loads the generated table on startup through `Scripts/Main.lua` and `Runtime/StudioGameplay.lua` exposes non-invasive runtime query helpers for:

- race configuration;
- race markers;
- traffic nodes;
- outgoing road links;
- event lookup;
- enabled-event enumeration;
- event start/finish route resolution;
- world-point lookup/filtering;
- nearest-world-point queries.

This creates a real editor -> module runtime data path without booting the game inside Heritage Studio.

## Compatibility

- HRACE v1 files remain loadable.
- HROAD v1 files remain loadable.
- Existing Scene, Weather, Vehicle, Audio and Assets workspaces remain intact.
- STUDIO09 runtime Vehicle Spawn behavior remains intact.
- The generated gameplay seed is empty until the author creates/publishes content, so this milestone does not inject unrequested events or world locations into existing scenes.
