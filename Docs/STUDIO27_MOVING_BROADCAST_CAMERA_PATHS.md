# STUDIO27 — Blender Fly Pitch Correction + Moving Broadcast Camera Paths

STUDIO27 keeps STUDIO26's HeritageEngine-style undecorated/dark Studio window and corrects the modal Shift+` fly camera pitch direction requested during Windows testing: mouse-up and mouse-down now rotate the fly view in the user's Blender muscle-memory direction while ordinary MMB orbit remains unchanged.

## Moving broadcast-camera authoring

Race authoring now contains a **TV CAMERAS** tab with four semantic rig labels: Dolly, Crane, Cable and Drone. Each path has an enabled flag, layout scope, activation radius, movement duration, easing amount and optional reverse traversal. Ordered control points are stored separately so path deletion can remove its owned points without touching static Replay Camera markers.

The intended workflow is deliberately Blender-like:

1. Enter Shift+` fly navigation.
2. Fly to the first camera location and confirm.
3. Press **ADD CONTROL POINT FROM CURRENT VIEW**.
4. Repeat for the remaining positions.
5. Use the viewport move gizmo or numeric inspector for fine adjustment.

The Race viewport renders the selected camera move as a Catmull-Rom spline and labels its control points. `F` frames the selected point, clicking a point selects it, and the move gizmo edits it directly. Two points are enough for a valid move; additional points create a smoother authored trajectory.

## HRACE v6 / runtime publication

HRACE v6 adds `CAMERA_PATH` and `CAMERA_NODE` records. HRACE v1-v5 remain readable and simply load with no moving camera paths. Saving/publishing emits generated StudioGameplay schema v17 with `broadcastCameraPaths` and `broadcastCameraNodes`; the runtime facade exposes `GetBroadcastCameraPaths(layoutId)` and `GetBroadcastCameraPathNodes(pathId)`.

The new data is presentation-only. STUDIO23 still owns physical incident truth, STUDIO24 owns replay history, and STUDIO25 owns the native FP64 world-camera submission path.

## Replay MOVING TV mode

The replay director adds a **Spline** camera mode exposed as **MOVING TV** in the Race debug UI. It selects the nearest layout-applicable authored camera path whose activation radius covers the reviewed incident/cars. The path is traversed so the incident time sits at the midpoint of the move by default; authored duration controls the full motion window, easing blends linear timing toward smoothstep timing, and Reverse flips traversal.

Only camera position comes from the spline. Orientation continues to pan dynamically toward the replay target, so the same authored rail can follow slightly different lines or two-car incidents without baking brittle look-at angles. If no valid path is nearby, MOVING TV falls back to the existing procedural trackside position rather than failing or stealing camera authority indefinitely.

## Compatibility / validation

- HGAME stays v11.
- HRACE advances from v5 to v6.
- Generated StudioGameplay advances from v16 to v17.
- HRACE v5 compatibility is retained.
- Authoring validation rejects enabled camera paths with fewer than two points, missing layout references, invalid radius/duration/easing, duplicate point orders and orphaned points.
