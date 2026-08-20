# Heritage Engine Architecture

## Dependency direction

Low-level reusable systems must not depend on Racing United gameplay scripts.

Expected direction:

`Platform/Window/Input -> Core/Entities -> Physics -> Vehicles -> Module Runtime -> Game Lua/UI`

Rendering, audio, networking, traffic, and tools interact through explicit services and handles rather than hidden global ownership.

## Engine versus module

Heritage Engine owns reusable native services:

- Window, display, rendering, audio, input, entities, physics, vehicles, scene loading, saves, and Lua bindings.

A module owns:

- Content definitions, gameplay rules, UI, race modes, traffic policy, vehicle configurations, and module-specific presentation.

Racing United must not be baked into general engine services. Another module may replace vehicle handling or omit vehicles entirely.

Source-tree hygiene follows the same boundary: reusable `Engine/HeritageEngine` code must not contain Racing United-specific scene classes, entry IDs, or gameplay fallbacks. Module-specific bootstrapping belongs in `Modules/<ModuleId>` manifests/scripts/content. A generic engine service may know that modules and scenes exist; it must not know that a particular game module exists.

## Simulation boundary

Native C++ owns computationally heavy, deterministic, networking-critical simulation. Lua configures and orchestrates it. Lua may prototype behavior, but production tire, suspension, drivetrain, differential, aero, and force-feedback solvers belong in native components.

## Scalability

The architecture must support:

- Multi-rate simulation.
- Physics, AI, and networking levels of detail.
- Interest management for large races and free-roam worlds.
- Sleeping and simulation islands.
- Data-driven arbitrary wheel counts and axle arrangements.
- Server-authoritative multiplayer with prediction/interpolation where appropriate.

## CPU concurrency

`Core/Jobs/JobSystem` is the default engine-wide CPU worker pool. Heavy native systems submit bounded
jobs to this shared scheduler rather than creating one permanent thread or private pool per subsystem.
The caller participates in synchronous batches, and authoritative phases keep explicit barriers and
deterministic ownership/reduction rules. Subsystems must remain valid on low-core machines; core count
is a scheduling resource, not an assumption embedded in gameplay or physics. See ADR-076 and
`PERFORMANCE_MULTICORE_ROADMAP.md`.

## Public contracts

Public contracts are represented by:

- C++ headers.
- Generated Lua API manifests.
- Architecture decision records.
- Versioned data schemas.
- Automated tests and diagnostic reports.

Changing a contract requires updating all relevant records in the same milestone.

## Source organization

File length is a diagnostic, not a design rule. Split code when one translation unit or function owns multiple independently changing responsibilities, not simply because it crosses an arbitrary line count.

Current organization rules:

- `Core/Modules/LuaModuleRuntime.cpp` owns Lua state/lifecycle, sandboxing, registration order, hot reload, API introspection and smoke-test orchestration. API handler bodies live below `Core/Modules/LuaBindings` and large namespaces such as Physics, Vehicle and Entity are subdivided again by responsibility. See `LUA_BINDING_ARCHITECTURE.md`.
- The directory path supplies broad subsystem context; filenames name the concrete mechanism/responsibility. Avoid both vague dumping-ground names and filenames that redundantly encode the entire directory path.
- Large numerical solvers should retain a small orchestration layer and extract well-defined stages only when regression coverage can prove behavior is preserved. In particular, vehicle-substep decomposition is treated as an isolated refactor rather than being mixed into unrelated feature work.
- `Vehicles/VehicleSystem.cpp` is the vehicle lifetime/handle/plumbing unit. Vehicle configuration, Dynamics Lab telemetry, vehicle-level stepping and the authoritative high-rate wheel solver live in `VehicleConfiguration.cpp`, `VehicleTelemetry.cpp`, `VehicleSimulation.cpp` and `VehicleWheelSimulation.cpp` respectively. This boundary is responsibility-based; tire milestone numbers do not become source-file architecture.
- Independent regression domains live in independent test translation units so safety coverage can grow without recreating a monolithic test file.
- Planned native vehicle mechanisms may be scaffolded ahead of implementation as project-visible, non-compiled files. This makes ownership visible before new code has an opportunity to accumulate in generic files.

