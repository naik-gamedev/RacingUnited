# ADR-011 — Vehicle Forward Axis, Axle Roles, and Driveline Roles

## Status
Accepted in Step 29J.4A.

## Context
Racing United content is authored in Blender with X left/right, Y fore/aft and
Z height. That coordinate contract alone does not define which sign of Y is the
vehicle's nose. The first creator-authored Peugeot 206 RC body was modeled in
the common Blender vehicle convention where the nose points toward **-Y**.

The Step 29J.4 temporary OBJ bridge had assumed +Y was vehicle-forward. Native
vehicle physics uses +Z as forward. The result was a 180-degree mismatch between
the rendered body and the authoritative wheel assemblies: the physics front
axle appeared under the rendered rear of the car. Because only the native front
wheels steer, this looked exactly like rear-wheel steering.

The temporary prototype definition was also still rear-wheel drive even though
the Peugeot 206 RC reference vehicle is front-wheel drive.

## Decision
- Racing United's Blender authoring axes remain:
  - X = left/right.
  - Y = fore/aft.
  - Z = height.
  - 1 Blender unit = 1 metre.
- For **vehicle assets**, the semantic forward/nose direction is **Blender -Y**.
- Heritage Engine native vehicle forward remains +Z for now. Import/attachment
  code owns the deterministic conversion; creators do not rotate their precise
  source geometry to satisfy native engine axes.
- During the temporary OBJ bridge, creator vehicle bodies receive one fixed
  180-degree native yaw after Blender OBJ axis conversion so authored -Y forward
  aligns with native +Z forward. This is an import convention, not a tuning
  offset.
- Wheel roles are explicit per corner. A wheel steers only when its authored
  `steerFactor` is non-zero. Visual presentation additionally gates steering by
  that role so non-steering rear wheels cannot inherit stale steering telemetry.
- Driveline roles are explicit per corner through `driveFactor`. The current
  Peugeot-oriented prototype uses front-wheel drive: FL=0.5, FR=0.5, RL=0,
  RR=0. Other vehicles may configure RWD, AWD, motorcycles, multi-axle trucks,
  or active/rear steering without changing the generic native solver.

## Consequences
The rendered body, native front axle, steering axle and driven axle now agree
for the Peugeot-oriented prototype. Future glTF vehicle import must preserve the
same Blender -Y vehicle-forward contract without exposing corrective rotation
sliders to creators.
