# ADR-079 — No implicit analogue deadzones

## Status
Accepted for INPUT07.

## Context
Racing wheels and calibrated pedals can provide useful motion immediately away
from their released/center positions. Device-class defaults such as a 2% wheel
axis deadzone discard that information and make steering onset visibly discrete.
The engine also exposes deadzone controls to the user, so a second hidden policy
is contradictory.

## Decision
Heritage Engine analogue bindings default to zero inner deadzone and zero outer
deadzone regardless of device class. Explicit values stored in a user/profile
remain authoritative and are not overwritten. Resetting a binding returns to
0/0 rather than a device-specific deadzone.

Windows DirectInput is likewise requested with `DIPROP_DEADZONE = 0`; this is
not a gameplay deadzone but a request that the driver deliver the axis without
DirectInput filtering.

INPUT08 extends the same rule through the vehicle sleep/steering boundary. A
parked vehicle may not ignore a small requested steering angle merely to remain
asleep, may not declare steering settled while a non-zero requested/current
angle error remains, and the steering-rate selector treats only an exact zero
command as "returning to center". These are simulation scheduling rules, not
places to hide another steering deadzone.

## Consequences
- A calibrated wheel can produce steering response from its first reportable
  non-zero movement.
- Users who need stick-drift or pedal noise filtering must choose a deadzone in
  Input Settings.
- There is one visible/source-of-truth deadzone policy: the saved per-binding
  analogue settings.
- Hardware/sample quantization may still create a tiny first reportable step.
  Increasing floating-point width cannot recover information absent from the
  controller report; optional smoothing/reconstruction must remain a separate,
  explicit control.
- With zero user deadzone, even a sub-0.01-degree road-wheel command is allowed
  to wake/update parked steering. If the physical wheel jitters, the user may
  choose an explicit deadzone rather than the engine silently suppressing it.
