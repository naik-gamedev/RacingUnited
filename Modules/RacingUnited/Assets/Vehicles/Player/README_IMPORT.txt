RACING UNITED - PLAYER VEHICLE VISUAL SLOTS
============================================

Step 29J keeps the creator-owned PlayerCar.obj slot and adds an optional
articulated wheel slot.

WHOLE-CAR QUICK TEST
--------------------
1. Export your car from Blender as OBJ.
2. Name it exactly:
       PlayerCar.obj
3. Replace this file:
       Modules\RacingUnited\Assets\Vehicles\Player\PlayerCar.obj
4. The renderer watches OBJ write times, so geometry edits reload without a C++
   rebuild.
5. Open:
       VEHICLE > VISUAL > BODY
6. Use live offset / rotation / scale controls to align the authored body.

ARTICULATED WHEEL TEST
----------------------
If PlayerCar.obj already contains its four wheels, leave articulated wheels OFF
or both the rigid authored wheels and animated wheel meshes will be drawn.

For a proper Step 29J articulated test:
1. Export a body-only PlayerCar.obj with the wheels removed/hidden.
2. Export one wheel centered on its own origin as:
       PlayerWheel.obj
3. Author that wheel with:
       X = axle direction
       Y = up
       Z = forward
       1 unit = 1 metre
4. Replace:
       Modules\RacingUnited\Assets\Vehicles\Player\PlayerWheel.obj
5. Open:
       VEHICLE > VISUAL > WHEELS
6. Enable articulated wheel meshes.

The shipped PlayerWheel.obj is only a generic 0.42 m-radius placeholder. The
same asset is currently reused at all four corners, but the vehicle definition
already stores an asset path per wheel so future cars can use different
front/rear/left/right visual assemblies without changing the solver.

WHAT THE WHEELS FOLLOW
----------------------
Rendered wheel transforms are driven from native VehicleSystem telemetry:
- suspension length / wheel center
- Ackermann steering angle
- simulated wheel rotation

Lua does not invent a second wheel simulation. It only presents native state.

VISUAL-ONLY FITMENT CONTROLS
----------------------------
The WHEELS panel temporarily allows radius, width, track, wheelbase and vertical
visual alignment. These do NOT move tire contact points or alter handling.
Production vehicle definitions should ultimately make authored and physical
geometry agree exactly.

CURRENT RENDERER LIMITATIONS
----------------------------
- OBJ geometry and vertex normals are supported.
- One engine-side colour is used per entity mesh.
- OBJ MTL/material textures are not yet the production material pipeline.
- The later glTF/PBR pipeline will replace this temporary multi-OBJ workflow.

IMPORTANT FOR FUTURE UPDATE ZIPS
--------------------------------
PlayerCar.obj is creator-owned after you replace it. Normal incremental engine
updates must NOT package PlayerCar.obj unless an asset replacement is explicitly
intended. PlayerWheel.obj is also a creator slot once you replace the placeholder.
