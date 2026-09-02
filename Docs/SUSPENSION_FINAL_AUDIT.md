# Suspension final audit — what SUSP24–SUSP26 closes

The post-SUSP23 audit found that SUSP15–SUSP23 compiled but were disconnected helpers. SUSP24–SUSP26 changes the architecture around one force coordinator and closes the specific physics defects found during that audit.

## Closed in this package

- actual provider callback registry (`SuspensionProviderRegistryV2`)
- full per-corner production element pipeline
- whole-vehicle/axle production coordinator
- dynamic coupled 6-DOF compliance
- compliance feedback contract into next geometry solve
- physical permanent-set feedback from bent damage
- leak, loose, seized, broken and detached damage behavior
- true active actuator force-speed-power envelope
- dynamic pneumatic spring with temperature/mass/flow/leak/levelling
- dynamic hydropneumatic gas/hydraulic state
- Damper V3 pressure chambers, valves, gas, cavitation, aeration, thermal state and leak
- 3D alternative motorcycle front geometry
- leaf interleaf friction in production dispatcher
- inerter in production axle dispatcher
- active anti-roll in production axle dispatcher
- built-in ride-height/skyhook actuator option
- semi-active damper controller option
- deterministic versioned runtime serialization
- runtime validation/duplicate-authority contract
- mixed 150-vehicle portable certification

## Deliberately not duplicated

- Chapman strut: existing strut provider
- pure trailing arm: SUSP13 semi-trailing provider with zero sweep
- De Dion: existing rigid-axle location provider with differential kept sprung
- 3/4/5-link: SUSP14 multi-link provider with the corresponding link count
- anti-dive/anti-squat/roll-centre/camber/toe migration: emergent from hardpoint geometry, not separate canned force functions

## Remaining external gate

The package cannot modify source code that is absent from the attachment runtime. Therefore the merged checkout still has to prove its live VehicleSystem/Lua/Studio calls. The strict wiring validator exists specifically to stop another header-only milestone from being mislabeled production-complete.
