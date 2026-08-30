# STUDIO25 — Replay / Broadcast Camera Director + Dark Studio Chrome

STUDIO25 builds directly on STUDIO24's bounded incident replay clips. The replay recorder remains the evidence/history authority; this milestone adds a native floating-origin-safe camera path so an incident can be reviewed from useful cinematic views rather than only from free camera or ghost positions.

It also makes the native Heritage Studio title bar follow the editor's dark visual language on Windows.

## Native dark Heritage Studio title bar

Heritage Studio now requests Windows immersive dark non-client rendering immediately after the GLFW window is created. On supported Windows 11 builds it additionally requests:

- black caption/title-bar background;
- light caption text/icons;
- near-black native window border.

Older Windows 10 builds fall back to the immersive-dark-mode DWM attribute. This changes only native window chrome; the existing ImGui editor styling remains authoritative for the client area.

## Why the camera path is native

Replay positions are stored in global FP64 coordinates. Driving a review camera by moving a fake vehicle or by storing local float coordinates would eventually break when the floating origin changes.

`VehicleCameraController` therefore now exposes a detached world-pose authority. Lua can submit a global FP64 eye position plus pitch/yaw/roll, and the renderer converts it into the current local origin every frame.

New Camera Lua bindings:

- `Camera.SetWorldViewActive(active)`;
- `Camera.IsWorldViewActive()`;
- `Camera.SetWorldPose(globalX, globalY, globalZ, pitchDeg, yawDeg, rollDeg)`;
- `Camera.GetWorldPose()`.

The detached/world camera render path is evaluated independently of a player chassis. That makes the same foundation usable for post-race review, spectators, future multiplayer observers and broadcast tooling.

## HGAME v11 replay-camera policy

`MotorsportReplayConfiguration` gains a separate presentation policy:

- broadcast director enabled;
- automatic incident camera when review starts;
- incident camera distance;
- incident camera height;
- trackside camera lead distance;
- helicopter camera height;
- camera smoothing response.

HGAME v1–v10 remain readable and receive STUDIO25 defaults. Save/publish writes HGAME v11 and generated `StudioGameplay` schema v16.

## Incident review camera modes

`Runtime/RacingReplay.lua` consumes the existing STUDIO24 clip and STUDIO23 incident evidence. It never changes race authority, physics, timing or the recorded evidence.

Five modes are available:

- **Incident** — cinematic side/rear framing around the involved cars, centered between both cars when both were recorded;
- **Trackside** — camera positioned from the actual global incident point, with authored lead and alternating side placement;
- **Chase** — follows the primary incident car from behind;
- **Helicopter** — elevated tracking view for larger-context review;
- **Off** — releases the world camera and returns camera authority to the normal game path.

The director interpolates recorded vehicle poses at the scrubbed replay time, computes a look-at pose in global coordinates, then applies exponential smoothing using the authored response value.

## Camera ownership and cleanup

Replay camera ownership is explicit:

- enabling review can automatically select Incident camera;
- disabling review releases the native world camera;
- selecting another clip resets camera smoothing history;
- changing scenes or clearing replay memory releases the camera;
- if retention removes the currently reviewed clip, ghosts and the directed camera are both released;
- `CAM OFF` can release the director without deleting the replay clip.

This prevents a stale replay camera from surviving after its evidence or review session no longer exists.

## RACE prototype controls

The STUDIO25 incident-review section adds direct buttons for:

- INCIDENT CAM;
- TRACKSIDE;
- CHASE CAM;
- HELICOPTER;
- CAM OFF.

The panel also reports the current camera mode and whether the native directed camera is active.

## Compatibility and authority

- HGAME v11 is additive; v1–v10 continue to load;
- generated StudioGameplay schema is v16;
- STUDIO23 remains the physical incident/steward evidence authority;
- STUDIO24 remains the bounded replay-history authority;
- STUDIO25 is presentation/camera authority only;
- all review camera positions remain floating-origin safe;
- replay ghosts and replay cameras remain non-physical.

## Next useful broadcast / replay depth

The next layer can build on this without replacing it: native compact full-session replay files, authored trackside camera banks and trigger volumes, automatic live-TV shot selection, flag/lap/pit timeline bookmarks, telemetry graphs, onboard camera definitions, slow motion, replay export/import and server-authoritative multiplayer replay capture.
