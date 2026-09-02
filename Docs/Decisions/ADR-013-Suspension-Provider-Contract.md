# ADR-013: Suspension Provider Contract

Status: Accepted in Step 29L.

## Context

The first vehicle solver stored spring and damper scalars directly on each
wheel and evaluated them inside `VehicleSystem`. That was sufficient for a
massless linear raycast wheel, but it could not honestly describe pushrod,
MacPherson, double-wishbone, live-axle, leaf-spring, kart-flex, or motorcycle
linkage systems.

## Decision

Suspension is a reusable authored component with a stable ID. Contact units
reference suspension components; the native definition compiler resolves those
references before a runtime vehicle can be created.

Every executable suspension provider returns bounded spring, damping, and
normal forces through `SuspensionModelInput` and `SuspensionModelOutput`.
`linear_raycast_v1` was the first implementation. The same contract now has
runnable native descendants for MacPherson strut, trailing-arm/torsion-bar,
double wishbone, pushrod/rocker double wishbone, rigid live axle,
leaf-spring live axle, motorcycle fork/swingarm, kart chassis and five-link
independent suspension. Provider IDs for future linkage systems may still be
stored and exported, but they do not become runnable until a matching native
implementation exists.

Vehicle classification is not consulted when choosing a suspension provider.

## Consequences

- Spring and damper data no longer live ambiguously on contact records.
- Multiple contacts may reference reusable suspension components.
- Unsupported layouts remain valid authoring data and receive an explicit
  unresolved-provider diagnostic.
- Implemented hardpoint providers own linkage geometry, wheel pose, camber/toe
  where applicable, and axle coupling; unsupported future layouts remain
  unresolved rather than being faked by the linear model.
- Anti-roll coupling remains a separate reusable mechanism rather than being
  hidden inside individual kinematic providers.
