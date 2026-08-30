# STUDIO19 — Competitors, Grids & Complete Motorsport Weekends

STUDIO19 adds the competition layer above STUDIO18 race execution.

## Data ownership

- **HRACE v5:** session/weekend sporting rules: timed races, time-plus-one-lap, stint limits, refuelling, tire changes, minimum pit service, classification and grid source.
- **HGAME v5:** reusable competition structure: motorsport classes, entrants, teams/vehicle presets, AI traits, championships, rounds and points schemes.

## Runtime

`Runtime/MotorsportWeekend.lua` builds grids, registers RacingEvents participants, produces deterministic logical qualifying order, drives replaceable assetless race proxies along the authored venue route, captures results and persists championship points. This is intentionally representation-independent so final Heritage vehicles/Racing AI can replace debug proxies later without rewriting weekend/points/timing logic.

## Grid sources

Event order, previous session, qualifying, championship standings, or reverse top N. Manual per-entrant grid overrides remain available.

## Endurance

Race sessions can be timed, optionally time-plus-one-lap, impose stint limits, control refuelling/tire changes, require tire service, set minimum stationary pit-service time and define classification percentage.

## Final integration additions
- Competition relationships are selected through Studio dropdowns rather than requiring numeric ID entry for entrant event/class, championship class, and calendar event/championship references.
- Championship results are persisted per round. Overall standings are recalculated from completed rounds and apply the authored `dropWorstRounds` rule.
- Pole-position and fastest-lap bonuses are applied to championship scoring; round point multipliers apply to base and bonus points.
- Racing AI participants publish best-lap telemetry into the shared event standings, allowing fastest-lap award calculation and future multi-class timing consumers.
- `RacingMotorsport.GetClassStandings(classId)` exposes class-filtered results while retaining overall position.
