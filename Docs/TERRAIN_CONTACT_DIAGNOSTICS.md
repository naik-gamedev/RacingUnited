# Terrain Contact Diagnostics

Step 29Q established the observable wheel-support baseline while the static
triangle world was still query-oriented. The engine has since gained a BVH-backed
rigid-body path for dynamic primitive colliders against those same static
triangles. Wheel support remains independently observable: every wheel records
why its latest suspension query did or did not produce authoritative load.

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

Static scene bounds are calculated when triangles are installed. Individual
triangle queries are accelerated by the immutable `StaticTriangleBvh`; wheel
rays and rigid-body contact broadphase no longer linearly scan the complete
triangle set. Query diagnostics still report candidate and exact-test counts so
acceleration regressions remain visible.

## Tunnelling classification cost

A reverse support probe is performed only on a grounded-to-airborne transition.
If it finds geometry above the ray origin, the status becomes
`surface_behind_ray_origin` and remains visible while that miss persists. Normal
grounded and already-airborne substeps do not pay for a second raycast.

This reverse probe is diagnostic evidence, not suspension-contact recovery. A
dynamic chassis primitive may now be stopped by the static-triangle rigid-body
solver, but that must not hide a suspension ray losing support or silently change
its `WheelContactStatus`. The wheel/contact-provider diagnostic contract therefore
remains useful after rigid-body triangle contact exists.

## Lua and live inspection

`Vehicle.GetWheelContactDiagnostic(vehicle, wheel)` returns 13 values without
changing the `Vehicle.GetWheelState` field order. `GetWheelState` itself is now a
60-value TIRE03 ABI after the appended tire turn-slip/contact-patch telemetry.
Racing United stores those fields in `vehicleWheelTelemetry` and displays them in
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
- an airborne chassis landing on the static triangle support surface; and
- dynamic sphere and box rigid bodies falling onto and settling on the same
  static-triangle world.

The tests print the final native status/contact evidence so a future contact
implementation cannot turn a failure into an unexplained boolean.
