# Vehicle Topology Architecture

## Why this layer exists

Heritage must support a family of road and off-road vehicles without turning the common vehicle
solver into a four-corner passenger-car special case. The common native core therefore owns the
mechanisms that genuinely transfer across vehicle families, while a thin topology layer owns only
whole-vehicle coupling that actually differs because of wheel arrangement or balance.

The architecture is deliberately scaffolded before every topology is implemented. Empty topology
sources are destinations, not feature claims.

## Shared mechanisms stay shared

The following are not intrinsically "car" or "motorcycle" systems and should remain reusable unless
a real physical difference requires a specialized provider:

- wheel/contact state and arbitrary wheel iteration;
- tire force, contact patch, thermal, wear, wet/winter/granular/deformable-terrain providers;
- suspension element/provider interfaces;
- wheel fitment and alignment data;
- brakes and brake thermal state;
- power units, clutch, gearbox, final drive and differential components;
- surface queries and SurfaceField state;
- telemetry and deterministic simulation scheduling.

A motorcycle can use a different tire profile, fork/swingarm provider, chain final drive and
whole-vehicle lean coordinator without duplicating the generic tire, brake or gearbox architecture.
A multi-axle truck can add axles, driven/steered roles, range/splitter stages and articulation
without teaching every common system that trucks are a special hard-coded vehicle class.

## Topology destinations

```text
Vehicles/Topology/
    Common/
        VehicleTopologyCoordinator.cpp
    TwoWheel/
        TwoWheelVehicleDynamics.cpp
    ThreeWheel/
        ThreeWheelVehicleDynamics.cpp
    FourPlusWheel/
        FourPlusWheelVehicleDynamics.cpp
```

### Common

Wheel-count-agnostic orchestration and future authored topology/axle-role interpretation. Common
code must iterate the vehicle's authored contacts/wheels/axles rather than indexing FL/FR/RL/RR.

### TwoWheel

Reserved for genuinely single-track whole-vehicle mechanics such as motorcycle lean/balance,
countersteer coupling, rider mass/control and front/rear balance interactions. Motorcycle-specific
suspension and tire providers remain in their existing subsystem directories.

### ThreeWheel

Reserved for rigid and leaning trikes. It must be able to represent tadpole (two front, one rear)
and delta (one front, two rear) layouts without pretending either is a damaged four-wheel car.
Reusable components remain common.

### FourPlusWheel

Cars, karts, many ATVs and trucks live here only for whole-vehicle coupling that benefits from a
multi-track/four-plus-wheel coordinator. The name does **not** mean "exactly four". Six-, eight- and
other multi-axle vehicles must remain natural data-driven cases.

Trailers/semitrailers and connected bodies continue to use `Vehicles/Articulation/`; articulation is
composition, not a replacement for the topology of each connected vehicle body.

## Native data rule

The current native `VehicleRecord` already stores wheels in a `std::vector`; preserve that property.
Future APIs should prefer stable wheel/contact IDs, axle/group IDs and semantic roles over positional
assumptions. `front/rear`, `left/right`, `steered`, `driven`, `braked`, `handbrake`, and topology
relationships are authored attributes, not array-slot meanings.

Do not introduce a hard-coded `wheel[4]`, a required four-entry tuple, or an API whose only legal
names are FL/FR/RL/RR in the common native layer. Convenience helpers for common four-corner cars may
exist in authoring/UI code, but they must compile down to the generic representation.

## Lua boundary

Lua describes and orchestrates configuration; C++ performs reusable high-rate simulation. The
Racing United scaffolds are:

```text
Scripts/Vehicles/Topology/
    Common.lua
    TwoWheel.lua
    ThreeWheel.lua
    FourPlusWheel.lua
```

These files are intentionally not wired into runtime behavior yet. When activated, they should
produce/validate authored topology data and call stable native APIs. They should not become Lua tire,
suspension or drivetrain solvers.

## Refactor rule

A subsystem file exists because it owns a stable responsibility, not because another file exceeded a
line-count threshold. CLEAN03A applies that rule immediately to vehicle configuration: suspension,
fitment, alignment, anti-roll bars, chassis compliance, unsprung mass, steering, brakes, driver aids,
drivetrain and tires now have distinct native ownership files.

## Shared high-rate wheel solver

CLEAN03B keeps the authoritative high-rate wheel/contact/tire solver in the common vehicle layer.
Its named phases under `Vehicles/Simulation/WheelSubstep/` operate on one wheel record at a time and
must remain independent of an exactly-four-wheel assumption. Whole-vehicle topology layers may
coordinate lean, balance, axle grouping, steering strategy or other category-specific coupling, but
should reuse the common wheel/suspension/tire providers whenever the underlying physics is shared.
## OPT01 source-hygiene note

The earlier empty C++/Lua topology scaffold files have been retired. Their architectural intent remains here and in `VEHICLE_SUBSYSTEM_ARCHITECTURE_MANIFEST.md`. Source files are created only when they contain a real implementation, contract or testable behavior.
