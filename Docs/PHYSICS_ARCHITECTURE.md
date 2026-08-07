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

`Engine/HeritageEngine/Tests/PhysicsRegression.cpp` is the first headless native
regression suite. It runs the same chassis, suspension, tire, brake, collision,
and rigid-body code as the game and fails the process when parked stability,
sleep/wake, rate behavior, or slope behavior leaves its numeric bounds.

## Surface metadata

Colliders carry generic `SurfaceMaterial` and normalized wetness. Raycasts and
sphere casts return this metadata with the hit. Vehicle code consumes it per
wheel, while collision response itself remains independent from vehicle tire
models. Do not reintroduce a scene-wide or vehicle-wide surface assumption when
the contacted collider can provide the authoritative material.

## Creator static triangle query bridge (Step 29J.4)

`PlayerScene_Collision.obj` may supply exact static triangles to the read-only
world-query path. Vehicle suspension/tire raycasts can therefore follow a real
sloped Blender terrain without approximating an entire terrain object as one
AABB. These triangles are deliberately **not yet** part of the rigid-body
contact solver; full chassis-vs-static-mesh collision requires a production
static-mesh/convex contact system plus spatial acceleration and belongs to later
world-physics work. Do not mistake the read-only query bridge for final mesh
collision.
