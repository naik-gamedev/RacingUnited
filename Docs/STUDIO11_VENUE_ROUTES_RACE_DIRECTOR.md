# STUDIO11 — Venue Routes, Timing Gates and Race Director Foundations

STUDIO11 is an additive Heritage Studio milestone. No existing Scene, Race, Traffic, Gameplay, Weather, Vehicle, Audio or Assets authoring capability is intentionally retired.

## HRACE v3

HRACE v3 retains all HRACE v1/v2 race markers and persistent race configuration, then adds named venue routes, ordered spline nodes, layouts, session chains, race-control configuration and support infrastructure. HRACE v1/v2 remain loadable; older files receive an in-memory default Main Circuit route and Grand Prix layout so they can be saved forward without discarding legacy markers.

Route nodes form cubic Bezier splines. Every node stores position, incoming/outgoing tangent handles, automatic-tangent preference, independent left/right legal-driving corridor widths, target speed, banking and overtaking-preference metadata. Main circuit, pit lane, safety-car, formation, alternate-layout, sprint, hillclimb and drag route roles are available.

## Timing and grids

Existing marker authoring remains available and is expanded with directional gate dimensions. Start/Finish, checkpoint, sector, timing-loop, speed-trap start/finish, pit entry/exit/speed line, Safety Car line and formation line objects expose gate width, gate height and crossing-direction enforcement.

Grid generation now supports staggered two-wide, two-wide, three-wide, single-file and endurance-angled templates with editable row spacing, lateral spacing and back offset. Generation fills missing numbered slots only and preserves authored slots.

## Layouts

A Race Layout binds a named race route, optional pit route and Start/Finish marker. The same physical venue can therefore carry GP, club, alternate and reverse layouts without duplicating the scene. Reverse layout duplication is available and validation reports a reverse layout that points at a route not marked reverse-capable.

## Session chain and race control

Practice, qualifying, warm-up, race, time-attack and test sessions are persistent ordered records with duration/lap mode, mandatory pit stops, formation/rolling-start rules, weather-change permission and starting fuel percentage.

Race control persists local yellow, FCY, VSC, Safety Car, red flag and blue-flag capability switches, Safety-Car pit-lane behavior, track-limit warning thresholds, pit windows, Safety-Car route and restart-line references. These are authored authorities for later runtime state-machine depth; they do not remove the existing event penalty fields.

## Marshal and recovery infrastructure

Venue support points now cover marshal posts, recovery vehicles, tow trucks, medical, fire crew, race control, Safety Car standby and timing equipment. Each carries position, heading, service radius and sector metadata and can be placed on the scene surface.

## Gameplay/runtime bridge

HGAME v2 adds an optional venue-layout reference to events while retaining HGAME v1 loading. Circuit and clandestine-circuit events can therefore bind directly to a Studio-authored venue layout.

Generated `Scripts/Generated/StudioGameplay.lua` is version 2 and publishes race routes, route nodes, layouts, sessions, race control and support points alongside the existing traffic, event and world-point data. `Runtime/StudioGameplay.lua` exposes layout resolution, ordered route-node access, checkpoint sequences, session chains, race-control data, support-point lookup and event-to-venue resolution.

## Validation

Validation now checks timing-gate dimensions, route-node references, closed-loop node counts, duplicate route node order, corridor widths, layout references, reverse permission, session duration/lap definitions, session order, Safety-Car/restart references, pit-window order, support-point radii and event layout references in addition to the STUDIO10 checks.
