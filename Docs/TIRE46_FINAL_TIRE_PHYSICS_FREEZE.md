# TIRE46 — Final Tire Physics Freeze

TIRE46 closes the tire-physics architecture before the suspension completion phase.

## Completion changes

- `Distributed3x3` is the native default for every vehicle; Lua no longer opts player/AI vehicles in.
- The 150-car / 600-tire benchmark exercises distributed contact on the entire fleet.
- Thermal construction expands to seven nodes: tread, belt, carcass, inner sidewall, outer sidewall,
  contained gas and rim, while retaining the rotating 16x3 tread surface field. Structural-loss heat
  is energy-conserving across belt/carcass/sidewalls and invalid >100% authoring partitions are rejected.
- Failure state expands from coarse puncture/blowout stages into independent construction coordinates:
  cuts/leaks, bead retention, belt/cord/sidewall fatigue, graining, blistering, delamination, rim
  damage and optional run-flat support.
- Explicit damage incidents are available to native/Lua gameplay and laboratory callers.
- Public MF6.2 PHYP turn-slip lateral-shift semantics are active rather than parse-only.
- Motorcycle rounded-crown geometry owns the actual support/contact query at lean instead of being
  telemetry only. Motorcycle tires inherit the same distributed contact, thermal, wear, wet and
  damage systems.
- Prototype `.tir` files explicitly author the new thermal/damage nodes and PHYP coefficients.
- Import diagnostics recognize all TIRE46 Heritage-owned thermal/damage keys; unknown vendor fields
  remain honestly reported instead of being silently accepted.

## Epistemic calibration rule

TIRE46 does not require proprietary tire-rig or molecular data to declare the solver complete.
Historical tires can use evidence-informed reconstruction with explicit provenance/confidence.
Future research improves parameter data without reopening the frozen tire architecture unless a
missing physical mechanism is demonstrated.

## Validation

The native Linux C++20 regression executable built from the TIRE46 source and completed with all
regressions passing, including universal distributed contact work counts, PHYP sign behavior,
motorcycle high-camber contour, seven-node thermal behavior and extended damage/endurance.
Windows/MSVC Release build and shipping-performance timing remain the user's normal build/run gate.
