# Racing United Vehicle Lua Architecture

This folder configures and orchestrates vehicles. High-frequency simulation remains in the Heritage Engine native C++ Vehicle and Physics services.

## Responsibilities

- `Definitions/` — data-only vehicle definitions. One file per real or prototype vehicle.
- `State.lua` — mutable handles, tuning values, and telemetry for the active vehicle instance.
- `Input.lua` — input action registration and interpretation.
- `Formatting.lua` — human-readable names used by debug UI.
- `Visuals.lua` — presentation-only authored body mesh assignment/alignment; never vehicle physics.
- `VisualWheels.lua` — compatibility loader for responsibility-owned wheel presentation under `Visual/`.
- `Visual/TransformMath.lua` — presentation-only quaternion/transform helpers.
- `Visual/ArticulatedWheels.lua` — separate/proxy wheel mesh presentation driven from native telemetry.
- `Visual/EmbeddedWheelBinding.lua` — embedded GLB `WH_*` semantic binding and tire deformation.
- `Visual/VisualWheels.lua` — per-frame wheel-presentation coordinator.
- `Tuning.lua` — sends mutable tuning values to native systems.
- `Factory.lua` — creates, resets, and destroys native vehicle instances.
- `Telemetry.lua` — reads native state for UI and diagnostics.
- `DynamicsLab.lua` — repeatable experiment control for the opt-in native high-rate recorder.
- `Definitions/VehicleDefinitionV2.lua` — schema version and Workshop template catalog only.
- `Definitions/VehicleDefinitionV2Builder.lua` — Workshop draft construction and component-graph building.
- `Definitions/VehicleDefinitionV2Validation.lua` — schema/core validation and report orchestration.
- `Definitions/VehicleDefinitionV2DynamicsValidation.lua` — suspension/contact/ARB/chassis-flex/drive component validity.
- `Definitions/VehicleDefinitionV2Compatibility.lua` — valid-definition versus current native preview readiness.
- `Definitions/VehicleDefinitionV2Serialization.lua` — deterministic export serialization.
- `Workshop.lua` — persistent authoring draft, current-solver preview bridge and module-private export.
- `SuspensionAuthoring.lua` — compatibility loader for responsibility-owned authoring under `Suspension/`.
- `Suspension/HardpointSources.lua` — hardpoint evidence/provenance and GLB metadata import.
- `Suspension/HardpointEstimation.lua` — assisted low-confidence hardpoint estimation.
- `Suspension/SuspensionAuthoring.lua` — native hardpoint activation and authoring facade.
- `Suspension/HardpointGizmos.lua` — creator-only suspension marker presentation.
- `Lifecycle.lua` — vehicle scene entry/exit, fixed-step control, presentation, and shift commands.
- `../UI/VehicleDebugPanel.lua` — vehicle debug UI only; it contains no simulation logic.

## Rules for future work

1. Keep heavy deterministic physics in C++.
2. Prefer reusable native components and data definitions over duplicated car, motorcycle, ATV, or truck logic.
3. Add a specialized component only when the mechanics genuinely differ.
4. Keep one clear responsibility per Lua file.
5. Avoid hidden global state; current globals are transitional and should gradually move behind stable APIs.
6. Vehicle definition files should contain data, not per-frame simulation code.
7. Networking-critical state must be exposed explicitly and deterministically.
8. Dynamics-lab scenarios may provide fixed-step inputs, but measurement and simulation remain native; scenario code must never become production physics.
9. Vehicle categories are Workshop templates and metadata, never switches that select duplicated generic solvers.

Examples of future data definitions include `Peugeot_206_RC.lua` and `Ducati_Monster_S4R.lua`. They should reference reusable engines, gearboxes, differentials, tires, suspension, brakes, and aerodynamic components rather than copying their solvers.

## Tire presets

Step 29H stores the actual tire description per native wheel. Racing United
content references named profiles under `Vehicles/Presets/Tires/`; C++ receives
only validated numeric data. The current `PrototypeRoad.lua` values are
diagnostic templates used to prove front/rear and per-corner independence.
They must not be treated as measured production tire data.

TIRE02 lets a named preset reference a module-relative human-readable `.tir`
file. `Vehicle.LoadWheelTirePropertyFile` performs per-wheel import and records
provenance/confidence in the native tire description. The current front/rear `.tir`
files under `Data/Tires/` are synthetic compatibility seeds, not measured production
data; the numeric fields in `PrototypeRoad.lua` remain the fallback/debug path.

Use `Vehicle.SetTireModel` only when an intentional all-wheel override is
wanted. Use `Vehicle.SetWheelTireModel` for a numeric per-corner assignment,
`Vehicle.LoadWheelTirePropertyFile` for imported MF parameters, and
`Vehicle.GetWheelTireModel` for authoritative diagnostics/readback.


## Player vehicle visual slot

Step 29I reserves `Assets/Vehicles/Player/PlayerCar.obj` as a creator-owned
drop-in mesh path. `Vehicles/Visuals.lua` applies that mesh to the chassis
presentation Entity and keeps translation, rotation and scale separate from the
native body/collider. The Entity mesh renderer watches OBJ modification times,
so authored geometry can be replaced while the engine is running.

Do not move collision dimensions, wheel mounts, tire simulation, or drivetrain
logic into this presentation layer. Whole-car OBJ files are acceptable for a
quick visual test. Step 29J adds `VisualWheels.lua` and the optional
`PlayerWheel.obj` slot. Step 29J.1 moves presentation to the exact native
wheel-center telemetry and establishes the wheel asset axis/side convention;
those wheel Entities read the existing native suspension
length, Ackermann angle and wheel rotation rather than simulating them again.
