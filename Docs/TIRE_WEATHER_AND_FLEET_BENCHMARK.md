# Tire Weather and Fleet Workload Laboratory

## What is implemented

`SurfaceWeather` is a deterministic world-scale liquid-weather baseline. It advances:

- precipitation in millimetres per hour;
- a bounded hard-surface water-film depth;
- depth-dependent drainage;
- humidity-, temperature- and wind-dependent evaporation;
- cloud/water/rain-influenced road temperature;
- cumulative rain, drainage, evaporation and overflow diagnostics.

`SurfaceWorld::localConditions` combines that weather state with scene-authored wetness. Hard-surface
tire contacts receive the explicit water depth rather than reconstructing it from normalized wetness.
The depth is sampled across the adaptive footprint and reaches the wet-tire drainage, hydrodynamic
lift, hydroplaning, rolling-loss and heat-transfer calculations. Wind also reaches tire convection.

`SurfaceHydrology` replaces the global film as the authoritative local depth wherever a static scene
has been baked. It derives elevation and connectivity automatically from collision triangles, resolves
pooling, runoff, infiltration, drainage and evaporation at 30 Hz, and lets the actual swept tire
footprints displace water. See `SURFACE_HYDROLOGY.md` for the cache and authoring contract.

The Racing United Surfaces panel provides Dry, Light Rain, Heavy Rain and Storm presets plus live
rain, humidity, wind and cloud controls. A reset clears accumulated water without changing the
chosen weather configuration.

## 150-car workload laboratory

The Tire Lab can execute a timed, single-threaded workload using the tire fitted to the spawned
vehicle. The default case runs 150 vehicles / 600 tires at a 1000 Hz tire force rate, with thermal,
wear and wet-state work at 100 Hz. Four player tires use the bounded 3x3 distributed contact patch;
the remaining tires use the same whole-tire physical provider at aggregate contact fidelity.

Results include wall-clock time, simulated time, real-time factor, tire evaluations per second,
microseconds per vehicle per 1 ms step, state-update counts and a force checksum. Wet mode also builds
a synthetic spatial road, advances its 30 Hz hydrology and submits all 600 tires' 1000 Hz swept-contact
water-clearing work. Hydrology cell, step and contact counts are reported separately.

This is an executable tire-stack benchmark, not a complete race benchmark. It deliberately excludes:

- chassis rigid bodies and collision broadphase/narrowphase;
- suspension and drivetrain work;
- AI and race rules;
- rendering and tire visual deformation;
- audio, networking and streaming.

Those exclusions make the result useful for isolating tire cost, but a real 150-car scene profile is
still required before claiming full-grid performance.

## Deliberately still open

- spatial variation in rainfall itself (water depth and flow are already spatial);
- authored storm-drain networks beyond per-surface drainage metadata;
- snow accumulation, compaction, ice formation and melt;
- vehicle spray influencing following vehicles;
- a complete 150-car CPU/GPU/AI/render/audio/network profile.
