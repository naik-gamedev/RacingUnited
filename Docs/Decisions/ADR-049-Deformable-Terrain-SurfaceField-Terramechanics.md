# ADR-049 — Persistent Deformable Terrain / SurfaceField Terramechanics

> **CLEAN10 ownership update:** ADR-057 moves this field from its original prototype location/vehicle ownership into `Physics/Surfaces/SurfaceWorld` with FP64 global addressing and chunked streaming-safe storage. The terramechanics decision here remains valid.


## Status

Accepted for TIRE15 candidate implementation.

## Context

TIRE14 covers a shallow gravel or hard-dirt layer over a load-bearing base. Mud, sand, soft soil
and deep snow are different because the ground itself yields, compacts and can retain a rut after
the wheel leaves. Treating these surfaces as MF6.2 with a lower friction multiplier would discard
the mechanisms that dominate traction and motion resistance.

Heritage also targets large racing/free-roam grids, so the normal runtime path cannot simulate every
sand grain, snow crystal or mud particle. The terrain model therefore needs reduced-order physics,
explicit bounded state and a clean separation between world/terrain memory and tire-owned state.

## Decision

### 1. MF6.2/SWIFT remains the pneumatic tire; terrain reaction becomes primary

The tire continues to own its geometry, pressure, structural modes, transients, temperature, wear
and tread traits. On deformable terrain, the direct MF hard-interface contribution is deliberately
scaled down. `TireDeformableTerrainInteraction` supplies the dominant ground reaction from:

- Bekker-style pressure/sinkage;
- Mohr-Coulomb-style maximum shear strength;
- Janosi/Hanamoto-style shear-displacement mobilization;
- lateral passive-wedge bulldozing;
- longitudinal plowing/compaction resistance;
- tread depth/aggressiveness/edge/open-void/flotation effectiveness.

The current equations are a clean-room reduced-order implementation. They do not claim parity with
Altair, Project Chrono SCM, Simcenter, DEM, CRM or any proprietary solver.

### 2. `SurfaceField` is shared world state, not tire state

`Physics/SurfaceField.*` stores persistent dynamic driven-surface state in a sparse grid keyed by
quantized world X/Z position and `SurfaceMaterial`. The initial TIRE15 state is:

- loose depth;
- compaction;
- moisture;
- rut depth;
- longitudinal and lateral shear history;
- displaced volume;
- approximate completed wheel-pass count.

The default cell size is 0.25 m and the field has a bounded maximum cell count with oldest-cell
eviction. This makes persistent terrain memory explicit and prevents unbounded growth.

All vehicles in one `VehicleSystem` query/update the same field. A following wheel therefore sees
rut and compaction left by a previous wheel or vehicle. `VehicleSystem::clear()` resets the field
with the world/vehicle simulation.

### 3. Persistent rut depth changes the physical support datum

The static collider remains the geometric reference, while the dynamic field supplies a local
support-depth offset. The wheel support solve adds the current persistent rut plus elastic sinkage,
so terrain memory changes hub/suspension physics rather than existing only as telemetry or visuals.
Plastic rut formation approaches its target over time instead of snapping to full depth in one
1000-Hz substep.

### 4. TIRE06 footprint fractions remain composable

Mud, sand, soft-soil and deep-snow fractions are collected from the adaptive footprint sampler.
A tire can therefore be partly on a hard surface and partly over deformable terrain. The dominant
deformable material selects the reduced-order field material at the current contact location while
the footprint fraction scales its contribution. A future higher-fidelity world tier may update
individual footprint subcells; the normal TIRE15 path deliberately remains bounded.

### 5. Tire metadata and terrain metadata remain separate

`[HERITAGE_DEFORMABLE_TERRAIN]` in `.tir` files contains only tire-side traits such as tread
aggressiveness, open-void ratio, shear/bulldozing/plowing coupling and flotation. Mud/sand/snow/soil
cohesion, friction angle, pressure-sinkage constants, moisture and depth are ground properties and
must migrate into authored `SurfaceMaterial` / `SurfaceField` data.

TIRE15 currently uses explicit synthetic material presets as a compatibility bridge. They must not
be represented as measured track/soil data.

### 6. Visual terrain deformation is a consumer, not the source of truth

Ruts, mud spray, sand displacement and snow spray can later render from `SurfaceField`. Individual
visual particles do not participate in the tire solver. TIRE15 makes the physical terrain state
available first; mesh tessellation/deformation, weather relaxation/erosion, persistence to disk and
network replication are separate world/presentation mechanisms.

## Consequences

- Mud/sand/deep snow no longer need to masquerade as low-grip asphalt.
- Repeated passes can change later vehicle behavior through one shared deterministic world field.
- The normal path remains much cheaper than particle/grain simulation.
- Current terrain constants are deliberately synthetic and need authoring/calibration tools later.
- The sparse field introduces world-state ownership that future weather, renderer, audio, particles,
  networking and editor tooling can share.

## Regression contract

TIRE15 regressions must verify at minimum:

- finite bounded pressure/sinkage and terrain forces;
- persistent rut/compaction/shear/displaced-volume state after repeated contact;
- untouched world cells remain virgin;
- mud, sand and deep snow remain distinguishable;
- partial footprint mixing is non-binary;
- field evolution is reasonably stable across high- and lower-rate integration;
- `.tir` import maps the tire-side deformable-terrain section without unsupported assignments;
- Visual Studio and portable native regression projects compile both SurfaceField and the provider.
