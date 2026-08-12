# ADR-054 — Lua Responsibility Ownership and Compatibility Facades

**Status:** Accepted candidate in CLEAN08  
**Date:** 2026-08-10

## Context

Racing United Lua had several files whose names described broad feature areas while their contents had
grown into multiple enduring responsibilities. `SuspensionAuthoring.lua` mixed evidence/source policy,
estimation, native activation and gizmos; `VisualWheels.lua` mixed quaternion math, separate-wheel
presentation and embedded-GLB binding; VehicleDefinitionV2 validation mixed structural validity with
current-solver readiness. Runtime lifecycle/common files also contained physics-demo implementation.

Lua remains an authoring/orchestration layer. Moving these responsibilities must not duplicate or migrate
high-rate deterministic vehicle physics out of native C++.

## Decision

Split Lua by subsystem responsibility before files become dumping grounds. Keep coherent authored vehicle
definition files intact even when they are several hundred lines. Use small root compatibility load
coordinators for established paths such as `Vehicles/SuspensionAuthoring.lua` and
`Vehicles/VisualWheels.lua`; new implementation belongs in their responsibility-owned subdirectories.

VehicleDefinitionV2 uses separate core, dynamics/component and compatibility phases. A definition being
valid and a definition being previewable by the current native solver are distinct facts.

`Runtime/Lifecycle.lua` dispatches subsystem updates. `Runtime/Common.lua` contains only genuinely shared
helpers; physics-demo update/destruction/legacy cleanup belongs to `Runtime/PhysicsDemo.lua`.

Internal cross-file helper tables are module-private by virtue of Heritage's isolated module Lua
environment. They are implementation contracts, not public engine APIs.

## Consequences

- Future suspension evidence sources, estimators and gizmos have obvious owners.
- Cars, motorcycles, trucks, karts, ATVs and future trikes can share authoring mechanisms without turning
  one category-specific script into a universal dumping ground.
- Wheel-presentation math and embedded GLB semantics can evolve independently.
- Structural validation does not falsely reject valid future vehicle topologies merely because the current
  preview solver cannot simulate them yet.
- Lifecycle code remains readable as orchestration rather than hidden subsystem implementation.
- Existing root script paths remain compatible while the internal architecture becomes explicit.
