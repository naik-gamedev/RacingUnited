# Vehicle Camera Authoring

## CAMLAB01 status

Heritage Engine now has a dedicated vehicle-mounted camera controller for creator
and diagnostic views. The ordinary spring-damped chase camera remains separate.
Racing United owns the named presets and persistence through module save data.

## Runtime coordinate contract

Vehicle camera positions are expressed relative to the interpolated player
chassis in Heritage runtime coordinates:

- `+X` = vehicle right;
- `+Y` = vehicle up;
- `+Z` = vehicle forward.

Pitch, yaw and roll are stored in degrees. Positive pitch looks upward, positive
yaw turns toward vehicle-right and roll rotates the image about the forward axis.
The full interpolated chassis right/up/forward basis is used, so fixed cameras
follow body roll, pitch and yaw exactly.

## Racing United camera set

The LAB > CAMERAS page provides independently editable presets named:

- Cockpit
- Nose
- Gearbox
- Roll Bar
- F Susp
- R Susp
- FL Wheel
- FR Wheel
- RL Wheel
- RR Wheel

The four wheel views default to a chassis-relative point slightly behind, above
and outside each wheel, with a small inward yaw so the wheel and road ahead are
visible together. They intentionally remain chassis-relative rather than
wheel-upright-relative, allowing suspension travel and steering motion to remain
visible in the camera.

## Persistent creator settings

Each named camera owns independent X/Y/Z, pitch/yaw/roll values. The LAB exposes
both sliders and exact numeric inputs. Changes are live but are only persisted
when the creator presses `SAVE CURRENT CAMERA` or `SAVE ALL CAMERAS`.

Camera values are keyed by vehicle definition ID in the module save store. A
creator may therefore author different camera sets for different vehicles without
changing native engine source.

## Free-fly authoring

A selected vehicle camera can enter Blender-style fly navigation with the LAB
button or `Shift + Grave` (`Shift + \`` / the `¨` key position on some layouts).
While active:

- mouse = look;
- W/A/S/D = forward/left/back/right;
- Q/E = down/up;
- Shift = 4x movement speed;
- Ctrl = 0.25x movement speed.

The cursor is captured during fly authoring and restored on exit. Vehicle driving,
steering and gear commands are suppressed while fly navigation owns those keys.
The resulting pose remains in the selected preset and can then be saved.

## Ownership

- `Camera/ChaseCamera.*` owns the normal dynamic chase view.
- `Camera/VehicleCameraController.*` owns live fixed/fly vehicle camera mechanics.
- `LuaCameraBindings.cpp` is the narrow module-facing bridge.
- `Vehicles/CameraViews.lua` owns Racing United names, defaults and persistence.
- `UI/Vehicle/CameraLabPanel.lua` owns creator controls.

The engine does not hard-code Racing United's camera names or save keys.

## Detached free-flight UI handoff (CAM10)

The ordinary detached world camera is toggled with the bindable `Toggle Free Camera`
action (default Grave). It keeps an FP64 world-space eye, uses the rebindable
Camera Forward/Backward/Left/Right/Up/Down actions, and uses Shift as the 400 m/s
travel gear added for cloud-volume inspection.

A visible Racing United prototype control panel now temporarily borrows the mouse
from fly navigation. `Tab` therefore has symmetric behavior while detached:
show controls -> cursor becomes normal/clickable and fly motion pauses; hide
controls -> cursor is recaptured and the same detached camera immediately resumes.
The ESC pause/settings menu uses the same temporary handoff. The camera is not
exited or rebuilt in either case, and the first mouse delta after re-capture is
discarded so GLFW cursor-mode changes cannot snap the view.

The vehicle handbrake toggle is persistent across this camera ownership change.
Live driving inputs remain suppressed while fly navigation owns the controls, but
a previously latched handbrake continues to feed full handbrake input to physics.
