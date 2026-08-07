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
- `raycast_linear` suspension contacts;
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

## Next extension

The next vehicle milestone should replace the single `raycast_linear` choice
with a suspension component/provider graph. Geometry, mass distribution and
inertia tooling can then compile through the same boundary without changing
the Workshop's versioned envelope.
