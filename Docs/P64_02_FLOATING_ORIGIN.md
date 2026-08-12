# P64-02 — FP64 World Origin + Local Simulation Frames

## Purpose

Heritage Engine now separates **absolute world location** from the compact
coordinate frame used by real-time physics and rendering.

The design goal is not to make every number a `double`. It is to keep the
numbers that remain FP32 small enough that FP32 is operating in its best range.

## Coordinate layers

1. **FP64 global origin** — `PhysicsWorld::m_globalOrigin` stores the absolute
   offset of the current local simulation frame.
2. **FP32 local physics world** — rigid bodies, primitive collision and static
   triangle collision are kept close to the active anchor body.
3. **Vehicle-local geometry** — wheel mounts and high-rate point-velocity
   calculations use chassis-relative offsets instead of recovering a one-metre
   lever arm by subtracting two large world positions.
4. **FP64 tire/high-rate scalar solver** — P64-01 remains authoritative for the
   tire, suspension and unsprung-mass scalar paths.
5. **Camera-relative FP32 rendering** — before draw submission, mesh/debug
   positions are translated by the camera position and the shader sees the
   camera at `(0,0,0)`.
6. **D32_FLOAT reversed-Z** — the GFX08 depth policy remains unchanged.

## Floating-origin behavior

Racing United assigns the player chassis as the floating-origin anchor with a
4096 metre threshold.

When any anchor coordinate crosses the threshold, rebasing happens **after the
complete fixed physics step**:

- every rigid body current/previous position shifts by the same amount
- fixed world constraint anchors shift by the same amount
- static scene triangles shift and their BVH is rebuilt
- cached world-space contacts are discarded and reconstructed next step
- root entities receive the same shift before render synchronization
- the FP64 global origin accumulates the shift

Relative distances, velocities, rotations, tire state and race order therefore
do not change.

All vehicles in a 150-car field remain distinct. They share one nearby physics
frame for collision, while each vehicle's own wheel/suspension geometry remains
chassis-local.

## Lua API

- `Physics.SetFloatingOriginAnchor(body, thresholdMeters)`
- `Physics.ClearFloatingOriginAnchor()`
- `Physics.GetWorldOrigin()`
- `Physics.GetOriginRebaseCount()`
- `Physics.LocalToGlobal(x, y, z)`
- `Physics.GlobalToLocal(x, y, z)`
- `Physics.GetBodyGlobalPosition(body)`
- `Physics.SetBodyGlobalPosition(body, x, y, z)`
- `Physics.ResetWorldOrigin()`

Lua numbers are doubles, so absolute coordinates are not truncated merely by
crossing the Lua boundary.

## Scene loading

`Scene_*.glb` remains authored in absolute Blender/glTF coordinates. At runtime:

- the visible scene root is offset by the current FP64 origin
- imported triangle collision is converted into the current local frame
- SPAWN_PLAYER is stored globally by Racing United and converted back to the
  current local frame whenever the vehicle is reset

This makes a later origin rebase transparent to the creator-authored spawn.

## What remains FP32 intentionally

- rigid-body local positions/velocities
- collision triangle local coordinates
- local wheel mounting geometry
- most entity transforms
- GPU vertex/shader math

Those values are kept numerically small by the coordinate architecture rather
than made larger merely for prestige.

## Future large-world work

The current rebasing implementation rebuilds one loaded static-triangle BVH.
That is appropriate for the present single-scene prototype. Very large free-roam
worlds should evolve this into streamed spatial chunks/regions so rebasing never
requires touching an entire continent-sized collision mesh at once.
