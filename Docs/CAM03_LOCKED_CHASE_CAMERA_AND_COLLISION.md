# CAM03 - Locked Chase Camera and Terrain Collision

## Goal
The gameplay chase camera must remain behind the player's chassis through full
roundabouts, yaw wrap, spins, hills and GLB/entity reloads. It must also avoid
passing through creator-authored static scene collision when the car enters a
ditch or drives near a wall/embankment.

## Root cause addressed
Previous chase-camera revisions derived heading from the Y component of an Euler
rotation. The rigid body internally owns a quaternion, and converting that
orientation to Euler angles is unnecessary for a camera that only needs the
vehicle's forward direction. Euler extraction can become awkward around angle
wrap and pitched/rolled chassis poses.

CAM03 adds `RigidBodySystem::interpolatedBasis()`. The chase camera follows the
interpolated quaternion's actual +Z chassis-forward vector and projects it onto
the horizontal X/Z plane. No steering-input sign is used by the camera.

## Rear-lock behavior
- Camera yaw is a damped spring follower of actual chassis heading.
- Heading is unwrapped around the current camera heading.
- Relative heading lag is hard-clamped to +/-7 degrees.
- The camera therefore retains a small NFSU-like side reveal in a turn but is
  never allowed to become a free 360-degree orbit camera.
- If chase state fails, the gameplay fallback is also derived from the rigid
  body's forward basis and stays behind the player.

## Jump / landing behavior
World-up heave inertia is retained. Sudden chassis-height motion excites a
bounded under-damped vertical spring, while horizontal placement remains tied to
the chassis so racing speed does not leave the eye metres behind.

## Camera collision
CAM03 uses a read-only collision probe from a point above the chassis toward the
full desired camera eye.

- The player chassis body is ignored.
- Existing `CollisionSystem::raycast()` sees primitive colliders AND the loaded
  static triangle scene, including `Scene_*.glb` collision surfaces.
- On a hit, the eye is pulled inward by the hit distance minus 0.28 m padding.
- Pull-in is immediate so clipping is never accepted for smoothing.
- When the obstruction disappears, camera distance springs back outward.
- This is deliberately a query, not a dynamic rigid body, so the camera cannot
  push cars, wake objects, or destabilize physics.

A future improvement can replace the centre ray with a swept camera-radius
query against static triangles for stronger corner/near-plane protection. The
current probe directly solves the reported terrain/ditch penetration case.

## Precision
Absolute chase-camera state remains FP64 and is converted to the compact
floating-origin FP32 render frame only at the render boundary.

## Validation performed for this patch
- ChaseCamera and RigidBodySystem compile with GCC C++20 and aggressive warnings.
- Synthetic three-roundabout heading test repeatedly crosses +/-180 degrees and
  verifies the desired eye remains behind the chassis with <=7 degree lag.
- Collision test verifies immediate pull-in and smooth outward recovery.
- Static-triangle raycast test verifies a creator-authored triangle surface is
  detected by the same query used by the camera.
