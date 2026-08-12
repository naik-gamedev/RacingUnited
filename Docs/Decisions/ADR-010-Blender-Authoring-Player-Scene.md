# ADR-010 — Blender Authoring Coordinates and Player Scene Bridge

## Status
Accepted; amended through Step 29J.4A.

## Context
Racing United assets are authored primarily in Blender. Blender's natural scene
axes are X left/right, Y forward/backward, Z height, while Heritage Engine's
current native simulation convention is X right, Y up, Z forward. Asking artists
to rotate, translate or scale production geometry merely to satisfy engine
internals creates avoidable mistakes and destroys the value of exact 1:1 source
geometry.

The prototype also needed a creator-owned environment that can be replaced
without recompiling the engine so vehicle development is not confined to the
old gray physics laboratory.

## Decision
- Racing United's permanent **content-authoring convention follows Blender**:
  - X = left/right.
  - Y = forward/backward.
  - Z = height.
  - 1 Blender unit = 1 metre.
- Engine-native coordinates remain an implementation detail. Importers perform a
  deterministic conversion when required.
- Vehicle semantic forward is specified separately from scene axes: creator-authored
  vehicle noses point toward Blender **-Y**. See ADR-011.
- For the temporary OBJ bridge, Heritage Engine explicitly understands Blender's
  default OBJ export convention: file X = Blender X, file Y = Blender Z, and
  file Z = -Blender Y. The import boundary maps that to native engine X/right,
  Y/up, Z/forward and reverses triangle winding for rendering.
- Creator-authored geometry is 1:1 by default. Routine body/wheel scale and
  translation compensation is removed from the normal vehicle-visual workflow.
- `Assets/Scenes/Player/PlayerScene.obj` is the temporary creator visual-scene
  slot and retains OBJ hot reload.
- `PlayerScene_Collision.obj` supplies **exact authored static triangles**. The
  query path uses them for suspension/tire raycasts, and the current static-world
  solver also uses the same BVH for dynamic sphere/box rigid-body contacts. A
  large Blender terrain therefore remains a terrain surface instead of being
  collapsed into one enormous axis-aligned box.
- This decision originally introduced the triangle bridge as query-only. The
  later static-world implementation now also resolves dynamic primitive
  sphere/box bodies against exact static triangles through the shared BVH. It
  still does not define general moving/concave triangle-mesh collision.
- OBJ object names may identify asphalt/road, wet asphalt, gravel, dirt, grass,
  snow, ice, kerbs/curbs and painted lines so each tire receives surface identity.
- OBJ does **not** preserve Blender object-origin/pivot metadata as spawn data.
  A mesh object named `SPAWN_PLAYER` or `PLAYER_SPAWN` may therefore provide the
  horizontal spawn location. Step 29J.4 accepts that marker from either the
  visual OBJ or the collision OBJ, then snaps its height to the actual triangle
  surface at that X/Z location. The marker itself never becomes collision.
- If no marker exists, the bridge first uses the walkable triangle under world
  horizontal origin, then the nearest walkable triangle to origin. It never
  derives spawn height from the top of an entire mesh bounding box.
- Numeric sliders must support exact keyboard entry as well as mouse dragging.
  Step 29J.2 adds double-click-to-type behavior globally to `UI.SliderFloat`.

## Consequences
Artists keep precise Blender geometry, dimensions and scene placement as source
truth. A normal terrain OBJ can immediately support vehicle ground/suspension
queries without manually reducing the whole landscape to giant collision boxes.

The OBJ-era bridge was later superseded for Racing United by the GLB authoring
path while preserving the same coordinate/source-truth contract. Static world
triangles are now BVH accelerated and participate in primitive rigid-body
contact; future world-physics work should extend capability through explicit
contracts rather than overextending import-format special cases.
