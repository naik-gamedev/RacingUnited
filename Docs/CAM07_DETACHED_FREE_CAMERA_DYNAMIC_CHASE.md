# CAM07 — Detached Free Camera, Dynamic Chase Damping and Handbrake Toggle

## Runtime camera modes

Heritage now has three deliberately separate player-camera authorities:

1. **Normal chase** — FP64 spring-damped chase camera with collision.
2. **Vehicle camera authoring** — existing vehicle-local named camera poses and Shift+Grave fly editing.
3. **Detached free camera** — FP64 world-space camera copied from the current rendered view at activation. After activation it no longer inherits vehicle translation, rotation, pitch or roll.

The detached camera is toggled through the module input action `Toggle Free Camera` (default `Grave`; Shift+Grave also reaches it while normal chase is active). It captures the mouse and uses the rebindable Camera action group:

- Camera Forward / Backward / Left / Right
- Camera Up / Down
- Camera Fast / Slow

Default navigation is WASD + E/Q, Shift fast, Ctrl slow. Base detached speed is 8 m/s. CAM08 upgrades detached `Camera Fast` to a 50x travel gear (400 m/s) so large maps and the volumetric-cloud layer can be reached quickly; vehicle-local authoring fly mode keeps its original 4x Fast multiplier. Slow remains 0.25x. Pressing the toggle again releases the cursor and returns directly to chase camera.

The free-camera eye is stored in global FP64 coordinates and only converted to the current floating-origin FP32 render frame at submission time.

## Lower chase composition

The default chase camera was lowered and flattened:

- Distance: 6.80 m
- Eye height: 2.20 m
- Target height: 0.95 m
- Look-ahead: 2.75 m

This produces substantially less top-down road/roof view and more horizon, closer to the requested early-2000s street-racing chase composition.

## Fully damped dynamic motion

Existing heading, manual-orbit return, vertical heave and collision recovery remain spring-damped. CAM07 adds bounded critically/over-damped translational response:

- small speed-dependent boom pullback;
- acceleration adds a subtle rearward camera offset;
- braking adds a subtle forward offset;
- lateral acceleration adds opposite-direction camera sway;
- all new offsets settle through one spring-damper instead of stopping abruptly.

The offsets are intentionally small and hard-bounded. They do not replace the exact vehicle position with a loose positional spring, so high-speed driving cannot leave the camera metres behind the car.

Paused gameplay disables new acceleration sampling to avoid interpreting a frozen rigid-body velocity as a braking impulse when entering or leaving the ESC menu.

## Input / handbrake latch

`Handbrake Toggle` is a separate rebindable Car action (default `B`). Each press flips a runtime latch. While latched, effective handbrake input remains 1.0 indefinitely until the action is pressed again. The original hold-style `Handbrake` action remains unchanged and is max-combined with the latch.

The toggle edge is consumed exactly once per rendered input frame even if the fixed-step accumulator executes multiple physics substeps.
