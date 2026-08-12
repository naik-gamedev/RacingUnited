# Heritage Engine Numerical Precision Policy

## Goal

Use precision where it protects simulation correctness, while keeping the GPU
and large race fields fast enough for real-time play.

The policy is deliberately *not* speed-dependent. A vehicle never changes
numeric format because it crossed a velocity threshold. That keeps handling,
replays, networking and debugging deterministic across the whole operating
envelope.

## P64-01 implemented policy

### CPU high-rate vehicle dynamics: FP64

`heritage::vehicles::VehicleScalar` is `double` (IEEE-754 binary64 on the
supported desktop C++ targets).

The following native 1000 Hz vehicle paths now evaluate and retain their
important scalar state in FP64:

- advanced road-tire model inputs, coefficients and force outputs
- suspension spring / damper / bump-stop / droop-stop model
- scalar unsprung-mass / tire-radial-compliance model
- wheel compression and suspension velocity telemetry
- tire deflection and tire radial damping state
- wheel angular velocity
- tire slip ratio and slip angle state
- longitudinal / lateral tire forces
- normal load, grip utilization, pneumatic trail and aligning torque
- wheel drive / brake torque state
- high-rate tire relaxation arithmetic
- chassis torsional-compliance stiffness/damping/modal state and diagnostics

Lua numeric values are already represented as doubles. Tire, suspension and
unsprung-mass model bindings therefore keep those values in FP64 instead of
truncating them to FP32 before entering the native provider.

### Explicit FP32 boundary remains

The current rigid-body world, collision geometry and render-facing `Vec3`
storage still use FP32. When an FP64 wheel force is converted into the current
FP32 rigid-body impulse vector, `VehicleSystem` performs an explicit cast at
that boundary rather than silently pretending the entire physics world is
already double precision.

This is intentional. P64-01 strengthens the highest-rate, most numerically
sensitive vehicle solver without simultaneously rewriting collision, entity,
scene and renderer coordinate contracts.

## P64-02 implemented large-world policy

P64-02 adds an FP64 `DVec3` absolute-world layer and a floating local physics
origin. The player chassis is the current Racing United anchor. Once it moves
4096 m from the local origin, all bodies, fixed world anchors, static scene
triangles and root entities are shifted together while the FP64 world origin
accumulates the exact local-frame shift.

The renderer now submits mesh/debug geometry camera-relative: the GPU camera is
`(0,0,0)` and object positions are small FP32 values. D32 floating-point
reversed-Z remains the depth policy.

Vehicle wheel mounts remain local FP32 geometry, but high-rate point-velocity
work now consumes chassis-relative offsets directly instead of subtracting two
large world coordinates. Tire/suspension high-rate scalar state remains FP64.

The current single-scene static-triangle BVH is rebuilt when an origin rebase
occurs. Large streamed free-roam maps should later move to region/chunk-local
collision so rebasing does not touch an entire world mesh. See
`P64_02_FLOATING_ORIGIN.md`.

## Race-field scaling policy

Precision is not simulation LOD.

Important vehicles may use the full high-rate solver while distant or
non-critical vehicles can run lower-rate/simplified simulation tiers. The
numeric format remains stable inside a given solver so a car does not suddenly
change handling because of speed or distance.

This is essential for large events such as 100+ vehicle endurance fields:
spend CPU time on physical detail where it is observable, not on dynamically
switching floating-point formats.

## Explicit FP64 -> FP32 boundaries (STEER03/WARN01)

High-rate vehicle state uses `VehicleScalar` (`double`) where P64-01 selected
FP64. Some legacy configuration, rigid-body, collision-query and telemetry
structures intentionally remain FP32. Conversions at those boundaries must be
**explicit** (`static_cast<float>`) rather than implicit narrowing assignments.

This is not merely cosmetic: warning-clean builds make accidental precision
loss visible again. Internal tire/suspension/drivetrain accumulators should stay
FP64 when they already use `VehicleScalar`; only the documented FP32 boundary
should narrow the value.

STEER03/WARN01 audited `VehicleSystem.cpp` under conversion warnings and removed
implicit FP64 -> FP32 conversions while preserving the regression results.
