# ADR-090 — Zero hidden DirectInput deadzone

## Status
Accepted for INPUT09. Supersedes the live-axis noise-deadzone portion of ADR-085/ADR-086.

## Context
Input analogue settings already define the user-owned inner and outer deadzones. INPUT07 removed automatic analogue deadzones from those settings, but the Windows DirectInput backend still contained an independent `kNeutralNoiseDeadzone = 0.015f` guard inside directional-axis evaluation. Consequently any movement within 0.015 of the calibrated rest value was forced to zero before the settings UI ever saw it.

The live Input Settings graph exposed the problem directly: with both visible deadzones at 0.000, the first non-zero steering raw value was approximately 0.015.

## Decision
Remove the backend live-axis deadzone completely. DirectInput axis rest calibration remains responsible only for locating the rest/neutral position and for rejecting the physically meaningless direction of endpoint-resting pedals. It must not suppress small movement away from rest.

The sole intentional deadzone authority is the per-binding analogue configuration chosen by the user/profile.

## Consequences
- Steering can propagate any representable DirectInput movement away from centre.
- Pedal rest calibration and opposite-direction endpoint protection remain intact.
- Device jitter is no longer silently hidden; if a user wants a deadzone, they set one explicitly.
- A validator prevents reintroduction of `kNeutralNoiseDeadzone`.
