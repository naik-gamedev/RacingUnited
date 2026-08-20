# ADR-082 — World-Space Visible Rain and Natural Gear Ordering

## Status
Accepted for WEATHER06F / INPUT03A.

## Context
WEATHER06C proved that a late transparent fullscreen rain draw is visible on the
live OpenGL/MSAA path, but its screen-space streak field travelled with the
camera. WEATHER06D/E removed that veil and introduced per-drop cover rejection,
but live testing then showed no visible rain at all. The near instanced lattice
therefore cannot be the only visibility safety net yet, and the cover rejection
must not be allowed to classify an entire steep/layered LiDAR view as sheltered.

Input Settings also exposed direct gears lexicographically because the generic
action list is name-sorted: Gear 1, Gear 10 ... Gear 2. A shifter page needs
physical/natural order instead.

## Decision
Rain keeps the explicit world-cell instanced near lattice and adds a second
bounded mid/far presentation tier executed on a fullscreen quad. The fullscreen
quad is only an execution surface: its fragment shader reconstructs the camera
ray and evaluates precipitation columns at wrapped **world-space positions** at
several depth shells. Camera translation therefore changes which world rain is
sampled instead of translating a screen texture with the vehicle.

The previous per-drop cover-texture rejection is removed from the live streak
shader. Until resolved scene depth is available to the late transparent pass,
bridge/roof/tunnel suppression uses a conservative tiny layered-hydrology query
around the camera. A clearly higher upward-facing surface marks the camera as
sheltered and suppresses the local rain presentation. Authoritative hydrology
rain exposure beneath layered cover remains unchanged.

The `Gears` Settings category receives a presentation-only natural order:
Shift Up, Shift Down, Neutral, Reverse, Gear 1 through Gear 24. Internal action
keys remain `Select Neutral` / `Select Reverse` for save compatibility, but the
Settings labels are shown as `Neutral` / `Reverse`.

## Consequences
- Rain has a guaranteed-visible OpenGL path without reintroducing a camera-space
  precipitation texture.
- The mid/far field is deterministic in world coordinates and uses five bounded
  depth shells rather than CPU rain particles.
- Shelter handling is intentionally conservative until scene-depth sampling is
  added; while directly under cover, nearby visible rain is suppressed as a
  whole rather than clipped per drop.
- Input persistence/action semantics do not change; only the Gears UI ordering
  and labels change.
