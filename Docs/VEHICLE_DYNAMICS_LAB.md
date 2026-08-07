# Vehicle Dynamics Laboratory

## Purpose

The Vehicle Dynamics Laboratory records authoritative native vehicle state for
repeatable diagnosis and comparison. It is not a render-frame graph and it is
not a second vehicle solver. `VehicleSystem` feeds the recorder from the same
high-rate substeps that calculate suspension, tires, brakes and steering.

Recording is opt-in per vehicle. An inactive recorder owns no sample buffer,
which keeps ordinary race vehicles free of laboratory storage and sampling
cost. A capture is bounded by duration, frequency, wheel count and an absolute
240000-sample safety limit.

## Prototype use

Open `Prototype -> Vehicle -> Lab`. The first laboratory UI provides:

- Manual driving capture from 2 to 60 seconds.
- A parked-settle experiment.
- A 250 mm suspension drop experiment.
- A deterministic straight acceleration/braking experiment.
- A deterministic turn-then-brake stability experiment.
- Native summary statistics and downsampled plots.
- CSV export to the active module's private save directory.

Automatic experiments temporarily provide throttle, brake and steering input
at the fixed world rate. Manual recording never takes control from the player.
The generated CSV is written below:

`UserData/Modules/RacingUnited/Saves/DynamicsLab/`

## Recorded data

Every native sample contains vehicle-local velocity and angular rates, position,
driver inputs, steering state and engine speed. Every wheel contributes ground
contact, suspension compression and velocity, normal/longitudinal/lateral
forces, steering, angular velocity, relaxed slip, grip utilization and aligning
torque. Step 29M additionally records separate spring, damper, bump-stop and
droop-stop forces plus instantaneous damper dissipation in watts. Step 29O adds
unsprung velocity, radial tire deflection and velocity, and radial damping
dissipation. Its summary reports peak wheel-hop speed, tire deflection and
radial damping power. Step 29P records authoritative camber and toe per contact,
adds peak absolute values to the summary, and exposes both plot and CSV channels.

Graphs are downsampled with a peak-preserving bucket selection. CSV retains
every captured sample and should be used for detailed offline analysis.

## Rules

1. Capture from native fixed/substep code, never from rendered transforms.
2. Keep recording explicitly opt-in so a large race field does not allocate
   telemetry buffers accidentally.
3. Add metrics to the native frame and CSV before presenting them in Lua UI.
4. Keep automatic scenario input in `Vehicles/DynamicsLab.lua`; do not hide
   scenario-specific behavior inside the production vehicle solver.
5. Add objective regression assertions for every numerical bug reproduced by a
   laboratory scenario.
6. Preserve complete captures when stopping. Clear or begin a new run only when
   replacement is intentional.

The present laboratory is the instrumentation foundation. Hardpoint motion,
wheel rate, tire temperature, pressure, damper velocity histograms, thermal
state and linkage-load channels will be added as those production systems
exist. Scalar unsprung-mass motion, radial-tire forces, camber and toe are now
captured; full hardpoint motion and linkage loads remain later work.
