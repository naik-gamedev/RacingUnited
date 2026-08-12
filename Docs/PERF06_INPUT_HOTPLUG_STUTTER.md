# PERF06 - Input hotplug stutter removal

## Problem

Heritage's Windows DirectInput backend called `IDirectInput8::EnumDevices` from the gameplay thread every 2.0 seconds. Hardware enumeration can block unpredictably on Windows and matched the reported rhythmic ~2 second frametime hitch extremely closely.

## Policy

DirectInput device discovery is now explicit rather than periodic.

- Enumerate once when the input system starts.
- Poll already-known DirectInput devices every frame as before.
- Do not call `EnumDevices` periodically while driving.
- If a wheel, shifter, pedal set, handbrake, or other DirectInput device is plugged in after launch, open Settings -> Input -> Bindings and press **REFRESH CONTROLLERS / WHEELS**.

This keeps hotplug support without putting potentially blocking hardware enumeration on a two-second gameplay timer.

## Diagnostics

The F8 performance overlay now separates **Events / input** and **Audio** from ordinary housekeeping. If a residual hitch remains, the subsystem timing should make the next culprit easier to identify.

## Authoritative baseline

PERF06 was made directly against the user-uploaded 2026-08-08 repository archive, not an older incremental patch snapshot.
