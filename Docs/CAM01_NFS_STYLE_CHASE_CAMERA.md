# CAM01 - Spring-Damped Chase Camera

## Goal
Replace the temporary rigid chase camera with a racing-game camera inspired by the behavior described from Need for Speed Underground: steering reveals a small amount of the corresponding side of the car, releasing steering recenters smoothly, and jumps/landings excite a small damped vertical oscillation.

This is a behavior-inspired implementation, not a reverse-engineered copy of proprietary camera constants.

## Coordinate / precision policy
- Chase camera absolute state is stored in FP64 global coordinates.
- PhysicsWorld floating-origin rebases therefore do not kick or reset the camera spring.
- Once per rendered frame the camera is converted to the current compact FP32 local frame.
- Renderers remain camera-relative FP32 on the GPU.

## Initial tuning
- Boom distance: 6.6 m
- Eye height: 2.70 m
- Look-ahead: 2.0 m
- Maximum steering peek: 5.5 degrees
- Steering spring: 1.85 Hz, damping ratio 0.90
- Vertical inertia: 72% of sudden chassis height motion
- Vertical spring: 1.90 Hz, damping ratio 0.58
- Maximum vertical lag: 1.20 m

Positive Racing United steering means left. Positive steering rotates the chase boom toward the vehicle's left side, exposing a small amount of the left body side. Right steering mirrors the behavior.

## Runtime behavior
The spring state is updated once per rendered frame, before any single- or triple-monitor render passes. This prevents multi-monitor rendering from advancing the camera simulation multiple times in one frame.

Horizontal vehicle translation is followed directly so high road speed does not make the camera trail metres behind the car. Steering offset is spring-damped in vehicle-relative space. Vertical chassis motion excites a separate bounded world-space heave spring, so jumps have weight without destabilizing the chase distance.

Large teleports/resets snap the spring cleanly rather than producing a violent camera launch.

## Future extensions
- Camera collision / obstruction avoidance.
- User/module tuning presets.
- Speed-dependent FOV or boom distance if desired.
- Small acceleration/braking pitch response.
- Cockpit/hood/bumper camera framework sharing the same camera controller interface.
