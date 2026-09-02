# SUSP10 — Motorcycle Fork / Swingarm / Linkage Suspension

SUSP10 adds the first motorcycle-specific suspension providers without changing
TIRE46 tire authority or pretending that rider/balance dynamics belong inside a
suspension solver.

## Front: `motorcycle_telescopic_fork_v1`

The authored upper/lower steering-stem points define one physical steering and
fork-slide axis. Positive suspension compression slides the axle along that
axis; steering rotates the complete lower-fork/axle package around the same
axis. The provider therefore exposes real axle-centre movement, wheelbase
change, steering-axis position and fork rake instead of applying a visual-only
front-wheel rotation.

The SUSP05 support-query bridge consumes that axle-centre path, so fork travel
and steering alter the actual high-rate tire support query. The spring and
damper are direct acting and retain 1:1 shaft-to-fork motion for a conventional
telescopic fork.

Required hardpoints:

- `steering_stem_upper`
- `steering_stem_lower`
- `wheel_center`

A conventional telescopic fork does not receive an invented anti-dive force.
Braking load transfer remains physical chassis/tire load transfer; future
special anti-dive linkages can be represented by a distinct provider if a
vehicle actually has one.

## Rear: `motorcycle_swingarm_linkage_v1`

The rear wheel and linkage pickup rotate on a rigid swingarm axis. A fixed
length dogbone constrains a chassis-pivoted rocker. Rocker angle is solved from
the length constraint at each suspension pose, and the physical shock shaft
compression and instantaneous motion ratio are derived from the moved rocker
pickup. This allows real rising/falling-rate rear suspension rather than a
constant motion-ratio approximation.

The authored countershaft center is retained as physical chain-line geometry.
The derivative of countershaft-to-axle distance with wheel compression is
combined with previous-step tire longitudinal force, effective wheel radius and
rear sprocket pitch radius using virtual work. The resulting bounded vertical
chain jacking term represents geometry-dependent anti-squat/squat without a
magic throttle multiplier.

Required hardpoints:

- `swingarm_pivot_left`
- `swingarm_pivot_right`
- `wheel_center`
- `linkage_swingarm_mount`
- `rocker_pivot_left`
- `rocker_pivot_right`
- `rocker_link_mount`
- `shock_chassis_mount`
- `shock_rocker_mount`
- `countershaft_center`

## Explicit boundary

SUSP10 completes the suspension mechanisms needed by a conventional motorcycle
front fork and rising-rate rear swingarm/linkage. It does **not** claim to solve
a complete motorcycle vehicle topology. Two-wheel balance, free steering-head
dynamics, rider mass/pose/control, gyroscopic steering coupling and fork/frame
structural flex are vehicle/rider dynamics work built on top of these
suspension providers. VehicleDefinition continues to mark `lean_dynamics` as a
future runtime capability rather than silently faking it.
