# Vehicle Architecture

## Generic composition

The engine must not assume four wheels or one vehicle category. A vehicle is composed from reusable components such as:

- Chassis bodies.
- Wheel/contact units.
- Suspension elements.
- Steering systems and steering axles.
- Engines or electric motors.
- Clutches, gearboxes, range/splitter stages, transfer cases, final drives, and differentials.
- Brakes and driver aids.
- Aerodynamic surfaces.
- Trailers or articulated bodies.

Cars, motorcycles, ATVs, karts, trucks, and unusual historical vehicles select different component arrangements and data.

## Native versus Lua

Native C++ performs high-rate tire, suspension, powertrain, differential, aero, and deterministic state integration. Lua definitions reference reusable native components and provide parameters. Lua scripts manage input, spawning, UI, camera, gameplay, and prototype orchestration.

Do not create duplicated solvers named `CarTransmission.lua`, `MotorcycleTransmission.lua`, and `TruckTransmission.lua` when one component model plus different configuration can represent them. Add specialized components only when mechanics genuinely differ, such as a truck range/splitter or motorcycle chain final drive.


## Surface contact contract

World surface identity belongs to physics colliders, not to a whole vehicle.
Suspension/tire contact queries carry `SurfaceMaterial`, wetness, and the exact
contacted collider into each wheel state. A vehicle may therefore have four
different tire surfaces at the same instant.

The collision layer remains generic. It reports asphalt, gravel, dirt, grass,
snow, ice, kerb, painted-line, or default material data; tire providers decide
how those inputs affect forces. Weather depth, loose-surface deformation, and
temperature are future contact inputs rather than hard-coded global presets.

## Tire providers

Step 29G establishes `Vehicles/TireModel.*` as the native tire-force boundary.
The current advanced road provider uses explicit small-slip stiffness, peak
friction, load sensitivity, shape/curvature, transient relaxation,
combined-slip envelope, and pneumatic-trail data. `VehicleSystem` supplies the
contact state and applies returned forces; Lua supplies configuration only.

The tire API must support replaceable providers:

- Advanced combined-slip asphalt model.
- Motorcycle profile/camber model.
- ATV/low-pressure tire model.
- Deformable dirt, gravel, sand, mud, and snow models.
- Arcade providers for unrelated Heritage Engine modules.

All providers return forces, moments, and telemetry through a stable contact interface.

### Near-zero-speed and parked behavior

Slip curves describe a rolling/sliding contact; they are not sufficient by
themselves to represent a parked tire. The vehicle layer therefore has an
explicit, capacity-checked rest state. It uses per-wheel normal load, friction,
brake torque, chassis mass, gravity, and contact normals. This is not a blanket
velocity clamp: flat quiet vehicles may rest, a braked vehicle may hold only a
slope its brakes and tires can support, and an unbraked vehicle still rolls.

Service and parking brakes share the same non-overshooting zero-speed wheel
constraint. Do not restore sign-flipping fixed brake torque near zero angular
velocity; at a 1000 Hz substep it can reverse a stopped wheel repeatedly and
inject false tire forces into the chassis.

### Per-wheel tire ownership

Step 29H makes the tire description a property of each wheel/contact unit. A
vehicle still stores a default tire description so old callers and simple
vehicles can configure all wheels at once, but each wheel receives its own copy
when created and may then be overridden independently. This is required for
staggered sizes, mixed compounds, temporary spare tires, motorcycles with
different front/rear construction, multi-axle trucks, and damage/replacement.

Lua should reference named tire presets. The native solver receives only the
validated numerical description and never depends on a Racing United preset
name. `Vehicle.GetWheelTireModel` is the authoritative readback for tooling.
The current `prototype_road_front` and `prototype_road_rear` profiles are
diagnostic templates only, not production measured tire data.

## Visual presentation boundary

Step 29I keeps authored vehicle meshes separate from simulation state. The
`Player Chassis` Entity is a presentation child of the native vehicle root;
changing its OBJ, colour, offset, rotation, or scale must never change the rigid
body, collider, suspension mounts, tire contacts, or drivetrain.

The creator drop-in slot is `Assets/Vehicles/Player/PlayerCar.obj`. The current
renderer watches OBJ timestamps, allowing geometry iteration without a C++
rebuild. Whole-car OBJ files are valid for presentation tests.

Step 29J adds optional articulated wheel mesh slots. Step 29J.1 tightens the
contract: rendered wheel translation comes directly from native
`WheelState.worldCenter`; Lua does not reconstruct `mount - suspensionLength`.
Steering and spin remain native telemetry, and each corner may specify its own
visual asset, facing yaw and spin sign. The temporary legacy wheel OBJ slot still follows native vehicle axes, but
Step 29J.2 establishes the permanent Racing United authoring convention as
Blender X left/right, Y forward/backward, Z height at 1:1 scale. Importers must
convert at the engine boundary; artists must not rotate or resize source assets
to satisfy native internals. Wheel origin remains on its axle/center and visual
mesh scale may never alter tire contacts. The later glTF/PBR pipeline should replace the temporary
single-colour OBJ workflow. See ADR-007 through ADR-009.

## Data definitions

Vehicle definitions should be mostly data and reference reusable presets. Planned examples include `Peugeot_206_RC.lua` and `Ducati_Monster_S4R.lua`.

Step 29J.6 establishes `VehicleDefinitionV2` as the first versioned authoring
envelope. Bodies, power units, transmissions, contact units and drive
connections are explicit arrays with stable IDs. Classification is metadata
and an editor-template selector; it may not select a category-specific generic
solver. Definition validation is separate from current-solver readiness so an
honest twin-engine, articulated, tracked, or leaning definition can be retained
before its native providers exist. See `VEHICLE_WORKSHOP.md`.

Step 29K makes that envelope native. `VehicleDefinitionCompiler` resolves
stable authored IDs into component indices and selects providers exclusively
from topology and requested capabilities. `VehicleDefinitionLoader` consumes
the compiled graph through `raycast_wheel_v1`; classification never participates
in provider selection. Workshop preview now uses this path rather than mutating
drive factors and adding wheels in Lua. See `VEHICLE_DEFINITION_RUNTIME.md` and
ADR-012.

Step 29L makes suspension an explicit part of that graph. Contacts reference
stable suspension IDs; the compiler resolves them and the loader selects a
native `SuspensionModel` provider. The first `linear_raycast_v1` implementation
preserves current behavior and evaluates spring/damper force, motion ratio, and
the force ceiling behind a stable input/output contract. Requested linkage
providers remain authored but unresolved until their native geometry solvers
exist. See ADR-013.

Step 29M expands the healthy `linear_raycast_v1` force law with preload,
progressive springs, digressive low/high-speed bump and rebound damping,
progressive bump stops, droop stops and native damper-power telemetry. Force
components remain visible separately in `WheelState` and the Dynamics Lab.
Damage and wear are deferred until linkage, unsprung-mass, load-cycle and
thermal state exist. See `SUSPENSION_MODEL.md` and ADR-014.

## Dynamics instrumentation

Step 29J.5 adds an opt-in `VehicleDynamicsLab` recorder owned by each native
vehicle record. It samples authoritative vehicle and wheel state inside the
high-rate solver, retains bounded captures, calculates summary peaks, supplies
downsampled plot series and exports complete CSV data. Inactive vehicles do not
allocate a capture buffer. Repeatable experiment inputs remain module-side so
diagnostic scenarios cannot leak special behavior into production physics. See
`VEHICLE_DYNAMICS_LAB.md` for the public workflow and extension rules.
