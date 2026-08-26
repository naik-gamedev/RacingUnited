# CLOUDURP15N — Twilight Lighting Continuity + Celestial Cloud Colour

## Problems observed

During accelerated sunset/sunrise the scene could still look as if unrelated lights were appearing and disappearing. Two causes remained after CLOUDURP15M:

1. The renderer's legacy single celestial key direction was a normalized linear blend between Sun and Moon directions. Near dawn/dusk those bodies can be close to opposite directions, so the blend can collapse toward a near-zero vector and then swing/flip as their power ratio crosses the midpoint.
2. Volumetric clouds were illuminated from that synthetic Sun/Moon key and took most of their ambient colour from the asynchronously refreshed environment cubemap. Consequently the sky/horizon could already be orange while the cloud body was still blue-grey. At 240x time scale the staged IBL snapshots could also be visibly behind the current sky.

The atmosphere also had multiple independently shaped twilight/warmth bands. They were mathematically continuous but their overlapping rise/fall could read as colour pulses when time was accelerated.

## Changes

- Replaced linear Sun/Moon direction blending with a robust spherical/great-circle interpolation, including a deterministic near-antipodal path.
- Volumetric clouds now receive the **actual astronomical Sun direction/colour/intensity** rather than the synthetic shared key.
- Moonlight remains as a separate cool cloud fill using the existing CLOUDURP15L2 0.50 scene-illumination scale.
- Cloud ambient lighting now follows the current `skyHorizon` / `skyZenith` colours directly (82% live authority, 18% cubemap context), so cloud colour tracks the sky every frame.
- Twilight cloud fill is derived primarily from the current warm horizon + Sun colour and is stronger near low Sun angles.
- Cloud shadow projection is Sun-owned again; Moon/key interpolation can no longer rotate the weather shadow field.
- Replaced separate twilight/warm-horizon timing bands with one broad low-Sun warmth envelope spanning astronomical twilight through low daylight.
- Ground horizon ambient receives a small matching warm component so the world and sky do not diverge during the transition.
- Environment cubemap staging retains the one-face-per-frame performance architecture, but its refresh threshold/cadence becomes denser only during twilight to reduce visible IBL stepping at accelerated time scales.

## Preserved

- CLOUDURP15L2 half-strength Moon scene illumination.
- Moon disc atmosphere/haze and post-tone-map rendering.
- CLOUDURP15K position-stable cloud field.
- CAM10 detached-camera cursor/UI and handbrake persistence.
- Existing rain, regional weather, cloud TAA, hydrology and LIVETRACK presentation.

## Local checks

- `EnvironmentSystem.cpp` compiled as C++20.
- `SkyRenderer.cpp` and `EnvironmentMap.cpp` passed C++20 syntax compilation against an OpenGL declaration stub.
- A 1-second dawn scan at the Racing United reference location retained small continuous steps: max key-light angular movement about 0.028 degrees/second, max key intensity step about 0.00033, max sky-exposure step about 0.00027.
