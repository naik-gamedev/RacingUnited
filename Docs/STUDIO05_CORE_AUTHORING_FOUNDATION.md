# STUDIO05 — Core Racing / Free-Roam Authoring Foundation

HeritageStudio.exe is now more than an Audio Lab shell. STUDIO05 establishes persistent editor-owned data for the categories a racing/free-roam simulator needs to author without booting Racing United.

## Presentation
- Loads `Assets/Fonts/Orbitron-SemiBold.ttf` for Studio headings when present.
- Keeps the normal ImGui body font for dense descriptions and controls.
- Falls back safely if the heading font is missing.
- STUDIO04 slider descriptions/tooltips/collapsible audio groups remain active.

## Persistent authoring assets
Studio saves its working project under:

`UserData/Modules/RacingUnited/HeritageStudio/Projects/RacingUnited/`

Formats introduced by this milestone:
- `scene.hscene` — scene entities, transforms, spawns, zones and triggers.
- `race.hrace` — start/finish, checkpoints, sectors, grid, pits, recovery, replay cameras, race/wet AI-line nodes.
- `traffic.hroad` — lane/intersection/rule/parking/spawn/destination nodes for free-roam traffic.
- `weather.hweather` — geographic and starting weather/surface state.
- `vehicle.hvehicleauthor` — Studio-side vehicle content linkage and eligibility metadata.

These are intentionally simple authoring formats first. Later milestones can compile them into optimized runtime assets without turning the editor format itself into runtime spaghetti.

## Scene workspace
- Add/remove/duplicate editable entities.
- Player and vehicle spawn points.
- Audio/weather zones and generic triggers.
- Position/rotation/scale, tag and asset path editing.
- Save/load `scene.hscene`.

## Race workspace
- Start/finish, checkpoints and sectors.
- Starting grid slots.
- Pit entry/exit/speed lines and pit boxes.
- Recovery positions and replay cameras.
- AI race-line and wet-line nodes.
- Save/load `race.hrace`.

## Traffic workspace
- Lane nodes and intersections.
- Stop/yield/traffic-light rules.
- Parking, spawn/despawn and destination nodes.
- Speed limit, lane count, priority, directionality, overtaking permission and density metadata.
- Save/load `traffic.hroad`.

## Weather workspace
- Latitude / longitude / elevation.
- Starting time, cloud/rain state, temperature, humidity and surface wetness.
- Save/load `weather.hweather`.

## Vehicle workspace
- Vehicle definition and acoustic-profile linkage.
- Tire-set linkage, fuel/spawn metadata.
- Race and free-roam traffic eligibility.
- Save/load `vehicle.hvehicleauthor`.

## Asset browser
A new `ASSETS` workspace browses `Modules/RacingUnited/Assets`, supports folder navigation, file inspection, copy-path and Explorer opening. Format-aware previews/import validation come later.

## Next logical editor milestones
1. Heritage renderer embedded in the Scene viewport with picking and transform gizmos.
2. Spline editing for race lines and traffic lanes.
3. Runtime compilers for HSCENE/HRACE/HROAD/HWEATHER authoring formats.
4. GLB-aware vehicle/scene asset inspection.
5. Event/championship/mission authoring and free-roam activities.
6. AI behavior/debug visualization, nav/traffic validation and race steward rules.
