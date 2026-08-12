# ADR-019 — Steering Coordinate and Ackermann Contract

## Status
Accepted for STEER03.

## Context
Heritage native vehicle coordinates are +X right, +Y up, +Z forward. A positive
rotation about +Y therefore turns a wheel toward +X (right). Racing United's
Lua input layer previously used the opposite scalar convention (positive =
left), while native suspension geometry used positive = right. That hidden sign
change made it possible for input, Ackermann assignment, tire basis and GLB
wheel presentation to disagree.

The first Peugeot prototype also needs a deterministic definition of what
`maximumAngleDegrees` means and which wheel receives the inner/outer Ackermann
angle.

## Decision

### One native steering sign
From STEER03 onward the vehicle steering scalar and road-wheel angle use:

- `-1` / negative road-wheel angle = **LEFT** turn.
- `+1` / positive road-wheel angle = **RIGHT** turn.
- `0` = straight ahead.

The Racing United input adapter converts named actions into that convention once:
`Steer Right - Steer Left`. No later layer is allowed to invert the sign again.

### Ackermann geometry
`maximumAngleDegrees` is the maximum **virtual steering-axle centre / bicycle
model angle**. With non-zero track and full Ackermann, the inside physical wheel
may therefore have more lock than this centre value and the outside wheel less.

For centre angle `delta`, wheelbase `L`, and steering track `T`:

1. `R = L / tan(abs(delta))`
2. `inner = atan2(L, R - T/2)`
3. `outer = atan2(L, R + T/2)`

`ackermannPercent` blends between parallel steering (`0`) and the geometrically
ideal solution (`1`). Anti-/over-Ackermann values remain available for research
and vehicle-specific tuning.

For a left turn the negative-X wheel is inside. For a right turn the positive-X
wheel is inside.

### Suspension geometry and tire basis
The signed road-wheel angle is passed unchanged into the native suspension
geometry evaluator. Steering-axis inclination, caster-derived axis direction,
camber and toe then produce the authoritative wheel basis. Tire longitudinal and
lateral directions are derived from that basis. Visual wheel presentation must
consume the authoritative native pose; it must not independently mirror or
reconstruct steering signs per side.

### Steering response tuning
The existing Peugeot-oriented prototype remains:

- centre lock: 38 degrees
- Ackermann: 1.0
- steering rate: 260 deg/s
- return rate: 360 deg/s
- high-speed steering-rate factor: 0.35
- high-speed reference: 40 m/s

High-speed tuning currently changes steering **rate**, not maximum road-wheel
lock. A UI reset restores these definition values if experimental sliders have
been changed.

## Verification
The native regression suite must verify at full lock that:

- left command produces negative FL and FR angles and both wheel-forward vectors
  point toward -X;
- right command produces positive FL and FR angles and both wheel-forward
  vectors point toward +X;
- left turn: `abs(FL) > abs(FR)`;
- right turn: `abs(FR) > abs(FL)`;
- left/right magnitudes are symmetric within tolerance.

The Vehicle/Drive panel exposes raw left/right action values, native input,
front road-wheel angles and front wheel-forward vectors so future visual binding
bugs can be distinguished from steering-physics bugs immediately.
