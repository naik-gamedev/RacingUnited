# Wheel Substep Phase Boundaries

CLEAN03B physically partitions the user-validated high-rate wheel solver without changing its equations or statement order.

`VehicleWheelSimulation.cpp` remains the single authoritative `VehicleSystem::simulateWheelSubstep()` entry point. The files in this directory are **function-scope implementation fragments** included in a fixed order. This is deliberate: the pre-CLEAN03B solver had many cross-phase intermediates, and introducing a large context object or rewriting every dependency in the same step would create unnecessary numerical/regression risk.

## Rules

- Keep the phase order in `VehicleWheelSimulation.cpp` authoritative.
- Do not compile these `.inl` files as standalone translation units.
- Do not duplicate tire-provider logic here; wet, winter, granular, deformable-terrain, thermal, wear, rigid-ring, contact-patch and Magic Formula mechanisms stay in their provider files.
- New code belongs in the narrowest enduring subsystem/provider first. Only orchestration and genuinely per-wheel coupling belong in these phases.
- Common per-wheel phases must remain independent of an exactly-four-wheel assumption. Cars, karts, ATVs, trucks, motorcycles and future trikes all reuse the same per-wheel mechanisms where physically valid; topology-specific whole-vehicle coupling belongs under `Vehicles/Topology/`.
- If a phase later develops a stable, narrow input/output contract, it may graduate from a lexical fragment to a private compiled helper. Do that as its own behavior-preserving cleanup, not while adding tire physics.

## Ordered phases

1. `00_PrepareWheelAndSupportQuery.inl`
2. `01_TelemetryAndAirbornePolicy.inl`
3. `02_SteeringBrakingAndFreeWheel.inl`
4. `03_RoadEnvelopeAndFootprintSampling.inl`
5. `04_TireStructureAndTerrainSupport.inl`
6. `05_SuspensionAndContactResolution.inl`
7. `06_ContactKinematicsAndPatchGeometry.inl`
8. `07_SurfaceProvidersAndContactPatch.inl`
9. `08_TireForcesAndSurfaceReactions.inl`
10. `09_TirePhysicalStateUpdate.inl`
11. `10_ApplyForcesAndIntegrateWheel.inl`
