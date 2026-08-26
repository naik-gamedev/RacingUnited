# LIVETRACK01 — Persistent Sensor Dynamic Surface

## Goal

Make wet-track simulation behave like a racing-game dynamic track rather than a centimetre-scale CFD experiment. Puddles, wetness, tire drying and hydroplaning must share one persistent world state, while the GPU presentation reconstructs smooth/organic detail without owning a second simulation.

## Authority

- World is partitioned into existing 100 m Dynamic Surface chunks/surface sheets.
- Hydro authority is 64 x 64 sensor cells per 100 m page (1.5625 m per cell).
- The same Hydro state is sampled by tire physics and modified by tire contacts.
- Water and moisture remain in the Hydro page map when a page leaves the active residency set, so returning to a section resumes its session state instead of creating a fresh puddle map.
- Static authored collision/surface-sheet data supplies support height, exposure, roughness, infiltration, drainage and depression storage.

## Runtime budget

Only pages within 350 m of real simulation-interest sources are active. Active pages receive atmospheric forcing at 2 Hz. Each 0.5 s Hydro publication interval performs rain/drainage/evaporation once and subdivides downhill shallow-sheet transport into steps no larger than 0.05 s.

This deliberately trades centimetre-scale fluid detail for a bounded sensor field that is appropriate for racing dynamics.

## Tire dry line

Tire contacts operate on the same persistent Hydro state used by tire physics. Contact load, tread void ratio, swept distance and slip energy clear water from the contacted sensor, redistribute a conservative fraction forward, and report the remainder as spray/evaporation. Repeated traffic therefore creates a drier line in the actual physics authority, not just in a visual mask.

## Presentation

The existing Dynamic Surface GPU page pool mirrors only resident 64 x 64 Hydro/support pages. The ordinary authored material shader bilinearly reconstructs the scalar water field and adds bounded world-space optical breakup at the shoreline. Optical breakup is sub-millimetric and cannot erase meaningful standing-water depth.

A 0.35 mm mobile film is treated primarily as wet material response. Water above that film capacity contributes to visible standing-puddle response, with a soft onset from roughly 0.1 mm to 3.5 mm of excess depth.

## Retired path

`DynamicSurfaceGpuRuntime` remains compiled temporarily to minimize project/ABI risk, but LIVETRACK01 never initializes or updates it and hard-disables GPU Dynamic Surface authority every frame. Therefore the former 10 m / 512 x 512 atlas is not allocated and its CFD compute shaders are not dispatched.

The retired configuration reserved a 32768 x 24576 R32UI water atlas (3.0 GiB) plus a same-size R8 presentation atlas (0.75 GiB), before optional snow/mud state and other resources. LIVETRACK01 removes that runtime requirement.

## Persistence scope

Persistence is session/world-state persistence in RAM, not save-game serialization. Active work is bounded; dormant Hydro state remains available for revisiting track sections during the session. Future save-game/session serialization can encode the same sensor pages without changing tire or renderer interfaces.

## Build workflow

`Tools/00_BuildAndRunCurrent.cmd` remains the canonical build-and-run helper. LIVETRACK01 adds no milestone-specific build script.
