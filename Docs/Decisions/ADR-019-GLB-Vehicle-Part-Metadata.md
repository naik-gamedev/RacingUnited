<!-- Compatibility path retained for older repository validators/tools.
Canonical decision number: ADR-021. Do not treat this filename as a second ADR-019. -->

# ADR-021 — GLB Vehicle Part Metadata

## Status

Accepted for VA01.

## Decision

Heritage Engine treats Blender/glTF metadata as authoring information, not as a
direct physics mutation mechanism.

Blender Custom Properties exported through glTF node `extras` are preserved as
generic scalar metadata. A vehicle-specific `VehicleAssetMetadata` layer then
discovers stable slots and replaceable parts.

Stable functional slots (for example `WH_RR_Tire`) remain separate from actual
part IDs (for example `Pirelli_PZero_Nero_205_40_ZR17_84W`).

The four wheel corners remain independent. A vehicle may therefore use different
front/rear wheel widths, tire sizes, manufacturers or configurations without a
special-case axle-global asset representation.

## Consequences

- GLB remains portable and inspectable by normal tooling.
- Blender is the source authoring environment; Heritage does not require a
  proprietary vehicle container.
- Racing United can build a Wikipedia-like vehicle/part information UI from the
  same fields used by compatibility and later simulation systems.
- Estimated values can be explicitly labelled as estimates instead of being
  confused with authoritative measurements.
- Runtime part replacement and physics consumption remain explicit later
  systems, preventing renderer metadata from silently changing simulation.
