# SUSP08 — Rigid Live-Axle Linkage Kinematics

SUSP08 introduces Heritage's first explicitly non-independent suspension provider,
`live_axle_v1`. The two wheel centres belong to one rigid axle body rather than
being solved as unrelated corners.

## Authority

The rigid axle body owns the paired wheel-centre geometry. Equal wheel travel
produces axle bounce; differential travel produces axle roll while preserving the
authored wheel-centre separation. A Panhard rod constrains lateral axle motion and
left/right trailing links constrain the longitudinal axle path. The resulting
lateral/longitudinal movement feeds SUSP05, so linkage scrub changes the actual
1 kHz tire support query rather than presentation only.

VehicleSystem snapshots every wheel's previous 1 kHz compression before entering
the per-wheel solve. Each live-axle corner therefore evaluates against the same
paired state instead of observing a partner that may already have been updated in
the current loop. `suspensionAxleId` is the preferred pairing authority; a
same-provider, opposite-side/same-longitudinal fallback exists for older data.

## Force elements

Left and right spring and damper attachment points move with the rigid axle.
Actual shaft-length changes and finite-difference instantaneous motion ratios are
computed from the live geometry. Spring and damper therefore need not share the
same leverage or orientation. The common suspension force model maps shaft forces
back to wheel force through those current ratios.

An anti-roll bar remains an optional force element. The rigid axle itself already
couples wheel geometry; an ARB does not create that kinematic coupling.

## Seventeen-point hardpoint contract

- axle centre
- left/right wheel centres
- Panhard chassis/axle mounts
- left/right trailing-link chassis/axle mounts
- left/right spring chassis/axle mounts
- left/right damper chassis/axle mounts

Incomplete or degenerate definitions are rejected by the VehicleDefinition
compiler/loader rather than silently falling back to independent suspension.

## Deliberate boundary

SUSP08 is the reusable rigid-axle linkage core. It does **not** fake a leaf spring
as a generic coil force element. Leaf-pack bending, shackle geometry, interleaf
friction/hysteresis, longitudinal compliance, axle wind-up/wrap and tramp belong
to a dedicated descendant built on this same rigid axle body. Likewise, complete
axle housing rotational inertia/torque-reaction dynamics are a later refinement,
not a reason to duplicate the wheel-path solver.
