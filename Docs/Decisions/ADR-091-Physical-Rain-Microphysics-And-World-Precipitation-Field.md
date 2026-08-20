# ADR-091 — Physical Rain Microphysics and World Precipitation Field

## Status

Accepted for WEATHER07A.

## Context

WEATHER06 proved that Heritage can render precipitation, darken storm lighting,
feed authoritative hydrology and classify direct-rain cover. It also exposed a
bad architectural shortcut: presentation code invented visually convenient rain
motion (including 22–38 m/s fall speeds and a hard-coded wind heading) instead of
consuming a physical precipitation state.

The next weather generation must separate three different quantities:

1. **authoritative rainfall mass** — millimetres/hour delivered to hydrology;
2. **physical/statistical raindrop population** — sizes, masses and trajectories;
3. **bounded per-view presentation representatives** — whatever number of drops
   the current GPU budget can afford.

One rendered streak must never imply one real raindrop. Rendering quality may
change without changing rainfall mass, puddle growth, drainage or tire-water
physics.

## Decision

Heritage now owns reusable rain physics under `Physics/Weather`.

### `RainMicrophysics`

`RainDropPopulation` is built from the same rainfall rate used by
`SurfaceHydrology`.

The first physically grounded baseline uses the Marshall–Palmer exponential
raindrop-size distribution shape:

`N(D) = N0 exp(-lambda D)`

with the common `lambda = 4.1 R^-0.21` relationship for rainfall rate `R` in
mm/h. The distribution is numerically integrated over a bounded ordinary-rain
range. Heritage then rescales its concentration so the integrated liquid-water
flux matches the requested rainfall rate exactly. The empirical distribution
therefore shapes the drop population but can never create or destroy rain mass.

For ordinary rain drops at and above 0.5 mm, terminal velocity uses the
Atlas/Srivastava/Sekhon approximation:

`v(D) = 9.65 - 10.3 exp(-0.6 D)` m/s.

The approximation naturally approaches the observed roughly 10 m/s upper range
for large stable raindrops. A continuous conservative small-drop continuation is
used below 0.5 mm until a later microphysics tier adds Beard-style atmospheric
density/viscosity correction for drizzle/cloud drops.

### `PrecipitationField`

`PrecipitationField` is one world-owned deterministic statistical field. A rain
representative is reconstructed from:

- world cell X/Y/Z;
- lane/index within that cell;
- a stable weather seed;
- physical rain population;
- continuous world precipitation time;
- explicit world wind speed and heading.

The camera never owns drop identity. A view chooses which world cells to render;
it does not translate the storm with itself.

Resetting accumulated road-water state does not rewind this precipitation clock, so a hydrology reset cannot visibly teleport the storm.

The field stores no millions-of-particles CPU array. Representative positions
and velocities are reproducible from their identity and time, allowing future
OpenGL compute/SSBO presentation to generate the same nearby field on the GPU.

### Wind convention

Surface weather now contains an explicit `windDirectionDegrees` value. In the
current engine convention this is a **velocity-to** heading:

- 0° = world +Z;
- 90° = world +X.

This removes the old renderer-owned `vec2(0.72, 0.69)` direction. A later
atmosphere/editor layer may additionally expose meteorological “wind from”
notation, but the engine-internal velocity convention stays unambiguous.

### Existing renderer during WEATHER07A

WEATHER07A is a physics/foundation milestone, not the final visual rain pass.
The existing OpenGL renderer is retained only as an acceptance/debug consumer,
but it now receives physical wind and representative terminal speed from
`PrecipitationField`. Its ordinary triangle fallback samples deterministic world
representatives directly.

Streak length is presentation exposure/motion blur and is explicitly separate
from physical fall speed. The renderer may make a 9 m/s drop appear as a longer
streak without pretending the drop actually falls at 30 m/s.

## Consequences

- Hydrology rainfall mass remains authoritative and unchanged by graphics level.
- Heavy rain changes the statistical drop-size population rather than simply
  speeding every streak up.
- All views can sample one coherent precipitation field.
- Explicit wind heading is available to rain, spray, vegetation and later cloud
  advection.
- WEATHER07B can replace the ugly WEATHER06 geometry with textured/optical GPU
  rain without redesigning precipitation physics.
- WEATHER07C impacts can carry real diameter, mass, velocity and impact energy.

## Deliberate limits of WEATHER07A

- Marshall–Palmer is a robust baseline distribution, not a claim that every
  storm on Earth follows one universal DSD. Future weather regimes may select
  gamma/lognormal/measured distributions.
- Air density, pressure, temperature and evaporation do not yet alter individual
  drop fall speed/diameter.
- Collision/impact events are not emitted yet; hydrology still receives bulk
  rainfall directly.
- No new rain texture is introduced in 07A. The user-supplied optical texture
  work belongs to WEATHER07B.

## Scientific references used for the baseline

- Marshall, J. S. and Palmer, W. McK. (1948), *The Distribution of Raindrops
  With Size*, Journal of Meteorology.
- Atlas, D., Srivastava, R. C. and Sekhon, R. S. (1973), *Doppler Radar
  Characteristics of Precipitation at Vertical Incidence*.
- NASA Global Precipitation Measurement guidance on size-dependent raindrop
  terminal speed and the approximately 10 m/s large-drop upper range.
