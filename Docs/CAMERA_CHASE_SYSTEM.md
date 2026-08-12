# Heritage Engine Chase Camera Contract

## CAM04 — Swept-Sphere Collision

The chase camera is a spring-damped vehicle-follow camera whose desired pose remains locked to the vehicle's actual interpolated rigid-body forward basis. Collision is read-only and must never push or wake gameplay bodies.

### Collision volume

CAM04 replaces the centre-line camera ray with a swept sphere. The default provisional radius is **0.32 m** with **0.08 m** additional surface padding. The sphere is swept from a chassis-height collision anchor to the full desired eye position. If it hits, the camera contracts immediately; when clear, it springs back outward.

The swept-sphere query sees both ordinary primitive colliders and creator-authored `Scene_*.glb` static triangle collision. This protects the view in ditches and against sharp terrain/wall corners where a single centre ray could be clear while the edge of the camera/near plane clips geometry.

### Future per-vehicle authoring

Camera geometry and feel should be vehicle data, not hard-coded globally. A future Heritage Editor/SDK camera-tuning panel should allow a creator to move/preview the chase rig live and save a vehicle profile containing at least: distance, eye height, target height, longitudinal/lateral offset, FOV/pitch where applicable, spring/damping values, heading-lag limits, jump response, collision radius/padding and minimum distance. Optional authored GLB camera anchors may supplement numeric offsets.

The Peugeot 206 RC is the first tuning reference; other cars, motorcycles, karts and trucks may override the defaults independently.
