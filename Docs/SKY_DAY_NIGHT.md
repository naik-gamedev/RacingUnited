# Visible sky and procedural day/night cycle (GFX7)

GFX7 turns the GFX6 environment from a reflection-only cubemap into a visible,
time-varying world environment.

## What GFX7 adds

- A visible sky rendered from the same environment cubemap used by materials.
- A renderer-independent `EnvironmentSystem` shared with the active module.
- A 24-hour procedural clock.
- A moving sun with time-dependent direction, color and intensity.
- Blue daylight sky, warm sunrise/sunset transitions and a dark blue night sky.
- A simple procedural star field at night.
- Environment cubemap regeneration as time advances so reflections follow the
  day/night cycle.
- CPU cubemap updates are rate-limited so accelerated preview speeds do not
  regenerate the 128x128x6 map every rendered frame.
- Direct GGX sunlight now follows the same sun direction/color as the sky.

The engine starts at 14:00 with the cycle enabled at `240x`, which means one
complete simulated day takes six real minutes. This intentionally makes the
feature easy to inspect during development rather than requiring a real 24-hour
wait.

## Developer shortcuts

- `F6` - pause/resume the day/night cycle.
- `F7` - cycle preview speed through:
  - `1x` - real time;
  - `60x` - one simulated minute per real second;
  - `240x` - one full day in six real minutes;
  - `1440x` - one full day in one real minute.

The title bar shows these preview shortcuts. They are development conveniences;
a future Heritage Editor can expose the same controls as normal visual fields.

## Lua environment API

The active module can control the same native environment clock:

```lua
Environment.SetTimeOfDay(17.5)       -- 17:30
Environment.SetCycleEnabled(true)
Environment.SetTimeScale(240.0)

local hour = Environment.GetTimeOfDay()
local enabled = Environment.IsCycleEnabled()
local speed = Environment.GetTimeScale()
local sunX, sunY, sunZ = Environment.GetSunDirection()
```

`Environment.SetTimeScale` is expressed as **simulated seconds per real
second**. The environment clock is independent from the deterministic physics
step so changing visual time preview speed does not change vehicle physics.

## Environment maps and HDRIs

An environment map is a representation of the light/color surrounding the
scene in every direction. An HDRI used as a Blender World environment is one
common source: it is often stored as a 2:1 equirectangular 360-degree image and
converted by a renderer into directions/cubemap faces.

Heritage currently generates its environment procedurally instead of loading an
HDRI file, but GFX6/GFX7 deliberately use an `EnvironmentMap` resource so a
later importer can accept HDR/EXR equirectangular environments without changing
material authoring.

## Still later

GFX7 is not the final sky/IBL system. Useful follow-ups include:

- HDR/EXR equirectangular environment loading;
- true GGX importance-sampled specular prefiltering;
- split-sum BRDF LUT;
- local reflection probes;
- physically richer atmospheric scattering, clouds, moon and weather;
- module/editor controls for geographic sun position/date if useful.
