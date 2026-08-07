# Physics Architecture

## Time domains

- Rendering is variable-rate and interpolated.
- The general rigid-body world currently runs at 240 Hz.
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

## Scale strategy

A 150-car event cannot run every distant vehicle, AI controller, collision shape, and tire at maximum fidelity at all times. Future systems require physics LOD, AI LOD, networking interest management, and deterministic transitions between fidelity tiers.

## Determinism

Networking-critical state uses fixed-step native calculations, stable iteration order, explicit input state, and reproducible data. Runtime tests should compare state hashes across render rates once the serialization layer exists.

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
