# ADR-022: Suspension Hardpoint Authoring Contract

**Status:** Accepted; estimation restriction superseded in part by ADR-024

## Context

The first suspension provider intentionally uses a straight raycast path plus
alignment curves. Real vehicles need mechanism-specific geometry, but guessing
linkage points merely to activate a MacPherson, wishbone or trailing-arm solver
would create false engineering data.

The Peugeot 206 prototype also demonstrates why "front suspension" and "rear
suspension" cannot be treated as one interchangeable blob: its front and rear
use different kinematic and spring arrangements.

## Decision

VehicleDefinitionV2 suspension components may carry zero to 32 creator-authored
hardpoints. Every hardpoint has a stable lowercase ID and a finite chassis-local
position in metres. Hardpoint sets are authored per corner; the tooling never
assumes left/right mirroring. The native compiler validates and preserves them regardless
of whether the currently requested provider is runnable.

Hardpoint absence is allowed for compatibility providers such as
`linear_raycast_v1`. Hardpoint-based providers must later declare their required
IDs and refuse current-solver readiness when those points are missing. The engine may also consume explicitly labeled/versioned estimated hardpoints under ADR-024; estimates must never be presented as measured data.

Racing United exposes non-physical suspension authoring gizmos before hardpoint
providers become active. Debug visualization is never an input to physics.

Kinematics, spring medium, damping and anti-roll coupling are separate
mechanisms. A vehicle may therefore combine, for example, trailing-arm
kinematics with torsion-bar springing and a distinct anti-roll bar without
creating a bespoke all-in-one category solver.

## Consequences

- Vehicle assets can progressively acquire measured/GLB-authored linkage data
  without changing schema or public component IDs.
- Front and rear axles may use fundamentally different mechanisms in one
  vehicle definition.
- Future providers can be strict about completeness while legacy/current
  definitions remain loadable.
- Tooling can report missing engineering data or use explicitly labeled, low-confidence estimates instead of hiding guesses as authoritative data.
