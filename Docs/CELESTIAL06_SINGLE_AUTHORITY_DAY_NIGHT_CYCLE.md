# CELESTIAL06 — Single-authority day/night cycle

## Problem
The astronomical clock itself was continuous, but presentation still had
multiple independently thresholded notions of day, twilight and night:
EnvironmentSystem exposure/colour envelopes, regional weather/fog raw Sun-Y
thresholds, material ambient/wet-reflection Sun-Y thresholds, and a separate
PBSKY handoff. In addition, the cloud temporal buffer stores resolved
scene+cloud RGB with very strong history; accelerated time could retain a
previous lighting state and intermittently re-accept it during twilight.

This produced the observed dawn/dusk behavior where frames could appear to
alternate between day and night even though local astronomical time advanced
monotonically.

## Reference
CELESTIAL06 adapts the control architecture of EnricoMonese/DayNightCycle
(MIT): one clamped Sun/up dot is the source parameter and every effect evaluates
its own continuous curve/gradient from that same parameter.

Heritage keeps its real geographic/date astronomy rather than replacing the
Sun orbit with Unity's simple 360-degree rotation. The adapted scalar is:

`dayNightCycle = clamp((solarElevation - (-0.20)) / (1 - (-0.20)), 0, 1)`

The -0.20 default is intentionally the reference project's default `minPoint`,
so twilight begins before geometric sunrise and ends after sunset.

## Runtime contract
- `EnvironmentLighting::daylightFactor` is now the single normalized
  day/night-cycle authority.
- Sky colour/exposure, atmosphere thickness, direct Sun intensity, Moon scene
  contribution and star fade are continuous curves of that value.
- Regional weather/fog no longer independently thresholds raw Sun elevation.
- Entity materials receive `uDayNightCycle`, not `uSolarElevation`, for ambient
  and wet-reflection night scaling.
- PBSKY's authored/physical handoff uses `uDaylightFactor`, removing another
  independent day/night threshold.
- Stars start appearing while the Sun is still close to the orange horizon.
- The cloud TAA resolve receives the per-frame day/night-cycle delta. When time
  moves quickly, stale resolved day/night RGB loses history authority; time
  jumps reject that history immediately.
- Physical Sun/Moon directions still control where the celestial bodies,
  direct illumination and shadows come from. They no longer define separate
  presentation states.

## Intentionally retained
- Real Sun/Moon astronomy and geographic scene metadata.
- Separate physical Sun and Moon cloud-scattering channels.
- Existing Moon neutral-colour fix near the horizon.
- Existing Dynamic Surface, tire, LIVETRACK and cloud-density behavior.
