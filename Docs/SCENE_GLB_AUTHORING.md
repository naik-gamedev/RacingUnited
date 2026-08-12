# Heritage Engine Scene GLB Authoring Contract (SC01)

Racing United now treats a `Scene_*.glb` file as the preferred creator-owned
world container. One GLB may contain visible scene geometry, static drive-surface
collision authoring, spawn metadata, materials, textures and ordinary node
metadata.

## Folder / discovery convention

Place scene GLBs anywhere below the active module's `Assets/Scenes` tree.
Racing United discovers files whose filename begins with `Scene_`.

Example:

```text
Modules/RacingUnited/Assets/Scenes/Scene_Ivarcko_Jezero/
    Scene_Ivarčko_Jezero.glb
```

Module-facing paths are UTF-8. Names containing Slovenian/Croatian/Czech/Slovak
characters such as `č`, `š`, `ž`, `ć`, `Č`, `Š` and `Ž` are supported by the
asset registry and the GLB scene/mesh resolution path.

## Collision geometry

Preferred Blender Custom Properties on a collision root/object:

```text
heritage.role = collision_mesh
heritage.collision_type = static_triangle_mesh
```

As a convenient fallback, Heritage also recognizes node names containing the
collision token in forms such as:

```text
Scene_Ivarcko_Jezero_Collision
Road_Collision
Collision_Road
```

A collision root may own child mesh objects. Descendants inherit the collision
role, so a clean Blender hierarchy can be:

```text
Scene_Ivarcko_Jezero_Collision
├─ Road_Asphalt
├─ Grass
├─ Kerb
└─ Gravel
```

Collision authoring nodes are hidden automatically by the visual GLB renderer.
They remain available to the static triangle query importer.

## Surface metadata

Per collision object/child, optional Custom Properties can explicitly identify
the drive surface:

```text
heritage.surface = asphalt
heritage.wetness = 0.0
```

Recognized surface strings include:

- `asphalt` / `tarmac` / `road`
- `grass`
- `gravel`
- `dirt`
- `snow`
- `ice`
- `kerb` / `curb`
- `paint` / `painted_line`
- `wet_asphalt`
- `mud`
- `sand`
- `soft_soil` / `softsoil`
- `deep_snow` / `deepsnow` / `powder_snow`

Object names remain a fallback when no explicit surface property exists.

### TIRE15B1 physical surface parameters

For deformable materials (`mud`, `sand`, `soft_soil`, `deep_snow`), Blender Custom Properties may refine the physical world material rather than relying on Heritage's default family preset:

```text
heritage.surface.density_kg_m3
heritage.surface.loose_depth_m
heritage.surface.moisture
heritage.surface.bekker_kc
heritage.surface.bekker_kphi
heritage.surface.sinkage_exponent
heritage.surface.cohesion_pa
heritage.surface.friction_angle_deg
heritage.surface.shear_modulus_m
heritage.surface.compaction_stiffness_gain
heritage.surface.compaction_shear_gain
heritage.surface.plastic_rut_fraction
heritage.surface.compaction_rate_hz
heritage.surface.loose_depth_loss_per_compaction_m
heritage.surface.mf_friction_scale
heritage.surface.stiffness_scale
heritage.surface.rolling_resistance_scale
heritage.surface.relaxation_scale
```

Any drive surface may also author a local reference road temperature:

```text
heritage.surface.temperature_c = 20.0
```

Surface identity is resolved child-first. Physical parameter overrides are then applied **parent to child**, so a terrain parent can define a broad soil profile while a particular collision child refines only one or two values. Invalid/out-of-range values are ignored and the validated material-family defaults remain the fallback. Live weather can add global wetness and optionally override road temperature at runtime; it does not destroy the authored base metadata.

## Player spawn

The preferred authoring object is a Blender Empty named:

```text
SPAWN_PLAYER
```

Optionally add:

```text
heritage.role = spawn_player
```

The Empty does not need geometry. Heritage uses its GLB node transform and then
snaps its vertical position to the imported collision surface. If no marker is
present, the deterministic terrain-at-origin / nearest-terrain fallback remains.

## Important physics boundary

SC01 replaces the old separate OBJ scene/collision authoring workflow for
Racing United. Imported collision nodes populate one immutable BVH-backed static
triangle world. That world is shared by suspension/tire raycasts, related
queries, camera/AI sphere casts, and rigid-body contacts from dynamic sphere/box
primitive colliders.

This does **not** mean arbitrary GLB mesh objects become moving concave rigid-body
colliders. The production contract here is static creator-world geometry versus
dynamic primitive bodies. Dynamic triangle meshes, mesh-vs-mesh collision and
deformable collision remain separate future features.

The old engine OBJ importers remain available for legacy/other module content,
but Racing United no longer needs `PlayerScene.obj` or
`PlayerScene_Collision.obj` for its creator world after a valid collision-marked
`Scene_*.glb` exists.
