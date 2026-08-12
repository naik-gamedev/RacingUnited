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

## High-rate vehicle-step staging

The native high-rate vehicle loop is deliberately staged rather than implemented
as one monolithic function. `simulateVehicleSubstep()` owns orchestration only:
it snapshots chassis state, advances steering/Ackermann, advances the driveline,
then evaluates each wheel in deterministic order before capturing Dynamics Lab
telemetry. `updateSteeringSubstep()`, `updateDrivelineSubstep()`, and
`simulateWheelSubstep()` are private implementation boundaries, not separate
solvers.

Keep numerical ordering stable when refactoring this path. A structural cleanup
must pass the same regression suite before and after; do not mix a decomposition
with tire, suspension, gearbox, or driver-aid tuning. High-rate temporary buffers
that scale with wheel count should be retained/reused per vehicle where practical
rather than allocated anew at 1000 Hz.



## Native code layout and scaffold-first rule

Heritage Engine creates the intended native vehicle-mechanism layout before every
provider is implemented. A scaffold `.cpp` is allowed to exist as a project-visible,
non-compiled placeholder. This makes the destination of future work explicit and
prevents broad files such as `VehicleSuspensionGeometry.cpp` from becoming permanent
dumping grounds. A scaffold is promoted to an active `ClCompile` source only when its
mechanism receives implementation.

The directory path carries broad context; the filename names the actual mechanism or
responsibility. Prefer paths such as
`Vehicles/Suspension/Geometry/DoubleWishbone/DoubleWishboneKinematics.cpp` over either
a vague `SuspensionGeometry.cpp` bucket or a filename that repeats the complete path.
Cars, ATVs, open-wheelers and trucks reuse the same mechanism implementation when the
physics is genuinely the same. Vehicle-category-specific files exist only for mechanics
that truly differ, such as motorcycle lean dynamics or kart chassis flex.

The current scaffold covers steering, common suspension forces, major linkage
geometries, axle/spring types, motorcycle suspension, wheels, tire submodels,
drivetrain/differentials, brakes, aerodynamics, chassis-flex/lean/kart dynamics and articulation.
Empty scaffolds are architectural destinations, not claims that those features already
exist.

CLEAN03A extends this rule to whole-vehicle topology. `Vehicles/Topology/Common` must remain
arbitrary-wheel-count; `TwoWheel`, `ThreeWheel` and `FourPlusWheel` are reserved for coupling that
truly changes with topology. Shared tires, suspension elements, brakes, powertrain components and
surface interaction must not be copied into category solvers. Multi-axle trucks remain natural
four-plus-wheel data rather than an exactly-four-wheel exception. See
`VEHICLE_TOPOLOGY_ARCHITECTURE.md`.

## Chassis mass properties and body roll

Vehicle asset/reference coordinates do not define the physical mass center.
ROLL01 separates the authored chassis origin from `centerOfMassLocal`, so wheel
mounts, suspension hardpoints, colliders and visual children retain stable
coordinates while the rigid body carries its own physical COM. Tire forces are
applied at contact points and therefore create real roll/pitch/yaw torque around
that COM; springs, dampers and independent anti-roll bars react to the resulting
suspension motion rather than a camera or visual animation faking body roll.

The Peugeot-oriented prototype currently uses a 0.20-confidence estimated compact
FWD hatch COM. This is explicitly replaceable evidence. Future component mass
authoring should derive total mass, COM and inertia from stronger vehicle data
without changing suspension geometry. See ADR-028.

ROLL02 additionally locks simultaneous pitch/roll/yaw and asymmetric four-corner
load transfer. FLEX01 promotes the dedicated `Vehicles/Dynamics/ChassisFlex/`
scaffolds into a reusable first torsional mode: `ChassisTorsionalCompliance` owns
the FP64 structural state/integration, `ChassisFlexEstimator` supplies explicitly
low-confidence starting data, and `ChassisFlexDiagnostics` exposes creator-facing
state. Gross rigid-body roll remains separate; flex rotates the virtual suspension
pickup frame by tiny front-to-rear section angles under diagonal load. See ADR-030.

## Suspension composition and per-axle mechanisms

A suspension definition is not a vehicle-category switch. Kinematics, spring
medium, damping and anti-roll coupling are separate responsibilities. One
vehicle may therefore use a strut/coil-spring front assembly and a
trailing-arm/torsion-bar rear assembly without introducing a special
`PeugeotSuspension` solver. The 206-oriented prototype is the first explicit
example of this rule.

SUS01 adds optional stable hardpoint IDs/positions to the native definition
contract and creator gizmos to inspect what is known. SUS03A extends that
contract with provenance/confidence so an unmeasured point may be supplied by a
versioned assisted-authoring estimate without being mistaken for measured data.
SUS03B adds the reusable trailing-arm/torsion-bar rear mechanism and locks
assisted suspension geometry to chassis reference-package data: wheel/rim/tire
fitment changes are downstream and cannot regenerate pickup points. SUS04 adds
anti-roll bars as independent top-level suspension coupling components with
stable left/right contact-unit references, rather than embedding them in any
kinematics provider. See `SUSPENSION_AUTHORING.md`, ADR-022, ADR-024, ADR-025,
ADR-026 and ADR-027.

SUS02 promotes the MacPherson scaffold into the first reusable hardpoint-derived
kinematics provider. `macpherson_strut_v1` solves lower-arm rotation, current
steering axis, passive tie-rod bump steer, wheel-centre/upright pose, strut
compression and instantaneous spring motion ratio from eight stable per-corner
hardpoints. SUS03A lets the Peugeot-oriented module activate that same provider
from a versioned low-confidence estimated package when better coordinates are
unavailable; GLB-authored or measured points can replace estimates incrementally.
See `SUSPENSION_GEOMETRY.md`, ADR-023 and ADR-024.

## Reference assembly, wheel fitment and alignment

FITMENT01 treats the creator asset as a neutral reference assembly and stores wheel
fitment/alignment as a separate per-corner setup. Reference suspension hardpoints,
mounts and steering geometry remain chassis data. Installed ET/spacers move the tire
centerline relative to that reference upright, while camber/toe and optional caster
overrides alter the active setup used by the suspension/tire basis. Front and rear
left/right linking is only a Workshop convenience; the native representation remains
independent per corner for oval and other asymmetric vehicles.

The setup layer may consume explicit wheel/tire GLB custom properties, but Heritage does
not yet infer that an arbitrary `WH_*` node origin is a physical wheel centerline. A
future origin-semantic contract must distinguish hub mounting face, wheel centerline and
spin-axis datums before those transforms become authoritative fitment coordinates. See
`WHEEL_FITMENT_AND_ALIGNMENT.md`, ADR-025 and ADR-032.

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
the force ceiling behind a stable input/output contract. Requested linkage providers remain authored independently of their implementation
status. As of SUS02, `macpherson_strut_v1` is solver-ready when its required
hardpoints are present; other requested linkage families remain unresolved
until their own providers exist. See ADR-013 and ADR-023.

Step 29M expands the healthy `linear_raycast_v1` force law with preload,
progressive springs, digressive low/high-speed bump and rebound damping,
progressive bump stops, droop stops and native damper-power telemetry. Force
components remain visible separately in `WheelState` and the Dynamics Lab.
Damage and wear are deferred until linkage, unsprung-mass, load-cycle and
thermal state exist. See `SUSPENSION_MODEL.md` and ADR-014.

Step 29N exposes atomic set/readback of that complete healthy description per
wheel. Live tools edit native truth rather than maintaining an unverified Lua
copy, and asymmetric axle/corner setups do not require a new solver. The
historical `Vehicle.AddWheel` argument order remains source compatible; new
nonlinear values are optional trailing arguments. See ADR-015.

Step 29O adds optional per-contact unsprung inertia and radial tire compliance.
A bounded scalar provider computes authoritative suspension-axis hub motion,
wheel hop, tire deflection and road-normal load at the high-rate vehicle step.
Effective mass zero preserves the earlier massless path as a compatibility and
scalability tier. This is deliberately independent of category and linkage
type; future geometry providers may reuse or replace it. See
`UNSPRUNG_MASS_MODEL.md` and ADR-016.

Step 29P adds `SuspensionGeometry` as the native upright-kinematics boundary.
Every contact owns a 3D steering axis and signed camber/toe travel curves. The
provider's orthonormal upright basis is shared by tire force direction,
telemetry and articulated wheel presentation; Lua no longer reconstructs
steering orientation. Zero curves preserve existing definitions. SUS02 adds the first hardpoint linkage
provider for MacPherson struts, including linkage-derived wheel-centre motion,
bump steer and instantaneous spring motion ratio. More complete caster/scrub
metrics, jacking-force decomposition, compliance and camber thrust remain
separate future work. See `SUSPENSION_GEOMETRY.md`, ADR-017 and ADR-023.

## Dynamics instrumentation

Step 29J.5 adds an opt-in `VehicleDynamicsLab` recorder owned by each native
vehicle record. It samples authoritative vehicle and wheel state inside the
high-rate solver, retains bounded captures, calculates summary peaks, supplies
downsampled plot series and exports complete CSV data. Inactive vehicles do not
allocate a capture buffer. Repeatable experiment inputs remain module-side so
diagnostic scenarios cannot leak special behavior into production physics. See
`VEHICLE_DYNAMICS_LAB.md` for the public workflow and extension rules.
