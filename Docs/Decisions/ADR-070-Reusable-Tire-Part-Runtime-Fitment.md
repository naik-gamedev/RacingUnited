# ADR-070: Reusable tire-part runtime fitment

**Status:** Accepted for TIRE17C (2026-08-11)

## Context

TIRE17A established family-derived tire baselines and TIRE17B established bounded creator bias mapping. A reusable tire asset still needed a single runtime resolution path so cars, motorcycles, karts, trucks and other topologies can reference the same tire definition without copying a full `TireModelDescription` into each vehicle.

The tire part and the vehicle fitment also own different physical facts. The part owns dimensions, family, load/reference-pressure engineering data and optional identified property data. The fitted vehicle/wheel owns the operational cold inflation pressure because the same tire may require different pressures on another axle, vehicle or load condition.

## Decision

- `TirePartDefinition` is a reusable part identity with engineering dimensions/load/reference pressure, family, creator biases and optional authoritative `.tir` provenance.
- `TirePartResolver` is the one native bridge from a reusable part to a runtime `TireModelDescription`.
- If an explicit `.tir` is authored and loads successfully, it is authoritative over family/bias estimates.
- If no `.tir` is authored, TIRE17A family generation plus TIRE17B bias mapping creates an explicitly estimated runtime model.
- `TirePartFitment` may override cold inflation pressure per wheel without modifying the reusable part definition or its reference pressure.
- `VehicleSystem::assignWheelTirePart` installs the resolved model, records part identity/source and resets tire thermal/wear state for the new fitment.
- Low-level tire-model/provider/property-file overrides clear reusable-part assignment identity. Heritage must not retain stale metadata after the resolved model has been manually replaced.
- Per-wheel assignment is the TIRE17C primitive. Topology-aware axle/group assignment and editor/Parts Lab workflows build on this primitive rather than hard-coding four-wheel assumptions.

## Consequences

One tire definition can be reused by multiple vehicles or multiple wheel positions while operational pressure remains fitment-specific. Manufacturer/model strings remain metadata only. Runtime provenance makes it possible for later tooling to distinguish generated estimates from identified property data.
