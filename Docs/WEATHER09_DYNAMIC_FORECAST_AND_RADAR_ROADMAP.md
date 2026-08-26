# WEATHER09 - Dynamic 24-Hour Weather and Radar Roadmap

## Status and purpose

Planned. This is the continuation contract for implementing time-varying
weather and its radar presentation. It builds on:

- `WEATHER_ROADMAP.md` for world-space rain and cloud rules;
- `WEATHER08_REGIONAL_RAIN_RADAR.md` for the regional-authority concept;
- `ADR-091-Physical-Rain-Microphysics-And-World-Precipitation-Field.md` for
  physical rain populations and deterministic representatives; and
- `Water-Laboratory.md` for camera-independent rainfall history and prebaked
  puddle reconstruction.

The target experience lets a creator describe a 24-hour tendency such as
windy, rainy and windy, stormy, torrential rain, and then clearing to sunshine.
The engine generates continuous spatial weather that approaches on the radar,
reaches the location, wets the surface and later dries it.

Gran Turismo 7 is a useful interaction reference for an understandable moving
rain map. Heritage Engine must implement its own data model, visuals and UI.

## Current repository truth

Inspect the current code before continuing. Older documents may describe an
intended or experimental feature rather than a compiled production feature.

Currently present:

- `RainMicrophysics` derives a statistical physical drop population from a
  rainfall rate in mm/h.
- `PrecipitationField` reconstructs deterministic world-space rain
  representatives using a seed, time and one configured wind vector.
- `SurfaceWeather` contains scalar rain, humidity, wind, cloud, drainage and
  evaporation inputs/state.
- Dynamic Surface and Water Laboratory contain wet-surface work, including the
  production `Prebaked Puddle Response` experiment.
- `WeatherRadarOverlay.cpp` contains an F10 radar UI prototype.

Not yet complete:

- no authoritative 24-hour weather director exists;
- generated fronts, rain cells and clearing periods are not implemented;
- `PrecipitationField` is not yet the complete sparse regional service expected
  by the radar prototype;
- the radar prototype calls regional APIs that must be implemented and added to
  the compiled/runtime path before it is complete;
- clouds are not yet driven by a full evolving regional atmosphere;
- forecast persistence, replay and multiplayer replication are not complete.

## Single-authority rule

There is one weather truth:

`forecast director -> regional atmosphere -> precipitation -> surfaces`

The following must consume that same truth:

- weather radar;
- volumetric clouds and cloud shadows;
- visible near, middle and far rain;
- wet roads and puddles;
- tire water interaction and spray;
- road and tire temperatures;
- drying and traffic-created dry lines;
- weather audio, vegetation motion and visibility.

Opening the radar, moving the camera or changing graphics quality must not
alter authoritative rainfall. Graphics may simplify or interpolate the state,
but must not own it.

## Creator-facing 24-hour weather story

A creator specifies broad intentions rather than keyframing every cloud. A
weather story is a deterministic seeded schedule of tendency segments. A
tendency is a bias, not an instantaneous hard state.

Required tendency families:

- clear and calm;
- clear and windy;
- fair with scattered clouds;
- overcast;
- intermittent showers;
- rainy and windy;
- sustained rain;
- stormy and windy;
- torrential rain (the friendly UI may say `Pissing Rain`);
- clearing;
- post-rain sunshine and drying.

Each tendency defines ranges and probabilities for continuous targets:

- rainfall rate in mm/h;
- cloud fraction and cloud-water/density proxy;
- cloud base and top altitude in metres;
- wind speed, direction, gust strength and directional variability;
- relative humidity;
- ambient temperature and pressure tendency;
- visibility and distant rain-curtain strength;
- rain-cell size, lifetime, travel direction and probability;
- optional thunder/lightning probability for later presentation.

Transitions must be smooth. A change selects new targets and evolves toward
them with separate response times. It must not instantly replace rain, cloud,
light, wind or surface state.

### Proposed scenario format

The final format may be Lua, JSON or a native module document. Preserve this
meaning:

```lua
WeatherScenario = {
    id = "nurburgring_variable_24h",
    seed = 24051984,
    durationHours = 24.0,
    loop = false,
    climateProfile = "temperate_europe",

    segments = {
        { fromHour = 0.0,  toHour = 5.0,  tendency = "clear_calm",      strength = 0.80 },
        { fromHour = 5.0,  toHour = 9.0,  tendency = "windy",           strength = 0.65 },
        { fromHour = 9.0,  toHour = 14.0, tendency = "rainy_windy",     strength = 0.75 },
        { fromHour = 14.0, toHour = 17.0, tendency = "stormy_windy",    strength = 0.90 },
        { fromHour = 17.0, toHour = 19.0, tendency = "torrential_rain", strength = 0.85 },
        { fromHour = 19.0, toHour = 21.0, tendency = "clearing",        strength = 0.80 },
        { fromHour = 21.0, toHour = 24.0, tendency = "sunny_drying",    strength = 0.75 },
    },

    constraints = {
        maximumRainfallMmPerHour = 150.0,
        maximumWindSpeedMps = 35.0,
        minimumStateDurationMinutes = 8.0,
        transitionMinutes = { minimum = 5.0, maximum = 45.0 },
    },
}
```

`strength` controls how strongly generation leans toward the tendency. It must
not simply multiply every variable. A strong shower bias, for example,
increases cell probability and intensity while retaining dry gaps.

The editor should also offer a simple mode: select tendencies and weights,
choose a duration, then press **Generate 24-Hour Forecast**. The result can be
previewed, regenerated with a new seed, edited and saved by the module.

## Reduced-order regional atmosphere

Heritage does not need CFD weather. Use a deterministic reduced-order model
suitable for racing and large free-roam maps.

### Regional field

Use a sparse world-space 2D or 2.5D grid. A practical initial scale is about
250 m per cell, with coherent features spanning kilometres. Each active or
lazily reconstructed cell stores:

- horizontal wind vector;
- gust/turbulence scalar;
- cloud fraction;
- cloud-water/condensate proxy;
- humidity;
- temperature anomaly;
- pressure tendency or front-driving scalar;
- current precipitation rate;
- accumulated precipitation;
- precipitation phase/type when later required;
- last authoritative update time.

The field covers the authored scene, not a camera bubble. Sparse storage and
analytical catch-up keep inactive distant regions cheap.

### Fronts and rain cells

The director creates bounded sources upwind of the playable area. They evolve
into fronts/cells that:

- advect with regional wind;
- grow, merge, split, weaken and dissipate deterministically;
- have soft spatial edges rather than square-cell borders;
- produce local rainfall independently of the camera;
- retain coherent motion visible on radar;
- follow the tendency envelope without repeating an obvious texture;
- allow dry gaps and bands of heavier rain within one storm.

Seeded multi-scale fields, source primitives or semi-Lagrangian advection are
all acceptable after profiling. Camera-distance fade is never simulation.

### Continuous evolution

Weather archetypes generate targets; they are not enum switches. Use critically
damped responses or monotonic curves with separate time constants for:

- wind speed and direction;
- cloud formation and dissipation;
- rainfall onset and decay;
- temperature and humidity;
- visibility;
- surface wetting and drying.

This lets a storm become visible in the distance before rain reaches the track.

## Wind roadmap

Wind becomes a shared service rather than a rain-renderer setting.

Required state:

- mean horizontal vector at reference height;
- optional low, middle and cloud-layer vectors;
- gust amplitude, frequency and coherence scale;
- terrain/urban exposure factor;
- deterministic time evolution;
- explicit meteorological `from` versus engine-velocity `toward` convention.

Consumers:

- regional cloud and rain advection;
- raindrop trajectories;
- evaporation and surface drying;
- spray, mist and smoke drift;
- vegetation and loose debris;
- weather audio;
- later aerodynamic gust loads where justified.

Wind authority may update slowly. Renderers interpolate it each frame. Visual
gust noise must not affect vehicle physics unless physics receives the same
authoritative gust sample.

## Clouds, light and visible weather

The regional atmosphere drives cloud presentation:

- cloud volume is world anchored and advected by the shared field;
- cloud cover/density continuously attenuate sunlight;
- cloud shadows travel across terrain;
- rain-bearing cloud regions correspond to radar cells;
- distant rain curtains occupy the same regions reported by radar;
- cloud base/top and storm structure respond to the current tendency;
- clearing exposes sunlight gradually and moves downwind.

High quality uses reconstructed volumetric clouds. Lower tiers may reduce ray
steps, resolution, shadow cadence or distant detail, while preserving position
and timing.

## Surface wetting, puddles and drying

Each 10 m surface tile samples local rain from the regional field. Its compact
world state accumulates rain even when detailed puddles are not visible. When
the tile enters detailed range, prebaked terrain response reconstructs where
the water belongs.

Conceptual local balance:

`new water = rain + runoff - drainage - infiltration - evaporation - tire clearing`

Drying responds to:

- air and road temperature;
- relative humidity;
- wind speed and exposure;
- solar intensity and cloud shadow;
- surface porosity and drainage;
- local depth;
- vehicle traffic and exact lines driven;
- tunnels, roofs and vegetation cover.

Thin films can vanish quickly in sun and wind. Deep puddles remain until their
stored water drains or evaporates. Frequently driven racing lines dry first and
new rain can overwhelm them again. The weather director changes inputs; it must
never directly paint the surface `wet` or `dry`.

## Full radar GUI

The radar is read-only presentation of the same regional cells used by clouds,
rain and surfaces.

Required views:

- live rainfall rate in physical mm/h;
- accumulated precipitation;
- optional cloud cover;
- optional regional wind vectors;
- recent movement history;
- short forecast based on authoritative cell motion/evolution;
- track/road outline when navigation data exists.

Required controls:

- toggleable HUD/window and module-selectable input binding;
- circuit and free-roam ranges;
- zoom, pan and recenter;
- north-up and heading-up modes;
- current-time marker;
- history/forecast time slider and play/pause;
- physical colour legend with units;
- opacity and size;
- draggable position with screen-edge clamping;
- gamepad, keyboard and mouse navigation;
- saved per-module layout and preferences.

Presentation requirements:

- player position and heading are unambiguous;
- motion is smooth between low-rate field updates;
- no square borders or pulsating tile alpha;
- colour normalization does not change every refresh;
- colours use fixed physical rainfall bands;
- dry areas remain readable;
- changing range never changes/materializes weather;
- world-to-radar orientation is tested in every quadrant;
- multiple cameras share authority but keep separate GUI state.

Suggested fixed legend, subject to visual testing:

| Band | Rainfall rate | Meaning |
|---|---:|---|
| Clear | below 0.1 mm/h | no meaningful rain |
| Blue | 0.1-1 mm/h | drizzle |
| Green | 1-4 mm/h | light rain |
| Yellow | 4-15 mm/h | moderate rain |
| Orange | 15-40 mm/h | heavy rain |
| Red | 40-80 mm/h | very heavy rain |
| Magenta | above 80 mm/h | torrential/extreme |

Fixed thresholds ensure that the same storm keeps the same colour when a
stronger cell enters radar range.

## Forecast persistence and multiplayer

Generate the forecast before or during scene loading from:

- scenario seed;
- 24-hour tendency timeline;
- climate profile;
- scene bounds/reference altitude;
- initial atmospheric state.

Persist:

- seed and generated event/source list;
- current scenario time;
- regional history checkpoints;
- accumulated precipitation;
- compact tile rain/dry-line state;
- creator edits.

On load, reproduce or analytically catch up weather independently of frame
rate. The multiplayer server owns scenario time/weather. Clients receive the
seeded forecast plus corrections and own only view-specific histories.

## Proposed code ownership

These names are proposals, not claims that the files exist:

- `Physics/Weather/WeatherDirector.*`: timeline generation/evaluation;
- `Physics/Weather/RegionalWeatherField.*`: sparse cells, fronts and history;
- `Physics/Weather/WindField.*`: deterministic wind/gust sampling;
- `Physics/Weather/WeatherScenario.*`: module schema/serialization;
- `UI/WeatherRadarOverlay.*`: immutable snapshots and GUI state only;
- existing `PrecipitationField.*`: physical representative drops from local
  regional rain/wind samples;
- existing SurfaceWeather/Dynamic Surface: local climate consumers.

Heritage Engine owns reusable mechanics. Racing United owns its climate
profiles, scenarios, styling and event rules. Another module may provide a
different weather generator through the same sampling contract.

## Milestones

### WEATHER09A - Contract and current-state audit

- synchronize documentation with compiled code;
- audit contradictory WEATHER08 completion claims;
- define units, heading convention, serialization version and seed rules;
- add the radar source to the build only after its required API exists.

Acceptance: the clean build accurately reports existing weather functionality;
no document calls an uncompiled prototype complete.

### WEATHER09B - Scenario and 24-hour director

- tendency presets and weighted timeline segments;
- deterministic continuous target curves;
- preview, regenerate, edit, save and load;
- scenario-time controls for accelerated testing.

Acceptance: identical seed/scenario gives an identical event schedule and an
accelerated 24-hour test has no discontinuous weather jumps.

### WEATHER09C - Sparse regional atmosphere

- regional cells and coherent fronts;
- advection, growth/decay and lazy catch-up;
- location-based rain replacing the global scalar;
- immutable read-only radar snapshots.

Acceptance: separated locations can receive different rain; an approaching
cell reaches them in the correct order; dormant catch-up matches continuous
observation.

### WEATHER09D - Wind field and gusts

- mean/layered wind and deterministic gusts;
- shared advection for fronts/clouds;
- rain, evaporation, vegetation/audio and later aero consumers;
- validated direction conventions.

Acceptance: radar motion, clouds and streak drift agree; camera heading never
rotates authoritative wind.

### WEATHER09E - Surface climate coupling

- local rain/humidity/temperature/wind/solar feed compact tile state;
- material-dependent evaporation, drainage and infiltration;
- traffic dry-line retention outside detailed range;
- near puddle reconstruction without whole-map live water flow.

Acceptance: driven lines dry faster; sunny/windy clearing dries faster than
humid calm weather; depressions retain water; mass is bounded/deterministic.

### WEATHER09F - Full radar GUI

- complete snapshot API and build/runtime integration;
- physical colour bands, zoom/pan/orientation and legend;
- history/forecast animation with interpolated motion;
- edge-clamped movable HUD and module styling.

Acceptance: the radar cell reaches the player when local rain begins; range
changes never alter weather; there are no seams, popping or colour pumping.

### WEATHER09G - Cloud, lighting and visibility integration

- regional cloud density, shadows and sun attenuation;
- distant rain curtains/fog aligned to radar;
- gradual arrival and clearing.

Acceptance: the storm appears in sky, shadows and radar before local rain and
then clears coherently downwind.

### WEATHER09H - Persistence, replay and multiplayer

- serialize forecast/history/surface response;
- deterministic time acceleration and save/load;
- server authority/client interpolation;
- replay event recording.

Acceptance: reload, replay and clients agree on cell positions, local rate and
accumulated water within defined tolerances.

### WEATHER09I - Performance and full-day validation

- benchmark circuits, free roam, 150 cars, mirrors and split screen;
- profile authority, radar, clouds and surface catch-up separately;
- quality tiers alter presentation cost, not rainfall correctness;
- accelerated 24-hour soak tests.

Acceptance: no unbounded state, allocation churn, pop-in or weather-dependent
simulation divergence; dormant regions remain cheap.

## Deterministic test matrix

Automated:

- same seed/timeline reproduces the forecast;
- different seeds remain within tendency constraints;
- fronts follow wind;
- dormant precipitation integrates correctly;
- transitions obey per-tick change limits;
- radar snapshots equal direct regional samples;
- colour bands remain fixed;
- wetting/drying keeps water bounded;
- dry lines persist and re-wet;
- covered areas reject direct rain;
- save/reload and replay resume identically;
- an accelerated full day remains finite/deterministic;
- extra cameras do not mutate weather.

Visual:

- scattered shower approaching and passing;
- windy dry day with fast film drying;
- rain/wind building into a storm;
- torrential cell followed by sunshine;
- radar visibility before local arrival;
- distant rain curtain aligned with radar;
- cloud shadows and sunlight returning;
- racing-line drying and re-wetting;
- circuit and free-roam radar ranges;
- no grid seams, square alpha, pulse or weather pop-in.

## Performance rules

- Regional authority updates at a physically adequate low cadence; renderers
  interpolate.
- Do not run detailed clouds/water in every regional cell.
- Use compact sparse state and analytical catch-up.
- Materialize 10 m / 256x256 water only near relevant views/vehicles.
- Radar snapshot generation is read-only and bounded.
- All cameras share weather authority.
- Reduce clouds, rain representatives, radar resolution and secondary effects
  before reducing weather correctness.
- Profile each milestone before increasing resolution/cadence.

## Future AI continuation instructions

1. Read this file completely.
2. Read `WEATHER_ROADMAP.md`, `WEATHER08_REGIONAL_RAIN_RADAR.md`,
   `Water-Laboratory.md` and the referenced ADRs.
3. Inspect the source/build; documents are not proof of implementation.
4. Check `git status` and preserve unrelated/user files.
5. Start at the first incomplete WEATHER09 milestone.
6. Keep one authority. Do not add a decorative second radar, rain field or
   camera-owned storm.
7. Build Release, run physics regressions and run `Tools/ValidateProject.ps1`.
8. Let the user test visually before Git operations unless explicitly asked.

Recommended request:

> Continue the first incomplete milestone in
> `Docs/WEATHER09_DYNAMIC_FORECAST_AND_RADAR_ROADMAP.md`. Inspect the current
> implementation before editing, preserve the single weather authority and do
> not commit or push until I have visually tested it.
