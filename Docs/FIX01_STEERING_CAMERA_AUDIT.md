# FIX01 — Steering / Camera / Coordinate Audit

The native steering regression tests were correct while the embedded GLB wheel
assemblies were visibly wrong. Inspection found the primary problem in the Lua
bridge, not in Ackermann geometry.

## Root cause
Five vector-valued Lua bindings contained copy/paste argument-index errors.
Most importantly, `Entity.SetMeshNodeAnchoredWorldDelta` read rotation X/Y/Z as
arguments 7/7/9 instead of 7/8/9. VA02H therefore discarded the actual Y
rotation sent by the native upright telemetry and duplicated X into Y. Because
native steering is primarily a Y-axis rotation, the visual wheels could point in
nonsensical or asymmetric directions even while tire physics was correct.

Corrected bindings:
- Entity.SetMeshNodeWorldPose rotation: 6/7/8
- Entity.SetMeshNodeAnchoredWorldPose rotation: 7/8/9
- Entity.SetMeshNodeAnchoredWorldDelta rotation: 7/8/9
- Physics.CreateSpringConstraint anchor B: 6/7/8
- Prefab.Instantiate rotation: 6/7/8

Project validation now checks these argument contracts.

## Camera
The chase camera now follows the authoritative interpolated rigid-body chassis
pose whenever the player has a physics body. Render-entity transforms remain a
fallback only. This prevents GLB presentation transforms/hot reload from becoming
the gameplay camera's source of truth.

## Coordinate-system decision
Blender remains the creator-facing convention: X left/right, Y longitudinal,
Z up. Heritage's current native runtime remains X right, Y up, Z forward for
now. A complete native Z-up migration would touch physics, collision, static
scene BVHs, renderer/camera, GLB transforms and tests. That migration is not mixed
into this steering hotfix because the audit found a concrete independent bug.
Coordinate conversion must remain an explicit boundary rather than being
re-invented inside steering, tire or camera code.
