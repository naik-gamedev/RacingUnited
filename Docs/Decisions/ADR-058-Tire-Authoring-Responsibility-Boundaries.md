# ADR-058 — Tire authoring responsibility boundaries

**Status:** CLEAN11 implementation candidate — 2026-08-10

## Decision

The public `.tir` import API remains `Vehicles/Tires/MagicFormula/TirePropertyFile.hpp`, but implementation ownership is split under `Vehicles/Tires/Authoring/`. Parsing/units, generic mapping, Magic Formula coefficients, common metadata, Heritage-owned extension metadata, structural/enveloping metadata, and diagnostics/validation are separate compiled responsibilities. `TirePropertyFile.cpp` is only the public orchestration/file-I/O façade.

Tire parts are reusable authoring objects rather than properties inherently owned by a particular vehicle. `TirePartDefinition` therefore exists independently of cars, motorcycles, trucks, ATVs, karts or future trikes. Its simple creator-facing bias contract includes Dry, Wet, Snow/Ice, Mud, Sand, Gravel and Wear/Endurance values bounded to [-1,+1], with zero representing the future dimension/construction-derived average baseline.

## Physics rule

Creator bias values never multiply final tire force directly. They are future inputs to coherent parameter generation/calibration: wet bias may influence drainage/compound behavior, snow/ice may influence cold compound/siping/stud-capable traits, mud/sand/gravel may influence tread/void/flotation/shear behavior, and endurance may bias wear/thermal durability. Advanced measured or imported engineering data remains authoritative when supplied.

## Compatibility

CLEAN11 does not change active tire equations, `TirePropertyFileData`, the Lua vehicle property-file API, or current vehicle definitions. The new tire-part contract is compiled and validated but is not yet consumed by the runtime.
