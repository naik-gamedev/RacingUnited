# Vehicle Audio Architecture

## Purpose

Heritage Engine vehicle audio is a native, module-authored subsystem. The engine owns playback, spatialization, voice lifetime and performance budgets. A module owns the acoustic identity of each vehicle through Lua data and may replace that definition without changing engine code.

The first implementation deliberately avoids a single pitch-shifted engine loop. It produces seven independently controlled continuous layers:

- exhaust pressure pulses
- intake resonance
- engine mechanical noise
- transmission and differential whine
- tire rolling, scrub and wet-surface noise
- aerodynamic wind
- suspension and chassis impacts

These procedural signals are the initial audible fallback implementation. They prove the architecture and respond to live physics, but they are not a claim that the Peugeot 206 RC is already acoustically authentic. The Racing United prototype now uses a hybrid provider: a provenance-recorded CC0 startup transient and seven neighboring RPM loops from one real engine-contact session are crossfaded over a quiet procedural support layer. A separate bounded event bank responds to gear changes, suspension impulses, the rev limiter and lift-off overrun. A measured Peugeot engine-order bank can replace this documented proxy behind the same runtime interface.

## Runtime data flow

```text
Lua vehicle definition
        |
        v
VehicleAudioRuntime <--- PhysicsWorld telemetry
        |
        +--> engine model ------> exhaust / intake / mechanical
        +--> drivetrain model --> transmission
        +--> tire model --------> rolling / scrub / wetness
        +--> aerodynamic model -> wind
        +--> chassis model -----> suspension / impacts
        +--> event bank --------> shifts / discrete suspension impulses
        |
        v
AudioSystem ---> XAudio2 voices ---> operating-system audio device
```

The renderer publishes the listener position, orientation and velocity once per frame. Every vehicle publishes emitter positions and velocities. The audio backend applies distance attenuation, stereo panning, Doppler shift and low-pass filtering without coupling audio behavior to rendering code.

For the closest 20 audible vehicles, the runtime also traces deterministic
geometry paths through the authoritative collision scene. Each update tests the
direct source/listener ray and bounded image-source reflection candidates
against real scene surfaces. Surface material controls obstruction transmission
and reflected energy; path length controls arrival delay. Three native XAudio2
reverb buses reconstruct short, medium and long reflected arrivals while the
direct signal remains independently panned, attenuated and filtered. Results
are smoothed between 20 Hz path solutions, and vehicle trace phases are
staggered so the 20 sources do not all query collision geometry in one frame.

This is deliberately a bounded real geometric-acoustics implementation, not a
claim to solve unrestricted wave acoustics. It currently models line-of-sight
occlusion and first-order specular reflections. Diffraction, multi-bounce path
sampling, authored cabin transmission surfaces and a headphone binaural
decoder can extend the same propagation contract later.

## Source layout

```text
Engine/HeritageEngine/Audio/
  AudioSystem.hpp/.cpp                 playback, buses, generated clips, listener and emitters
  Acoustics/
    AcousticPathTracer.hpp/.cpp        direct and reflected collision-geometry paths
  Vehicles/
    VehicleAudioTypes.hpp              definitions, telemetry and public handles
    VehicleAudioRuntime.hpp/.cpp       native vehicle/physics/Lua bridge
    VehicleAudioSynthesis.hpp/.cpp     bounded, seamless procedural source generation
    Models/
      EngineAudioModel.hpp/.cpp
      EngineSampleBankModel.hpp/.cpp
      VehicleAudioEventModel.hpp/.cpp
      VehicleAudioFleetBudgetModel.hpp/.cpp
      DrivetrainAudioModel.hpp/.cpp
      TireAudioModel.hpp/.cpp
      AerodynamicAudioModel.hpp/.cpp
      ChassisAudioModel.hpp/.cpp
  Weather/
    WeatherAudioTypes.hpp
    WeatherAudioRuntime.hpp/.cpp        native SurfaceWorld/audio bridge
    Models/WeatherAudioModel.hpp/.cpp   bounded rain/wind mix and smoothing

Modules/RacingUnited/Scripts/Vehicles/
  Audio.lua                            module lifecycle and persistent enable state
  Definitions/Peugeot206RC/
    AudioDefinition.lua                authored Peugeot acoustic topology and gains

Modules/RacingUnited/Scripts/UI/Vehicle/
  AudioLabPanel.lua                    live definition and telemetry diagnostics

Modules/RacingUnited/Scripts/Audio/
  WeatherDefinition.lua               module-owned rain/wind sound bank
  Weather.lua                         native weather-audio lifecycle

Modules/RacingUnited/Assets/Audio/ThirdParty/
  Freesound/MiniCooperSContactBank/  conditioned CC0 real-engine proxy bank
  OpenGameArt/RacingCarEngineLoops/   superseded generic CC0 reference bank
  OpenGameArt/Weather/                CC0 rain-intensity and wind loops
  Kenney/                              CC0 impact and interface packs

ThirdParty/stb/stb_vorbis.c            MIT/public-domain OGG/Vorbis decoder
```

Touch, switch, control and cockpit-detail sounds should remain separate event subsystems. For example, steering-wheel leather movement, indicator stalks, switches, wipers, rain strikes, gravel strikes and body creaks should not be baked into the continuous engine model. They can eventually occupy focused files under `Audio/Interactions`, `Audio/Weather`, `Audio/Impacts` and module-owned Lua definitions.

## Lua vehicle definition

`Audio.CreateVehicleSound(vehicleHandle, definition)` creates a sound instance for a native physics vehicle. The definition supports:

- stable definition ID and vehicle category
- cylinder count and crank revolutions per combustion cycle
- idle, reference and redline RPM
- maximum torque reference
- independent layer gains
- local exhaust, intake, engine, transmission and cabin emitter positions
- cabin listening radius
- full, reduced and crowd LOD distances
- an optional startup transient and bounded RPM-indexed engine-loop bank
- an authored procedural support gain, with automatic full fallback when the
  recorded bank is unavailable
- bounded event banks for gear changes and light/heavy suspension impacts

The current Peugeot definition is in `Modules/RacingUnited/Scripts/Vehicles/Definitions/Peugeot206RC/AudioDefinition.lua`. Other modules may author inline engines, V engines, flat engines, rotaries, two-strokes, motorcycles, electric motors, multiple power units or entirely custom sound providers. Cylinder count and cycle length are data, not hard-coded assumptions about cars.

Available native calls:

```lua
local handle = Audio.CreateVehicleSound(vehicleHandle, definition)
Audio.SetVehicleSoundEnabled(handle, true)
local state = Audio.GetVehicleSoundState(handle)
local runtimeStats = Audio.GetRuntimeStats()
Audio.DestroyVehicleSound(handle)
```

`GetVehicleSoundState` exposes the useful diagnostics: RPM, engine load, speed, aggregate tire slip, suspension activity, gear, interior/exterior state, listener distance, detail level, active continuous layers, sample voices, event voices, traced rays, valid reflection paths, direct obstruction gain and reflection delay.

`GetRuntimeStats` exposes the backend-wide active native voice count and decoded
clip-cache memory for Audio Lab and 150-car performance verification. Decoded
PCM is retained in a bounded 256 MiB least-recently-used cache; idle entries
are evicted under pressure so LOD transitions do not repeatedly decode the
same OGG/WAV loops.

## Detail levels and large fields

The definition uses three distance bands, then a deterministic nearest-first
fleet allocator applies hard native voice caps:

- Full: individual exhaust, intake, mechanical, drivetrain, tire, wind and chassis layers.
- Reduced: mechanically minor layers are suppressed and the mix is simplified.
- Crowd: only the strongest identifying energy remains.
- Beyond the crowd range: the instance is silent while its lightweight runtime identity remains valid.

Continuous XAudio2 voices are now physically created and destroyed with the
assigned detail level; silent layers are not merely left running at zero gain.
The current fleet budget allows at most 192 continuous/sample voices across
eight full-detail, 24 reduced-detail and up to 48 crowd candidates, with a
separate 48-voice transient ceiling. The regression exercises a 150-vehicle
candidate field and verifies every cap. A future crowd renderer should
aggregate distant competitors into packs and add race-context importance on
top of the present deterministic distance ordering.

Geometry propagation has a separate hard budget: the nearest 20 audible
vehicles receive direct/reflected path tracing at 20 Hz. Other audible vehicles
keep the fleet renderer's ordinary distance attenuation, stereo pan and
Doppler. This makes the user-requested 20-car acoustic field independent of a
larger 150-car race field instead of multiplying ray work by every entrant.

## Interior and exterior sound

The runtime detects whether the listener is inside the authored cabin radius. Interior mode changes layer balance and acoustic openness; it does not load a second unrelated sound system. Later work should add authored firewall, glass, door, roof and floor transmission paths, plus window and door state.

## Peugeot 206 RC acoustic baseline

The Racing United module's first authored engine is the naturally aspirated
EW10J4S inline four. Factory material identifies a 1,997 cm3 engine, 130 kW at
7,000 rpm, 202 Nm at 4,750 rpm, a usable maximum of 7,300 rpm, VVT, a dual-mode
intake with a Helmholtz resonator, a four-into-one tubular header, and a large
rear silencer. References:

- [Peugeot 206 RC press dossier (archived copy)](https://fr.scribd.com/document/599462975/206-Dossier-Presse-2)
- [Peugeot UK 206 GTi 180 announcement](https://www.peugeotpress.co.uk/releases/843)
- [Peugeot Australia 206 GTi 180 brochure](https://xr793.com/wp-content/uploads/2023/07/2006-Peugeot-206-206-CC-AUS.pdf)

Those facts now drive the native firing-frequency, engine-order, pulse-shape,
high-RPM intake transition and load model. The compression ratio and acoustic
shape values are authoring inputs and are deliberately labelled as tuning data.
They make the prototype structurally resemble the car; they do not turn the
documented CC0 Mini Cooper S contact recording into a measured 206 RC sample
set. The checked-in loops are real engine material from one internally
consistent recording, while the startup is a separate CC0 Fiat Punto event.
Their exact provenance and processing are recorded beside the assets. When a
legally redistributable measured Peugeot bank is available, Lua can replace the
sample paths without changing native code or physics.

## Native weather ambience

Weather audio reads the same authoritative `SurfaceWorld` precipitation rate
and wind speed used by the physical road/weather simulation. Four neighboring
rain recordings use logarithmic intensity mapping and equal-power crossfades,
which preserves drizzle detail without playing every rain layer at full volume.
The wind layer starts gradually above 1.5 m/s, reaches its authored reference
near 24 m/s and receives a small bounded pitch change as speed increases. A
1.25-second attack and 3.5-second release prevent abrupt changes when the live
weather GUI sliders move. The module owns only the CC0 files and reference
gains; native code owns playback, smoothing and lifetime.

## Planned development

1. Calibrate the engine-order baseline against legally redistributable measured Peugeot 206 RC recordings when such a bank is available.
2. Extend the implemented firing-order and resonance model with authored exhaust runner lengths and measured transfer functions.
3. Add a granular provider and measured engine-order bank while retaining the current hybrid RPM sample provider and procedural fallback.
4. Split four tire emitters and consume per-wheel surface material, longitudinal slip, lateral slip, load, temperature, water and contamination.
5. Extend the implemented gear-change, rev-limiter and lift-off overrun events with gear whine per ratio, differential whine, clutch engagement, ignition, starter and shutdown transients.
6. Extend the implemented direct obstruction and first-order exhaust reflections with diffraction, bounded second-order paths, explicit tunnel/reverb zones and propagation-delay voices where perceptually valuable.
7. Add authored cockpit transmission paths and event libraries for switches, controls, trim, leather, rain, wipers and loose components.
8. Add collision, scrape, suspension-top-out, bottom-out, deformation and damage event voices.
9. Add motorcycle rider/body wind, chain, sequential gearbox, intake and exhaust placement models.
10. Extend the implemented fleet voice allocator and repeatable 150-vehicle budget regression with a real-time mixed-output performance capture.

## Validation rules

- Continuous sources must be seamless and finite.
- Generated floating-point samples must remain inside `[-1, 1]`.
- Runtime pitch and volume changes must be bounded.
- Physics supplies authoritative state; audio never modifies vehicle physics.
- Module reload destroys all module-owned voices.
- An absent or disabled audio device must not prevent the simulation from running.
- Distant audio degradation must be independent from visual LOD.

`VehicleAudioRegression.cpp` checks generated-layer sample counts, peaks, energy and basic response to load, RPM, tire slip, speed, interior filtering and LOD.
