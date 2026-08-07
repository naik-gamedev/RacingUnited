# ADR-012: Native Vehicle Definition Compiler and Provider Selection

Status: Accepted in Step 29K.

## Context

The first Vehicle Workshop built and validated a topology in Lua, then applied
supported values by mutating the handwritten prototype and calling low-level
vehicle functions. That proved the authoring model but left two competing
runtime paths and allowed future scripts to interpret component references
differently.

## Decision

Heritage Engine compiles `VehicleDefinitionV2` into an immutable native graph.
The compiler owns structural validation, stable-ID resolution, derived drive
weights and runtime-provider capability matching. A separate loader adapts a
compiled graph into one solver provider.

Classification is never an input to provider selection. Components and
explicit requirements are authoritative.

Unsupported provider requirements do not invalidate otherwise sound authored
data. They prevent runtime creation and produce explicit capability reasons.

Lua tables are copied through a bounded bridge. The native runtime retains no
Lua pointers or table references.

## Consequences

- Workshop preview and future game loading share one native interpretation.
- Invalid references fail before a partially configured vehicle is created.
- Loader failures roll back the native vehicle record.
- New suspension, motorcycle, articulation, track and powertrain providers can
  be added without category branches or schema replacement.
- The temporary chassis entity/collider creation remains module-side until the
  definition gains production collision and mass-property components.
