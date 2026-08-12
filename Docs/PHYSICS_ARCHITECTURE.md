# Physics Architecture

## Time domains

- Rendering is variable-rate and interpolated.
- The general rigid-body world defaults to 120 Hz and is configurable independently.
- Vehicle tire and suspension calculations can run on an independent 1000 Hz clock.
- Force feedback will use its own high-rate path.
- Networking uses lower-rate snapshots plus interpolation/prediction.

Changing render FPS must not change simulated time. The fixed-step accumulator has bounded catch-up and giant-stall protection.

## General physics responsibilities

- Rigid bodies and inertia.
- Primitive collisions and contact solving.
- Sleeping and islands.
- Queries and continuous collision protection.
- Generic constraints.
- Stable handle ownership and body-dependent cleanup.

## Vehicle responsibilities

Specialized vehicle calculations apply forces at contact and attachment points to the general chassis body. Tire and suspension substeps must not force the entire world to run at 1000 Hz.

## Authored body origin versus physical center of mass

A rigid body's authored/entity origin is a reference datum and is not implicitly
the physical center of mass. `RigidBodyDescription.centerOfMassLocal` stores an
optional body-local COM offset while pose/collider/wheel/hardpoint coordinates
remain in the authored-origin frame. Linear velocity is COM velocity; impulses,
contacts, constraints and collider-derived inertia use the physical COM for their
lever arms and parallel-axis terms. Rotational integration preserves COM motion
while allowing an offset authored origin to orbit it. Zero-offset bodies preserve
the legacy behavior. See ADR-028.

This separation is required for real chassis pitch/roll/load transfer. Vehicle
tire forces already act at the contact patch; an elevated physical COM gives
those forces the correct torque arm instead of treating a road-level authoring
datum as the mass center.

### Combined chassis attitude and four-corner response

Pitch, yaw and roll are not independent gameplay modes. They are the three
rotational components of the same rigid-body state and may occur simultaneously
under the same force/impulse accumulation. A braking turn can therefore produce
nose-down pitch, body roll, yaw rotation, different travel at all four suspension
corners, damper motion and anti-roll-bar torque in the same 1000 Hz vehicle loop.
There is no additional "diagonal pitch/roll" degree of freedom; diagonal chassis
attitude is a combination of the existing rotations, while diagonal load is an
observable four-corner suspension/load-transfer quantity. ROLL02 permanently
regression-locks this combined behavior before structural chassis compliance is
introduced. See ADR-029.

### Structural chassis compliance

FLEX01 adds a first torsional compliance mode without replacing the ordinary rigid
chassis. Gross 6-DOF motion remains in `RigidBodySystem`; the structural mode stores a
small relative front-to-rear twist and twist rate. The difference between front and
rear suspension roll reactions drives the mode, while stiffness and damping return it
toward neutral. Virtual suspension pickup frames rotate by the local interpolated
section twist before the wheel/contact solve.

This keeps the effect physical and coupled to four-corner loads while avoiding a
many-body or finite-element shell for every vehicle. The mode is generic: stiffness,
damping, modal inertia, reference stations and evidence provenance are vehicle data.
Estimated values are explicitly low-confidence and replaceable. The rendered body mesh
is not structurally deformed by FLEX01. See ADR-030.

Suspension force providers return explicit spring, damping, travel-stop and
energy-dissipation terms. The linear-raycast provider remains a massless contact
approximation; future linkage/unsprung-mass providers must preserve the same
observable force contract rather than hiding tuning inside vehicle categories.

## Parked contact and sleeping

Raycast suspension impulses are support/contact impulses and therefore keep an
ordinary rigid-body sleep timer awake. `VehicleSystem` owns the additional
parked-state decision for its chassis. A quiet vehicle may sleep on effectively
flat ground without brake input. On a slope it may sleep only when the requested
service/parking brake torque and the contacted tires' friction capacity exceed
the gravity load along the surface. Releasing that brake or applying throttle
wakes the chassis; an unbraked vehicle must remain awake and roll downhill.

Brake torque is applied as a bounded constraint that may stop a wheel at zero
angular velocity but may not overshoot and reverse it in a high-rate substep.
This prevents the millisecond-scale forward/reverse wheel chatter that otherwise
feeds low-speed tire-force oscillation.

## Scale strategy

A 150-car event cannot run every distant vehicle, AI controller, collision shape, and tire at maximum fidelity at all times. Future systems require physics LOD, AI LOD, networking interest management, and deterministic transitions between fidelity tiers.

## Determinism

Networking-critical state uses fixed-step native calculations, stable iteration order, explicit input state, and reproducible data. Runtime tests should compare state hashes across render rates once the serialization layer exists.

`Engine/HeritageEngine/Tests/` contains the headless native regression suite.
`PhysicsRegression.cpp` is intentionally only the runner; shared world/test support
and the collision/terrain, vehicle-dynamics, chassis-dynamics, chassis-flex, suspension, and definition/compiler
regressions live in separate translation units so each domain can grow without
recreating a monolithic test file. The suite runs the same chassis, suspension,
tire, brake, collision, and rigid-body code as the game and fails the process
when an established numeric/behavioral contract leaves its bounds.

## Surface metadata

Colliders carry generic `SurfaceMaterial` and normalized wetness. Raycasts and
sphere casts return this metadata with the hit. Vehicle code consumes it per
wheel, while collision response itself remains independent from vehicle tire
models. Do not reintroduce a scene-wide or vehicle-wide surface assumption when
the contacted collider can provide the authoritative material.

## Creator static triangle world (Step 29J.4 onward)

The preferred Racing United path is a single `Scene_*.glb` containing both
visible geometry and explicitly marked static collision nodes. Blender Custom
Properties (`heritage.role=collision_mesh`,
`heritage.collision_type=static_triangle_mesh`) are preferred; `_Collision` /
`Collision_` node names remain a convenient fallback. Legacy OBJ import remains
available to other/older module content.

Imported triangles are now installed into an immutable `StaticTriangleBvh`. The
same accelerated world participates in suspension/tire raycasts, camera/AI
sphere casts, and first-generation rigid-body contact for dynamic sphere and
box colliders against exact static triangles. Static contacts use bounded local
manifolds, positional correction, cached/warm-started impulses, friction and the
normal velocity solver. Headless regressions require both a dynamic sphere and
a dynamic box to fall onto the triangle world and settle stably.

This is intentionally narrower than a general triangle-mesh collider system.
Static scene triangles do not become ordinary per-node `ColliderHandle` objects,
and Heritage does not yet promise dynamic triangle meshes, mesh-vs-mesh contact,
deformable meshes, or arbitrary concave moving bodies. Those capabilities must
be added deliberately rather than inferred from the static-world path. See
`SCENE_GLB_AUTHORING.md` and `TERRAIN_CONTACT_DIAGNOSTICS.md`.

## Observable terrain contact loss (Step 29Q)

Every wheel now records a native contact status instead of reducing all support
failures to `grounded = false`. The statuses distinguish supported load,
bottom-out, road-without-load, a surface behind the ray origin, static scene
bounds, missing geometry, zero query candidates, exact-test misses and support
beyond droop. The collision query also reports primitive/static candidate counts
and cached scene-bound evidence.

Only a grounded-to-airborne transition performs a reverse diagnostic probe.
This identifies the characteristic query-only tunnelling case without doubling
the ordinary 1000 Hz query cost. It does not apply a corrective impulse. See
`TERRAIN_CONTACT_DIAGNOSTICS.md` and ADR-018.
