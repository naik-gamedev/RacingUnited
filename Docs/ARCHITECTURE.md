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

## Public contracts

Public contracts are represented by:

- C++ headers.
- Generated Lua API manifests.
- Architecture decision records.
- Versioned data schemas.
- Automated tests and diagnostic reports.

Changing a contract requires updating all relevant records in the same milestone.
