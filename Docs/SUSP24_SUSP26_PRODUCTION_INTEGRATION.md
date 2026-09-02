# Heritage Engine — SUSP24 → SUSP26 production suspension closure

## Status

The code in this package closes the physics-core defects found in the post-SUSP23 audit. The portable C++20 certification is green under `-Wall -Wextra -Werror` and under ASan+UBSan.

This package does **not** claim that an arbitrary RacingUnited checkout is production-wired merely because the headers compile. The final gate is the live `VehicleSystem`/Lua/Studio integration described below. `Tools/Validate_SUSP24_26_Wiring.ps1` returns a failing exit code until those calls are actually visible in the checkout.

## SUSP24 — one production suspension authority

### Required high-rate order

For every high-rate vehicle substep (normally 1 kHz):

1. Compute contact/support target and requested suspension travel.
2. Read `suspensionMountOffsetForNextKinematicsV2(cornerState)` from the **previous** force step.
3. Apply that 6-DOF mount/subframe/bent offset to the authored chassis-side hardpoints.
4. Dispatch exactly one kinematic provider through `SuspensionProviderRegistryV2` (or an existing SUSP01–SUSP13 adapter registered into it).
5. From the solved geometry, construct `SuspensionKinematicSampleV2` with exact element coordinates and Jacobians/motion ratios for spring, damper, actuator and stops.
6. After every corner has a kinematic sample, call exactly one `stepVehicleSuspensionV2(...)` for the vehicle.
7. Apply each returned `generalizedWheelForceN` exactly once to the wheel/unsprung support coordinate and the equal/opposite reaction to the chassis at the physical suspension load path.
8. Continue tire/contact solve from the solved upright/wheel pose.
9. Publish `SuspensionCornerTelemetryV2` and axle coupling telemetry.
10. Serialize `VehicleSuspensionRuntimeV2` wherever deterministic vehicle state is snapshotted for replay/save/network rollback.

### Forbidden duplicate authority

When a corner is SUSP24 production-owned, the old scalar `springRate`, `bumpDamping`, `reboundDamping`, canned camber/toe migration or a second ARB force path must **not** also add forces. Legacy fields may remain only as migration/default inputs.

`SuspensionAuthorityAuditV2` exists so the runtime regression can explicitly prove:

- production coordinator called;
- provider solved exactly once;
- compliance/damage mount feedback applied before geometry;
- generalized force applied exactly once;
- legacy scalar spring/damper force disabled.

## Kinematic provider registry

`SuspensionProviderRegistryV2` is now a callback dispatcher, not merely an enum list.

Built-in adapters in this package:

- `multi_link_v1`
- `swing_axle_v1`
- `sliding_pillar_v1`
- `motorcycle_link_front_v1` (new full 3D Hossack/girder/Duolever-style carrier authority)

Existing live providers from SUSP01–SUSP13 must register their current description/state adapters at VehicleSystem initialization. The canonical provider list remains:

- MacPherson/Chapman strut
- double wishbone
- pushrod/rocker wishbone
- rigid/live axle linkage
- leaf-spring live axle
- conventional motorcycle fork/swingarm
- semi-trailing arm / pure trailing-arm alias
- twist beam
- multi-link
- swing axle
- sliding pillar
- De Dion alias to rigid-axle location with differential mass kept sprung
- alternative motorcycle linked front

Aliases must resolve to an existing canonical callback; they are never a second solver.

## SUSP25 — physical element fidelity now owned by the production coordinator

### Springs and stops

- progressive coil
- main + helper/tender dual-rate coil with helper bind
- leaf pack plus velocity-opposing interleaf friction
- torsion-bar equivalent using authored arm ratio
- air spring with gas mass, gas temperature, chamber volume, reservoir flow, leak, compressor/vent and levelling deadband
- hydropneumatic sphere with gas thermal state, hydraulic line compliance, restrictor flow and leakage
- nonlinear bump and rebound stops

### Damper V3

- compression/rebound pressure chambers
- piston and rod displacement
- gas accumulator/preload
- bleed/orifice flow
- shim crack/full-open dynamics
- compression/rebound asymmetry
- seal friction
- hydraulic bump support
- cavitation and aeration/recovery
- oil leakage
- heat generation, cooling and temperature fade
- semi-active valve scaling
- bounded force and internal substepping

### Interconnected elements

- passive anti-roll bar, including nonlinear/damped twist
- third/heave spring/damper
- hydraulic left/right cross-link
- inerter
- active anti-roll torque
- exact bar-angle/rate inputs when provider geometry supplies them; wheel-travel/arm conversion is only the generic fallback

### Active/semi-active

- actuator force-rate limit
- true force-speed envelope using **actual** extension speed
- true mechanical power limit using actual speed
- motoring efficiency and regeneration bookkeeping
- optional built-in ride-height/skyhook controller
- Karnopp-style semi-active damper valve switching
- air-spring compressor/vent levelling

### Compliance and elastokinematics

`SuspensionComplianceDynamic.hpp` owns a dynamic coupled 6×6 stiffness/damping system with generalized mass/inertia, free play, Coulomb friction, hysteresis, wear softening and bounded internal substeps.

The resulting deflection is not a telemetry-only number: it must be fed to the next kinematic solve via `suspensionMountOffsetForNextKinematicsV2`.

### Wear, damage and failure

`SuspensionDamageV2.hpp` now has physical transitions for:

- bent/permanent set;
- seal leakage;
- fatigue;
- joint wear/free play;
- seized state;
- broken state;
- detached state / disabled constraint.

The production coordinator maps leakage into air/hydraulic/damper leakage, wear into compliance, permanent set into the next hardpoint solve, seizure into a high-force travel lock, and detachment into zero suspension constraint force.

## SUSP26 — state, telemetry and certification

### Deterministic state

`SuspensionSerializationV2.hpp` packet version 3 includes all current runtime state needed by the production element stack, including pneumatic/hydraulic state, damper chambers/shims/thermal/aeration, dynamic compliance, compliance feedback, damage geometry feedback, actuator energy/state and axle hydraulic state.

Required consumers:

- replay snapshot/restore;
- save/load where physical vehicle state is persisted;
- multiplayer/rollback state if/when authoritative vehicle rollback is enabled.

Do not serialize raw pointers or provider implementation addresses.

### Telemetry

Expose per corner at minimum:

- travel / velocity;
- spring force and tangent wheel rate;
- leaf friction where applicable;
- damper force, chamber pressures, temperature, aeration;
- bump/rebound stop forces;
- active force and mechanical power;
- air/hydropneumatic pressure;
- compliance 6-DOF offset and stored energy;
- damage flags, wear, leak and permanent set;
- final generalized/support force;
- constraint enabled/limited state.

Expose per axle:

- passive ARB torque;
- active ARB torque;
- third-element heave/roll force;
- hydraulic left/right force;
- inerter left/right force.

## Lua/vehicle-definition contract

The native binding layer must allow first-party Lua vehicle definitions to select a provider and author the physical fields above. Do not expose only one magic `springRate` slider and then hide the new hardware behind C++ defaults.

Recommended API ownership is one table/namespace (`Vehicle.Suspension` or the existing project equivalent) with narrow setters/builders for:

- provider + provider hardpoints;
- spring hardware;
- damper hardware;
- stops;
- per-corner compliance;
- per-corner damage/failure limits;
- axle interconnection;
- active/semi-active hardware/controller;
- state/telemetry queries.

Existing vehicle definitions must migrate without changing their current physical result until their old scalar parameters are intentionally replaced by measured/authored hardware.

## Heritage Studio authoring contract

Studio needs a Suspension page that edits the same native description, not a second simulation model. At minimum:

- provider selector;
- hardpoint gizmos and constraint-error display;
- live bump/droop sweep;
- camber/toe/caster/KPI/scrub curves derived from geometry;
- spring/damper force and motion-ratio plots;
- bump/rebound-stop engagement;
- ARB/third-element/interconnect editor;
- active/semi-active limits;
- compliance matrix/free-play/wear preview;
- damage/failure preview;
- live per-corner/axle telemetry.

## Certification exit gate

Suspension may be called **fully implemented** only when all of these are true in the merged RacingUnited checkout:

1. portable `SuspensionCertification.cpp` passes;
2. ASan/UBSan reference run passes where available;
3. HeritagePhysicsTests contains the SUSP24–26 authority/regression cases;
4. `VehicleSystem` calls the production coordinator in its actual high-rate substep;
5. no duplicate old scalar suspension force remains on production-owned corners;
6. compliance + bend feedback reaches the provider before geometry;
7. provider callback exists for every supported architecture;
8. Lua can author every production field needed by shipped vehicles;
9. Studio edits the same native data;
10. telemetry/state serialization are live;
11. 150-car mixed-provider engine benchmark stays finite/deterministic and meets the project's performance budget;
12. canonical Windows build-and-run smoke passes.
