RACING UNITED - PLAYER SCENE SLOT - STEP 29J.4

AUTHORING COORDINATES (PERMANENT RACING UNITED CONTENT CONVENTION)
  X = left / right
  Y = forward / backward
  Z = height
  1 Blender unit = 1 metre

VISUAL SCENE
  Replace PlayerScene.obj with a 1:1 OBJ exported from Blender.
  Author normally in Blender. The temporary OBJ bridge now understands Blender's
  DEFAULT OBJ export axis conversion and maps it back to Heritage Engine at the
  import boundary. Do not rotate or rescale your source scene for the engine.

DRIVE-SURFACE SCENE
  PlayerScene_Collision.obj is intentionally separate from the pretty visual
  mesh, but Step 29J.4 no longer turns each entire Blender object into one huge
  axis-aligned box. The actual OBJ triangles participate in read-only world
  raycasts used by vehicle suspension and tires.

  This means a large sloped/hilly Blender terrain can already act as the road
  surface without manually replacing it with hundreds of proxy cubes.

  IMPORTANT: this is NOT yet full chassis-vs-triangle rigid-body collision.
  Suspension/tire ground queries follow the terrain, but arbitrary body impacts
  with triangle walls/buildings are a later physics feature.

SURFACE NAME HINTS
  Object names automatically assign tire surfaces when they contain:
    ROAD / ASPHALT / TARMAC
    WET_ASPHALT
    GRAVEL
    DIRT
    GRASS
    SNOW
    ICE
    KERB / CURB
    PAINT / LINE
  Other names use the default tire-surface fallback.

SPAWN
  A mesh object named:

    SPAWN_PLAYER

  may exist in EITHER PlayerScene.obj or PlayerScene_Collision.obj.
  Heritage Engine uses its horizontal position, then snaps spawn height to the
  actual drive-surface triangle beneath that location. The marker itself never
  becomes collision.

  If no marker exists, the engine first looks for the walkable triangle under
  horizontal world origin and then the nearest walkable triangle to origin.
  It no longer spawns on top of an entire terrain bounding box.

HOT RELOAD
  PlayerScene.obj remains hot-reloaded by the OBJ renderer. Press LOAD / RELOAD
  PLAYER SCENE after changing the collision OBJ so its drive-surface triangles
  and spawn are rebuilt.

IMPORTANT
  These OBJ slots remain a bridge to Step 29K's hierarchical glTF pipeline.
  Preserve exact Blender scale and placement; do not build production metadata
  around OBJ limitations.
