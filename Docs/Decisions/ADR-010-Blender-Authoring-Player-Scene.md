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
- `PlayerScene_Collision.obj` is now queried as **exact authored triangles for
  read-only world queries**, most importantly vehicle suspension/tire raycasts.
  A large Blender terrain therefore remains a terrain surface instead of being
  collapsed into one enormous axis-aligned box.
- The triangle bridge is not yet full rigid-body triangle-mesh collision.
  Chassis/body impacts against arbitrary scene triangles remain a later world-
  physics feature. Existing primitive box/sphere colliders continue to handle
  rigid-body contacts.
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

This remains a temporary OBJ-era bridge. Step 29K should build on the same
authoring contract with glTF hierarchy, named nodes and proper materials; later
world-physics work should add production static-mesh/convex rigid-body contact
and acceleration structures rather than overextending the prototype importer.
