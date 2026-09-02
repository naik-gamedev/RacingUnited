# CELESTIAL05 — Solar Twilight Continuity

## Runtime faults found

The renderer still had a hidden day/night authority outside `EnvironmentSystem`.
`EntityMeshRenderer` uploads `lighting.keyLightDirection` through the historical
`uSunDirection` material uniform. That key is intentionally a continuous blend
between the physical Sun and Moon so the legacy single-directional-light path can
cover both bodies. The material shader then incorrectly used `uSunDirection.y`
to decide whether ambient IBL and wet-surface reflections were in deep night.

That meant Moon altitude could masquerade as solar altitude. On the 2026-08-24
Ivarcko fallback date, the old material test read essentially **daytime from
20:30 through midnight** because the Moon was above the horizon even though the
Sun was already far below it. As the Moon later approached the horizon, ambient
lighting darkened as though another sunset were happening. Around Sun/Moon
handoffs the synthetic key direction could also move non-monotonically, making
morning/evening transitions appear to alternate between day and night.

Two independent presentation issues amplified the transition:

- the physical sky LUT could become effectively black once solar direct energy
  reached zero while the star map was still held off until a later threshold;
- low-altitude lunar atmospheric RGB transmission was multiplied directly into
  cloud radiance, allowing Moon-lit clouds and the Moon halo to acquire a
  sunset-like red/orange cast.

## Fix

- Added a dedicated `uSolarElevation` material uniform sourced only from
  `lighting.sunDirection.y`.
- Ambient IBL/night attenuation and dynamic-water reflection darkness now use
  real solar elevation, never the blended celestial key direction.
- The blended key remains responsible only for directional Sun/Moon lighting
  and the shared cloud-shadow transport it was designed for.
- The physical sky now crossfades continuously into the existing authored
  twilight/night sky floor from `sunY=-0.035` to `-0.12`, removing the black
  handoff gap at both dusk and dawn.
- Star-map visibility begins during late orange twilight (`sunY=-0.02`) and
  reaches full night visibility by `sunY=-0.20`; atmospheric transmission still
  naturally hides dim stars until contrast is sufficient.
- Lunar atmospheric transmission now attenuates cloud-light **brightness only**.
  It no longer spectrally reddens the cloud volume near Moonset.
- The Moon halo remains neutral/cool at low altitude instead of blending toward
  the old orange sunset tint.
- Cloud ambient uses 82% live current-frame sky colour again, reducing dependence
  on the asynchronously refreshed environment cubemap during fast time-of-day
  transitions.

## Preserved

- Astronomical Sun/Moon positions and geographic/calendar sidereal rotation.
- One continuous shared legacy key light for scene directional lighting.
- Separate physical Sun and Moon channels inside volumetric clouds.
- Existing Moon scene-illumination scale, cloud shadows, cloud TAA, regional
  weather, hydrology, LIVETRACK and tire systems.
