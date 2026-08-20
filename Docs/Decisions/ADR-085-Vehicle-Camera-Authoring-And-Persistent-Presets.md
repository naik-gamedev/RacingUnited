# ADR-085 — Vehicle Camera Authoring and Persistent Presets

## Status

Accepted for CAMLAB01.

## Context

Racing United needs technical and driving cameras that can be authored per
vehicle, including cockpit/nose/gearbox/roll-bar/suspension views and cameras
behind each individual wheel. The existing ChaseCamera is intentionally a
spring-damped driving camera and is the wrong owner for fixed creator mounts or
Blender-style free navigation.

Camera edits must survive restart, but engine code should not contain
Racing-United-specific camera names or project save data.

## Decision

Add a small native `VehicleCameraController` beside `ChaseCamera`. It stores one
live vehicle-local position plus pitch/yaw/roll pose and optional fly-navigation
state. Engine simulation converts that pose through the player's full interpolated
right/up/forward chassis basis into the render `CameraFrame`.

Expose only the live pose/activation/fly controls through a `Camera` Lua API.
Racing United defines the named presets, default locations and persistence in Lua
using the module Save store. Saved values are keyed by vehicle definition ID.

Free-fly authoring is vehicle-local and uses mouse look plus WASD/QE. `Shift +
Grave` toggles the mode. Driving/gear input is suppressed while fly navigation is
active so creator navigation cannot accidentally command the car.

Wheel cameras remain chassis-relative by design. This lets the wheel move within
the shot as suspension and steering work, which is more useful for diagnosis than
locking the camera to the moving upright.

## Consequences

- Chase-camera dynamics remain unchanged when no named camera is active.
- Every camera preset can be edited with exact numbers or sliders and saved
  independently.
- Adding future vehicle-specific camera names does not require native C++ changes.
- The native camera service is reusable by other modules without inheriting
  Racing United's UI/persistence policy.
