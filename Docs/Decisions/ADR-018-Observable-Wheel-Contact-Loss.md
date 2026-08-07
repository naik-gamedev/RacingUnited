# ADR-018: Observable Wheel Contact Loss

Status: Accepted in Step 29Q.

## Context

The creator static triangle scene participates only in suspension/tire queries.
A zero-width support ray can lose the road at a real mesh gap, outside scene
bounds, beyond droop, after the ray origin crosses the surface, or because an
exact triangle test rejects a broadphase candidate. All of those cases were
previously the same `grounded = false` result. Correcting collision before
classifying the failure would make regressions difficult to distinguish from
masked defects.

## Decision

Give every native wheel a stable `WheelContactStatus`, persistent loss-transition
counter, bottom-out measurements and a copy of the support ray's diagnostic
evidence. Cache aggregate static scene bounds on scene installation. Perform one
reverse probe only on a grounded-to-airborne transition to identify a surface
that crossed behind the support-ray origin.

Expose the result through a separate `Vehicle.GetWheelContactDiagnostic` Lua API
instead of extending the already established 51-value wheel-state ABI. Keep all
classification native; Lua only presents the authoritative result.

## Consequences

- Terrain loss is reproducible and named before solver replacement.
- A real hole is distinct from tunnelling, bottom-out and scene-boundary exit.
- Full-rate supported wheels retain one raycast per substep.
- The transition-only reverse probe is diagnostic and cannot recover contact.
- Static triangle query acceleration and rigid-body mesh contacts remain
  Milestone 2 work.
- Future suspension/contact providers must preserve this observable contract or
  explicitly version it.
