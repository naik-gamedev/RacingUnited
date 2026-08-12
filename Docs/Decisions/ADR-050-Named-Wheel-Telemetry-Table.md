# ADR-050 — Named Wheel Telemetry Table

## Status

Accepted for CLEAN01; Windows/user validation pending.

## Context

By TIRE15, `Vehicle.GetWheelState` returned 169 positional Lua values. That design failed twice in
ways unrelated to tire physics itself: the first-party Lua unpack exceeded Lua 5.4's local-variable
limit, then the native binding required an explicit large `lua_checkstack` reservation to avoid a
runtime crash while pushing the payload.

The API was also becoming difficult to maintain because a field's meaning depended on a historical
numeric position. Every new tire/surface milestone increased the return count and required native
and Lua positional order to remain perfectly synchronized.

## Decision

Add `Vehicle.GetWheelTelemetry(vehicle, oneBasedWheelIndex)` as the preferred wheel readback API.
It returns one Lua table whose fields are named according to the existing Racing United telemetry
contract.

CLEAN01 includes in the same table:

- the complete legacy `GetWheelState` data set;
- contact-query/support diagnostics previously read through `GetWheelContactDiagnostic`;
- authoritative upright/basis orientation previously read through `GetWheelUprightPose`.

Racing United's first-party `Vehicles/Telemetry.lua` consumes only this named table.

The three positional APIs remain registered for compatibility during the transition. They are not
extended for first-party use unless a compatibility requirement demands it. New tire/surface
telemetry should be added to `GetWheelTelemetry` by name.

The table is intentionally flat in CLEAN01 because the existing Lua/UI/presentation code already
consumes flat field names. A future versioned/nested public telemetry schema may be added if it
provides measurable value, but CLEAN01 does not force a broad consumer rewrite merely for nesting.

## Consequences

Positive:

- one Lua return slot regardless of telemetry field count;
- no 200-local unpack failure;
- no giant return-payload C-stack requirement for first-party telemetry;
- field additions do not renumber existing fields;
- contact and upright state come from the same native wheel snapshot;
- TIRE15C/TIRE16 and later work have an extensible diagnostics destination.

Costs:

- one Lua table allocation per wheel per telemetry refresh;
- legacy APIs temporarily remain duplicated;
- external scripts using the old positional ABI are not automatically migrated.

The telemetry path is for tooling/presentation/debug readback, not the authoritative high-rate
physics solver, so the table allocation is acceptable. If profiling later shows this path is hot,
the API can reuse tables or expose a bounded snapshot object without returning to a positional ABI.
