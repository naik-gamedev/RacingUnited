# CAM09 — Lateral Follow Damper + Detached Free-Camera Direction Fix

CAM09 keeps the CLOUDURP15L2 moon/cloud/weather state intact and corrects two camera issues reported during live testing.

## Chase camera lateral follow

CAM07 already spring-damped heading, heave, collision recovery, speed pullback, longitudinal acceleration/braking and a small lateral-acceleration sway. The remaining abrupt motion came from the **base chase rig position**: chassis X/Z translation was copied directly every render frame.

CAM09 preserves direct forward follow, but projects each chassis displacement onto the current horizontal chassis-right axis. That lateral displacement accumulates a bounded camera-rig lag (0.65 m maximum) and returns to zero through a critically damped 1.05 Hz spring. The same follow lag is applied to the eye and target, so it behaves as positional inertia rather than another yaw wobble. The existing smaller acceleration sway remains independent and eye-only.

This means steering/lane-change side motion eases into its final position and eases back instead of translating one-for-one with the chassis and stopping abruptly.

## Detached free camera directions

Heritage's renderer constructs screen-right as `cross(forward, up)`, while the older vehicle-local authoring basis uses `cross(up, forward)`. Those vectors are opposites. Detached navigation had reused the authoring basis, which made A/D and horizontal mouse-look feel reversed.

CAM09 uses the renderer-facing right vector only for detached translation and reverses detached yaw input to match the rendered screen convention. Vehicle-local camera authoring semantics are left unchanged. Vertical mouse-look remains conventional (mouse up looks up, mouse down looks down).

The CAM08 Shift travel gear remains 400 m/s for cloud fly-through testing.
