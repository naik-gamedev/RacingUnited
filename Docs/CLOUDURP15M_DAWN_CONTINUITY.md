# CLOUDURP15M — Dawn / Dusk Continuity

## Problem

The astronomical day/night path still contained two hard state changes during twilight:

1. `skyExposure` was clamped to 0.055 whenever `deepNight > 0`. Because that condition was boolean, exposure remained pinned until the Sun crossed exactly `sunY = -0.03`, then jumped immediately to the uncapped twilight exposure.
2. The scene's single directional key light switched abruptly between Moon and Sun using threshold tests. The regional-weather renderer had a second independent threshold (`daylightFactor < 0.22`). This could replace a still-useful Moon key with a much weaker dawn Sun and create a temporary dark trough.

The Moon ambient lift was also attached to the old key-light branch, so it could disappear at the same handoff.

## Fix

- Night exposure hold now blends out continuously from astronomical night into dawn/dusk instead of using a boolean clamp.
- Added `resolveCelestialKeyLight(EnvironmentLighting&)` as the shared astronomical key-light resolver.
- Sun and Moon scene power overlap continuously; the renderer no longer picks one with unrelated thresholds.
- Direction and colour cross-fade according to relative celestial power while total key energy remains continuous.
- Regional weather re-runs the same shared resolver after weather attenuation instead of owning a second dawn switch.
- Moon sky/ground lift now scales continuously with actual Moon visibility/daylight rather than appearing/disappearing with key ownership.

## Continuity check

At the Racing United fallback location/date (46.50619924 N, 14.97089803 E, 2026-08-24), sampling dawn once per simulated second gave:

- old maximum one-second `skyExposure` step: about **0.1673**
- CLOUDURP15M maximum one-second `skyExposure` step: about **0.000265**

A year-wide dawn scan at the same location gave:

- old maximum one-second celestial key step: about **0.0795**
- CLOUDURP15M maximum one-second celestial key step: about **0.000326**

This removes the artificial black/flat interval and the sudden lighting handoff while retaining the existing astronomical Sun/Moon positions, CLOUDURP15L2 half-strength Moon illumination, cloud lighting, regional weather, CAM09 and LIVETRACK systems.
