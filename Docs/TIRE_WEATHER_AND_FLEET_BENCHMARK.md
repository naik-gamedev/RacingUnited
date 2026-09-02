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
chosen weather configuration. Production spatial water is GPU-owned; the CPU tire benchmark does
not instantiate a second hydrology authority.

## 150-car workload laboratory

The Tire Lab can execute a timed, single-threaded workload using the tire fitted to the spawned
vehicle. The default case runs 150 vehicles / 600 tires at a 1000 Hz tire force rate, with thermal,
wear and wet-state work at 100 Hz. TIRE46 requires the bounded 3x3 distributed contact patch for all
600 tires. Aggregate contact remains an explicit diagnostic mode only and is never selected merely
because a vehicle is AI-controlled or outside the camera range.

Results include wall-clock time, simulated time, real-time factor, tire evaluations per second,
microseconds per vehicle per 1 ms step, state-update counts and a force checksum. Wet mode exercises
the tire's wet-film/drainage/hydroplaning state. Spatial GPU water is profiled in the renderer runtime;
the compatibility hydrology counters in this CPU-only benchmark therefore remain zero.

The 2026-08-31 Windows Release regression measured its 50 ms / 150-car case at 58.16 ms dry and
59.33 ms wet, or 0.86x and 0.84x real time on one CPU thread. This is useful evidence that the tire
stack is bounded and close to one-core real time. It is not a reason to camera-cull tire physics:
full-race scaling belongs at the vehicle/job-system level, while the 24x13 visual carcass already
uses a visibility-demand lease.

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
