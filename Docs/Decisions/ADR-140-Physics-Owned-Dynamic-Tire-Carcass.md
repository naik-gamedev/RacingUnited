# ADR-140 — Physics-Owned Dynamic Tire Carcass

## Status

Accepted for TIRE44 implementation, 2026-08-29. Live visual calibration remains required before the new carcass can be called validated.

## Context

The TIRE41 visual flexible-ring path was a bounded single-field improvement over stacked shader dents, but its runtime authority was still an instantaneous presentation solve. In particular, the bridge could consume the scalar physical tire deflection/loaded radius as a prescribed radial shape target. A scalar hub-to-road deflection is a force/contact state; it is not a unique three-dimensional carcass shape. Under load this could pull the lower carcass inward, and force-related rigid-ring motion could make the error visually worse.

A proposed TIRE43 support-plane presentation lock was rejected before integration. Snapping rendered tread points to a derived support plane would hide the symptom without solving the structural problem and would create a second geometric authority downstream of physics.

## Decision

TIRE44 moves the flexible carcass into `VehicleSystem` as persistent per-wheel physics state.

The carcass is a deterministic reduced-order 24 x 13 displacement/velocity lattice. When requested for visible presentation it advances at 125 Hz from the existing 1000 Hz wheel path. Each structural step solves inertia, damping, circumferential/lateral carcass coupling, pressure-dependent structural stiffness, moving rigid-ring attachment, tread-patch torsion/flat-spot inputs, external road contact and internal rim/flange contact in one bounded implicit solve. Because this milestone does not feed the lattice back into MF6.2 forces, unseen carcasses are not advanced: the readback API renews a short simulation lease. This preserves the large-grid CPU budget without changing tire-force physics.

Road contact is not reconstructed in the renderer and is not replaced by a global or local support plane. The carcass reuses the actual collision points, normals and explicit misses already sampled by the tire-force road-envelope path. Supported samples enter as unilateral structural contact constraints. Explicit misses remain misses so road edges do not become infinite planes. Each structural node is associated with its nearest existing road-envelope query once per 125 Hz step; only the lookup is cached, while the true point/normal constraint is solved on every implicit iteration.

The physical `tireDeflection` / loaded-radius state remains valid for tire force/contact geometry, suspension support and MF6.2 inputs, but it is never converted directly into a carcass-node displacement command. Wheel-centre placement plus collider intersection creates the structural contact demand naturally.

The rim/flange is an internal unilateral boundary in the same structural solve. It is not a post-deformation radial clamp.

`Entity.SetMeshNodeTireFlexibleRingFromWheel` becomes copy-only. It resolves the wheel and publishes the already-simulated field to the visible/shadow tire mesh. It performs no collision query, equilibrium solve, support-plane lock or tire-force calculation.

MF6.2 and the existing contact-patch pipeline remain the traction-force authority. TIRE44 does not infer grip from rendered vertices. The dynamic carcass consumes the same physical pressure, contact and rigid-ring states, but promotion of simulated carcass geometry back into force/contact authority requires a separate measured-validation milestone.

## Consequences

- The visible tire has time history and structural velocity instead of jumping between render-time equilibrium shapes.
- Static load, acceleration and braking no longer need a prescribed visual radial-collapse target.
- Road, kerb and edge response comes from collision samples already owned by tire physics.
- Flat/low-pressure tires may collapse farther, but cannot pass freely through the rim/flange structural boundary.
- Unseen/non-requested carcasses may stop advancing because TIRE44 does not feed them into tire forces; a visible readback request restarts the physics-owned state. This prevents 600 visual carcass lattices from consuming the 150-car CPU budget.
- TIRE41's render-time `LuaEntityTireFlexibleRingBridge` is retired from the build.
- The old deterministic equilibrium evaluator may remain only as historical/regression utility; runtime code must not call it.
- This is still a reduced-order simulation, not a full finite-element tire. Its stiffness/mass/damping parameters require live calibration and eventually measured tire data.
## TIRE45B runtime cache correction — 2026-08-29

The TIRE44 ownership decision remains unchanged, but one road-envelope cache representation violated
its coordinate contract. `CarcassRoadSampleCache` retained absolute world-space sample points while
the cached envelope was reused across later 1 kHz wheel substeps. Phase 06 then subtracted the current
wheel centre from those older world points. Any vehicle translation during the cache lifetime therefore
appeared to the carcass as a moving/asymmetric road constraint. The error scaled with speed and cache
age and could accumulate in the persistent structural state even on ordinary forward rolling.

TIRE45B stores cached sample points as world-direction offsets from the centre contact measured at the
same road-envelope refresh. When the structural solver consumes the cache, it adds that offset to the
live centre contact and only then converts the reconstructed point into the current wheel frame. Cached
normals remain world-space directions. Thus the cache represents local road *shape* rather than stale
world position. Real asymmetric geometry such as a kerb or road edge is preserved, while translation of
the vehicle itself cannot create a false sidewall/tread constraint.

A TIRE45A development hypothesis attempted to convect the complete 24x13 persistent state by wheel
angular velocity. That hypothesis is rejected and is not part of this ADR: the circumferential lattice
stations are Eulerian wheel-frame locations. Material-fixed effects such as flat spots are mapped into
those spatial stations using `wheelRotationRadians`; ordinary wheel spin must not advect the whole
field.
