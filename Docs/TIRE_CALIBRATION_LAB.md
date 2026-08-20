# Tire Calibration Laboratory

## Purpose

TIRE18 converts tire development from subjective tuning into repeatable evidence. The native
steady-state laboratory evaluates the exact `TireModelDescription` used by the vehicle solver and
retains complete force, moment, stiffness, trail and operating-point telemetry for every sample.

This laboratory does not decide whether a curve is realistic without evidence. A fitted dataset may
legitimately omit a pressure, camber or turn-slip dependency. In that case the laboratory must show a
flat curve; it must never manufacture sensitivity to make a plot appear more interesting.

## Native API

`Vehicles/Tires/TireCalibrationLab.hpp` provides:

- `TireCalibrationAxis` — canonical SI sweep axes;
- `TireCalibrationSweepDescription` — baseline operating point plus one or two axes;
- `runTireCalibrationSweep` — deterministic ordered evaluation;
- `standardTireCalibrationSweeps` / `runStandardTireCalibrationSuite` — the TIRE18A evidence set;
- `exportTireCalibrationSweepCsv` — complete CSV output for external plotting and fitting.

The generic two-axis representation supports both curves and maps without embedding one UI or one
vehicle topology in the tire solver. The secondary axis is disabled with `None` and exactly one
sample. Work is bounded to 262,144 samples per sweep.

## Standard TIRE18A suite

| Sweep | Range | Samples | Baseline held constant |
| --- | --- | ---: | --- |
| `pure_longitudinal` | slip ratio -0.30..0.30 | 121 | zero slip angle/camber, nominal load/pressure |
| `pure_lateral` | slip angle -12..12 degrees | 121 | zero longitudinal slip/camber, nominal load/pressure |
| `combined_slip_map` | slip ratio -0.20..0.20 and slip angle -12..12 degrees | 41x49 | nominal load/pressure |
| `load_sensitivity` | 0.50..1.60 nominal load | 45 | 6-degree slip angle |
| `pressure_sensitivity` | 0.55..1.45 reference pressure, clipped to fitted validity | 45 | nominal load, 6-degree slip angle |
| `camber_sensitivity` | camber -6..6 degrees | 49 | nominal load/pressure, 4-degree slip angle |
| `turn_slip_sensitivity` | turn slip -2..2 1/m | 81 | nominal load/pressure, 6-degree slip angle |

The ordinary suite stays inside the fitted hard-road model's pressure range. Zero-pressure failure,
carcass collapse, thermal transients and puncture progression are separate stateful scenarios; they
must not be inferred by extrapolating MF polynomials outside their identified range.

## Units and sample order

Native input is SI:

- forces in newtons;
- moments in newton-metres;
- pressure in gauge pascals;
- angles in radians;
- speed in metres per second;
- turn slip in inverse metres.

CSV includes degree and PSI convenience columns in addition to canonical SI values. Samples are
ordered secondary-axis-major, then primary-axis-minimum to primary-axis-maximum. Given identical tire
data and sweep description, repeated runs must produce identical ordering and values.

## Evidence and acceptance

`tireCalibrationLabProducesDeterministicSweeps` verifies:

- all seven standard sweeps are valid and complete;
- repeated runs agree numerically;
- braking/driving and left/right slip force directions are correct;
- the combined map has the declared dimensions;
- increased load produces increased absolute lateral force at the test operating point;
- pressure endpoints remain finite and ordered without requiring invented pressure coefficients.

Passing these structural tests does not prove commercial accuracy. Measured tire-rig data, documented
conditions, parameter provenance and curve envelopes remain the TIRE18D calibration gate.

## Installed-tire integration: TIRE18B

The in-game Tire/Vehicle Lab selects an installed wheel, runs native sweeps against its resolved
tire part, plots selected channels, retains independent A/B runs and exports CSV plus a manifest
containing:

- tire part identity and family;
- parameter source/provenance/confidence;
- fitted validity envelope;
- wheel fitment pressure and dimensions;
- engine build identity and sweep description.

The UI must not reimplement force equations or silently normalize curves. It is a viewer/controller
for the native evidence runner.

## Stateful scenarios: TIRE18C

The same panel runs a stable common-schema scenario set:

- relaxation step response;
- slip heating followed by airborne/rolling cooling;
- sustained-cornering thermal wear;
- locked-wheel flat-spot formation;
- brake-to-rim heat soak followed by rim/carcass cooldown;
- slow-puncture pressure loss;
- blowout pressure loss and structural progression.

Scenario CSV and plots expose forces, slip state, tread/carcass/gas/rim temperatures, pressure,
wear/flat spots and failure state. World-surface-specific providers retain their dedicated native
regressions; the lab does not fabricate a fake world merely to put every mechanism into one menu.

## Acceptance envelopes: TIRE18D

`TireCalibrationAcceptance` owns a provenance-labelled curve-envelope format and checks:

- finite force, moment and stiffness output;
- fitted load/pressure/slip/camber validity;
- pure-slip force-sign conventions;
- adjacent-sample force and moment continuity;
- enabled Fx/Fy/Mx/My/Mz minimum/maximum envelopes.

The repository regression builds an explicitly synthetic relative envelope, proves an unchanged run
passes, and proves a mutated or topologically mismatched run fails. This is calibration
infrastructure—not measured commercial tire data. Future lawful rig datasets must state source,
conditions, units, confidence and whether the reference is synthetic.

## Distributed contact and scalability: TIRE18E/F

`Distributed3x3` is a bounded local brush around one calibrated whole-tire force target. It preserves
the aggregate result on homogeneous support while resolving split friction and partial support into
local force and moment contributions. The player vehicle enables it explicitly; aggregate remains
the scalable baseline.

The deterministic work-bound regression for 150 four-tire vehicles reports 600 whole-tire
evaluations and 36 local brush cells when only the player uses the spatial tier. A real 150-car scene
CPU profile remains required before making a platform-performance claim.
