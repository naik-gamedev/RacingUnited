# ADR-079 — Integrated OpenGL Weather Presentation

## Status
Accepted for WEATHER06A.

## Context

JOB03 provides deterministic, distance-adaptive surface hydrology but the world
still reads visually as a clear day with debug-like water. Earlier WATER04/05
experiments also demonstrated two failure modes that must not return: injecting
wetness into the general entity renderer can destabilize unrelated rendering,
and expensive per-fragment neighborhood rain searches can hurt input/frame
responsiveness.

## Decision

Keep weather authority and presentation separate. `SurfaceWorld` weather and
`SurfaceHydrology` remain authoritative. OpenGL presentation is split by concern:

- `SkyRenderer` owns sky/cloud integration. Volumetric cloud density is procedural,
  world-space and wind-advected. It renders at one-third view resolution and is
  reconstructed over the procedural sky.
- `WeatherPresentationRenderer` owns falling rain. It draws a fixed-capacity
  instanced lattice after opaque/surface geometry. Representative positions are
  derived from world precipitation cells, camera cell remainder and weather time;
  the camera does not own the storm.
- `SurfacePresentationRenderer` continues to own wet-film/standing-water visuals
  because it already consumes hydrology records. Early wetness uses bounded
  deterministic circle functions and depth-driven merging; no CPU raindrop list or
  3x3 fragment search is allowed.
- `EntityMeshRenderer` receives only weather lighting/far-haze parameters. It does
  not own rain particles or a wetness atlas.

Cloud cover/rain attenuate environment lighting before shadows and cubemap
generation. `EnvironmentMap` therefore treats material lighting changes as a
refresh trigger in addition to time-of-day changes.

## Consequences

- Storms affect the sky, direct light, reflections, far visibility, falling rain
  and wet ground coherently from one weather state.
- Rain representative count is bounded and GPU-generated; visual quality can be
  scaled independently from millimetres/hour supplied to hydrology.
- Volumetric clouds cost a bounded low-resolution ray march rather than a full-4K
  march.
- Cloud simulation is currently a deterministic presentation density field, not
  a meteorological solver. A later weather-field service may replace its procedural
  density without changing the view renderer.
- Roof/tunnel precipitation occlusion and spatial cloud-shadow maps remain later
  milestones.
