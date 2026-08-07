# Terrain Contact Diagnostics

Step 29Q establishes an observable baseline before Heritage Engine replaces its
temporary query-only static triangle bridge. It does not silently correct a
lost contact. Every wheel records why its latest suspension support query did
or did not produce authoritative load.

## Native status contract

`WheelState.contactStatus` has stable numeric values and a lowercase diagnostic
name returned by `wheelContactStatusName`:

| Status | Meaning |
| --- | --- |
| `supported` | The road is within suspension travel and the tire carries load. |
| `suspension_bottomed` | The road lies above minimum suspension length; the clamped support remains valid and penetration is reported. |
| `road_detected_no_load` | Geometry is in reach, but the constrained unsprung/tire state currently carries no load. |
| `surface_behind_ray_origin` | A previously supported wheel missed downward and found support in the opposite direction; the support surface crossed behind the ray origin. |
| `outside_static_scene_bounds` | The wheel origin left the imported static scene's horizontal bounds. |
| `no_world_geometry` | No eligible support geometry exists beyond the vehicle's own ignored colliders. |
| `no_ray_candidates` | World geometry exists, but no primitive or individual static triangle overlaps the support ray bounds. This is the expected signature for a real hole. |
| `ray_candidates_missed` | Broadphase candidates existed, but exact intersection rejected all of them. |
| `beyond_suspension_reach` | A hit exists below the wheel but beyond maximum droop plus tire radius. |
| `no_support_hit` | Initial/fallback state before a more specific result is available. |

The persistent `contactLossTransitionCount` increments only when a wheel changes
from grounded to ungrounded. A wheel that remains airborne does not inflate the
counter at 1000 Hz.

## Ray evidence

The collision system caches diagnostic state for the most recently completed
closest-hit raycast:

- eligible primitive collider candidates;
- individual static triangle candidates;
- exact narrowphase test count;
- whether a static scene and cached scene bounds exist;
- whether the origin is inside the scene's X/Z bounds;
- whether the complete ray AABB overlaps the scene bounds;
- whether any static triangle hit; and
- whether the chosen closest hit was a static triangle.

Static scene bounds are calculated once when triangles are installed, not for
every wheel query. Individual triangle AABBs are still constructed during each
query in Step 29Q; Milestone 2 replaces that linear scan with a deterministic
spatial acceleration structure.

## Tunnelling classification cost

A reverse support probe is performed only on a grounded-to-airborne transition.
If it finds geometry above the ray origin, the status becomes
`surface_behind_ray_origin` and remains visible while that miss persists. Normal
grounded and already-airborne substeps do not pay for a second raycast.

This probe is diagnostic evidence, not collision recovery. The chassis and
wheel are still permitted to pass through query-only triangles until the
authoritative static triangle contact path lands in Milestone 2.

## Lua and live inspection

`Vehicle.GetWheelContactDiagnostic(vehicle, wheel)` returns 13 values without
changing the established 51-value `Vehicle.GetWheelState` ABI. Racing United
stores those fields in `vehicleWheelTelemetry` and displays them in
Vehicle > Suspension > Live.

## Deterministic regression coverage

`terrainContactDiagnosticsClassifyFailureModes` verifies:

- an exact shared triangle edge;
- deliberately reversed triangle winding and returned normal orientation;
- a steep triangle surface and returned support normal;
- a 20 mm real gap inside the global scene bounds;
- suspension bottom-out and penetration depth;
- support geometry just beyond maximum suspension reach;
- a 120 m/s downward state whose support plane is already behind the ray;
- departure beyond imported scene bounds; and
- an airborne chassis landing on the static triangle query surface.

The test prints the final native status names so a future contact implementation
cannot turn a failure into an unexplained boolean.
