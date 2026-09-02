# SUSP09 — Leaf-Spring Live-Axle Dynamics

`SUSP09_LEAF_SPRING_LIVE_AXLE_DYNAMICS` adds the physical leaf pack to the
SUSP08 rigid live-axle authority instead of creating a second axle solver.
`live_axle_leaf_v1` therefore keeps one pair-coupled axle body, one Panhard /
trailing-link axle path, and one SUSP05 physical tire-support offset while the
leaf packs own their compliant bending, shackle motion, hysteresis and axle
housing torque reaction.

## Provider contract

The provider reuses the complete SUSP08 seventeen-point live-axle package and
adds eight leaf/shackle hardpoints:

- left/right leaf front eye
- left/right rear shackle chassis pivot
- left/right leaf rear eye
- left/right axle clamp

A complete `live_axle_leaf_v1` definition therefore carries 25 named points.
Incomplete or degenerate packages are rejected by the native compiler/loader.

## Leaf geometry

The SUSP08 rigid axle moves the axle clamps. For each side, SUSP09 then solves
the rear leaf eye from two rigid geometric constraints: the authored rear-leaf
segment length and the authored shackle length. The valid circle intersection
nearest the rest configuration supplies the moving rear eye and real shackle
angle.

Leaf bending is represented by the signed axle-clamp camber/sag relative to the
instantaneous front-eye to rear-eye chord. Change from the authored rest sag is
the generalized leaf compression. Finite differentiation of that coordinate
provides the instantaneous wheel-to-leaf motion ratio, so leverage can change
through bump and droop instead of using a fixed coefficient.

The existing spring preload/rate/progression parameters describe the effective
leaf-pack bending law. They operate on actual leaf compression. The direct
SUSP08 damper remains geometrically independent and retains its own shaft
compression and motion ratio.

## Interleaf hysteresis

Leaf-pack hysteresis is not folded into shock damping. It has an explicit
Coulomb-plus-viscous interleaf friction law operating at leaf-shaft velocity.
The resulting force is mapped to the wheel through the instantaneous leaf
motion ratio. Separate telemetry reports interleaf force and dissipated power,
so leaf-pack energy loss cannot masquerade as damper work.

## Axle wrap and tramp

Drive/brake tire longitudinal reaction torque excites one paired axle-housing
rotational state. The reduced-order housing mode has authorable torsional
stiffness, damping, inertia and maximum angle. Integration uses a natural-
frequency-bounded internal step so the same mechanism remains stable across
supported physics rates.

Both wheels on the axle share the same wrap angle/rate. The state can feed a
bounded vertical/jacking coupling while remaining separate from the primary
rigid-axle translation/roll solve. This captures wind-up/release and wheel-hop /
tramp excitation without pretending the axle housing is another independent
wheel suspension.

## Authority boundaries

- SUSP08 remains the sole rigid axle-pair location/roll authority.
- SUSP05 remains the sole hardpoint-derived tire-support scrub authority.
- SUSP09 leaf geometry owns leaf compression/shackle motion only.
- SUSP09 interleaf friction is separate from the shock absorber.
- SUSP09 axle-wrap state is a paired housing mode driven by physical tire
  longitudinal reaction torque.
- Tire physics remains frozen under TIRE46.

The provider is intentionally suitable for linked leaf-spring live axles; a
future specialized historical layout may relax individual locator components
when evidence says the leaves themselves are the sole axle locators, without
replacing this shared rigid-axle core.
