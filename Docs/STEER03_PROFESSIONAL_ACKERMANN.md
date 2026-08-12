# STEER03 — Professional Ackermann Steering Audit

This pass fixes the steering sign contract and rewrites the Ackermann calculation
around an explicit virtual steering-axle centre model.

## Key behavior
- Native vehicle sign: **negative = LEFT, positive = RIGHT**.
- Racing United input adapter converts named actions once (`Right - Left`).
- No later steering/suspension/tire layer inverts the sign.
- `maximumAngleDegrees` is the virtual centre/bicycle-model angle.
- At 100% Ackermann the inside wheel receives greater lock and the outside wheel less.
- Existing Peugeot prototype tuning remains 38 deg centre lock, 1.0 Ackermann,
  260 deg/s steering rate, 360 deg/s return rate, 0.35 high-speed rate factor,
  40 m/s high-speed reference.

## Diagnostics
Vehicle -> Drive now displays:
- raw left/right action values;
- native steering input;
- FL/FR road-wheel steering angles;
- FL/FR wheel-forward X/Z vectors;
- detected wheelbase and steering track.

There is also a **RESET STEERING FROM VEHICLE DEFINITION** button so temporary
slider experiments cannot leave the test car in a mystery state.

## Warning cleanup
The FP64 vehicle pass had left many implicit `VehicleScalar` (double) -> `float`
conversions in VehicleSystem.cpp. STEER03 makes intentional FP32 boundaries
explicit and keeps internal accumulators FP64 where appropriate. The file was
checked with conversion warnings enabled and produced zero implicit-conversion
warnings in that audit.

## Verification
The full headless physics regression suite passes, including a new steering test:
- left: FL -45.41 deg / FR -32.43 deg;
- right: FL +32.43 deg / FR +45.41 deg;
- both wheel-forward vectors point into the requested turn;
- left/right Ackermann magnitudes are symmetric.
