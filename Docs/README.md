# Heritage Engine Technical Memory

This directory is the repository-owned memory for Heritage Engine and Racing United. Chat history, an AI model's recollection, and an old screenshot are never authoritative for exact APIs or ownership rules.

Read these documents before changing an interconnected system:

1. `PROJECT_STATE.md` — current known-good milestone and immediate roadmap.
2. `AI_WORKFLOW.md` — required workflow for an AI or human contributor.
3. `ARCHITECTURE.md` — subsystem boundaries and dependency direction.
4. `MEMORY_OWNERSHIP.md` — object lifetime, handles, destruction, and Lua/C++ safety.
5. `LUA_API_RULES.md` — binding contract and generated API manifests.
6. `PHYSICS_ARCHITECTURE.md` — fixed-step and multi-rate physics rules.
7. `VEHICLE_ARCHITECTURE.md` — generic vehicle composition and Lua/native split.
8. `VEHICLE_TOPOLOGY_ARCHITECTURE.md` — shared vs two-wheel/three-wheel/four-plus-wheel topology ownership.
9. `VEHICLE_DYNAMICS_LAB.md` — native recording, repeatable experiments, plots and CSV diagnostics.
10. `VEHICLE_WORKSHOP.md` — versioned topology authoring, validation, live preview and module-owned exports.
11. `UNSPRUNG_MASS_MODEL.md` — scalable wheel/upright inertia, radial tire compliance and wheel-hop rules.
12. `SUSPENSION_GEOMETRY.md` — authoritative steering-axis, camber, toe and upright-pose rules.
13. `WHEEL_FITMENT_AND_ALIGNMENT.md` — reference assembly vs per-corner ET/spacer/camber/toe/caster setup contract.
14. `TERRAIN_CONTACT_DIAGNOSTICS.md` — native wheel support status, query evidence and terrain-loss regressions.
15. `Decisions/` — architecture decision records that must not be silently reversed.

Step 29L contributors must also read `VEHICLE_DEFINITION_RUNTIME.md` for the
native compiler, suspension-provider and runtime-loader contract.
Step 29P suspension work must also follow `SUSPENSION_MODEL.md`,
`UNSPRUNG_MASS_MODEL.md`, and `SUSPENSION_GEOMETRY.md` for force, wheel-hop,
upright pose, telemetry, scalability, and future geometry/damage conventions.
Step 29Q terrain and collision work must preserve the observable wheel-contact
contract in `TERRAIN_CONTACT_DIAGNOSTICS.md` and ADR-018.

Generated reports are written to `Build/Reports/`. Runtime diagnostics are written to `UserData/Diagnostics/`.

- `MATERIALS_AND_TEXTURES.md` - OBJ/MTL material and texture-map authoring contract.
- `SKY_DAY_NIGHT.md` - visible procedural sky, day/night clock, developer shortcuts and Environment Lua API.

- `MODULE_ASSET_DISCOVERY.md` - active-module asset indexing, discovery Lua API and Racing United Vehicle_*.glb development auto-load.

- `VEHICLE_ASSET_METADATA.md` - Blender Custom Properties/glTF extras vehicle-part metadata and compatibility inspection.
- `VEHICLE_ASSET_NODE_BINDING.md` - VA02 embedded WH_* Root/Pivot binding to native suspension/upright/spin telemetry.

- `SCENE_GLB_AUTHORING.md` - single-GLB visible world, hidden collision, surface metadata, spawn and UTF-8 scene authoring contract.
