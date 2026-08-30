# STUDIO08 — GLB/PBR World Viewport

Heritage Studio's Scene, Race and Traffic workspaces now render the current Racing United world GLB underneath the Blender-style authoring overlays.

## Source of truth

Studio discovers the newest `Scene_*.glb` under:

`Modules/RacingUnited/Assets/Scenes/`

The viewport reuses Heritage Engine's existing GLB parser, mesh uploader and texture cache rather than maintaining a separate editor-only file format or importer.

## Material preview

The Studio preview understands the GLB material data already consumed by Heritage:

- base color factor / map;
- vertex color;
- normal map;
- roughness factor / map and packed channel;
- metallic factor / map and packed channel;
- ambient occlusion map and packed channel;
- emissive factor / map;
- opacity from base-color alpha / material opacity.

Lighting is intentionally a lightweight Studio PBR preview. It does not boot Racing United weather, sky, hydrology or race simulation merely to edit content.

## Shared authoring surface

The same loaded scene appears under:

- Scene object authoring;
- Race checkpoints, grids, pits, recovery and AI-line overlays;
- Traffic lane/intersection/light/parking/spawn overlays.

Placement clicks first raycast against the visible GLB geometry. If no scene is loaded or the ray misses the scene, Studio falls back to the legacy Y=0 authoring plane.

This lets checkpoints and traffic nodes land on banked roads, hills, bridges and other non-flat authored geometry instead of being forced onto an abstract plane.

## Blender-style controls retained

STUDIO07 navigation and transform conventions remain unchanged. `Home` frame-all now includes the imported Scene GLB bounds when available.

## Scene controls

The Scene viewport adds:

- `Scene GLB` visibility toggle;
- `Wireframe` toggle;
- preview exposure;
- `LATEST Scene_*.glb` discovery/reload;
- `RELOAD GLB` for Blender-export iteration.

## Architectural boundary

This is still `HeritageStudio.exe`, not a hidden Racing United boot. Studio initializes only the GLB/material GPU path required to preview authoring geometry. Runtime weather/clouds/hydrology/traffic remain outside this workspace until a dedicated opt-in preview requires them.

Future milestones can replace the simple editor lighting with fuller Heritage renderer parity, add GLB node/material inspection to the Outliner/Properties editors, add selection against imported mesh nodes, and build acceleration structures for very large-world surface picking.

## Lazy workspace initialization

The scene preview is initialized only when Scene, Race or Traffic is first opened. Launching Heritage Studio into Audio does **not** parse/upload `Scene_*.glb`, preserving the Studio rule that unrelated authoring workspaces do not boot expensive systems they do not need.
