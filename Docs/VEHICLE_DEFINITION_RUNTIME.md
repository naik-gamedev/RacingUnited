# Native Vehicle Definition Runtime

## Purpose

Step 29K makes `VehicleDefinitionV2` an engine-consumed contract instead of an
editor-only Lua shape. The native path has three deliberately separate layers:

1. `VehicleDefinitionV2Source` preserves authored IDs, components and provider
   requests.
2. `VehicleDefinitionCompiler` validates bounded data, resolves string
   references into indices, derives drivetrain contact weights and selects a
   compatible runtime provider.
3. `VehicleDefinitionLoader` adapts the immutable compiled definition into the
   selected native solver.

This boundary prevents gameplay scripts from manually reconstructing the same
component graph each time a vehicle is spawned.

## Provider selection

Provider selection is based only on components and requirements. The
`classification` field remains metadata and may contain `car`, `motorcycle`,
`truck`, a fictional label, or anything else without changing solver choice.

The first provider is `raycast_wheel_v1`. It currently accepts:

- one primary rigid body;
- one combustion power unit;
- one manual or direct transmission with 1 to 16 forward ratios;
- one drivetrain route reaching at least one contact;
- exactly four wheel contacts;
- resolved `linear_raycast_v1` suspension components;
- `advanced_road` tire contacts; and
- no requested lean, articulation or continuous-track capability.

Other topologies can still be structurally valid. The compiler reports
`unresolved` plus the exact missing provider capabilities instead of deleting
components or pretending a four-wheel car solver implements them.

## Lua boundary

`Vehicle.CompileDefinitionV2(definition)` returns native validity, current
solver readiness, provider ID, a summary and diagnostic text. The Workshop uses
this result as the authoritative live capability report.

`Vehicle.CreateFromDefinitionV2(definition, chassisBody, ...)` compiles and
immediately loads the definition through the selected provider. No pointer to a
Lua table is retained. The bridge bounds every component collection before
copying it into native source data.

The Step 29K loader owns creation of the native vehicle record, powertrain,
gear ratios, resolved drivetrain weights, wheel/contact descriptions,
suspension parameters and brake factors. The prototype still creates its
temporary entity, chassis body and box collider outside the loader; moving
those presentation/collision concerns into reusable compiled definitions is a
later extension.

## Regression contract

The headless native suite verifies that:

- valid references compile into exact component indices;
- drivetrain contact weights are derived from explicit connections;
- the current provider creates four wheels and the authored gear count;
- a missing component reference is rejected;
- a leaning two-contact definition remains valid but is not falsely runnable;
- classification metadata does not influence provider selection; and
- all earlier 1000 Hz dynamics regressions continue to pass.

## Suspension provider graph

Step 29L adds `suspensions` as an explicit component collection. Each contact
references a suspension by stable ID, and the native compiler resolves that
reference into an index. `SuspensionModel` is the high-rate force boundary;
the current `linear_raycast_v1` provider returns spring, damper, and bounded
normal force while honoring an authored motion ratio.

Formula, IndyCar, kart, sprint-car, ATV, motorcycle, and truck templates now
retain honest provider requests such as pushrod double-wishbone, kart chassis
flex, motorcycle linkage, and live-axle leaf suspension. These definitions are
valid and exportable but remain unresolved until those native providers exist.

The next extension is authoring and visualization of linkage anchors, steering
axes, centers of mass, inertia, and collision volumes. That geometry will let
the first non-linear provider calculate authoritative wheel pose, camber, toe,
and motion ratio rather than approximating them in Lua.

Step 29M extends every suspension component with healthy non-linear force data:
preload, spring progression, low/high-speed bump and rebound damping, velocity
knees, bump/droop stops, and travel-limit progression. The compiler validates
bounded finite parameters before the loader copies them into the selected
native provider. See `SUSPENSION_MODEL.md`.

Step 29O extends each contact unit with effective unsprung mass, radial tire
stiffness/damping, maximum tire deflection and maximum tire normal load. The
compiler validates these values and the loader configures the independent
native `UnsprungMassModel`. Effective mass zero is valid and selects the legacy
massless contact path. See `UNSPRUNG_MASS_MODEL.md`.
