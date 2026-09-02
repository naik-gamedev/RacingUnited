# SUSP11 — Kart Chassis Kingpin / Frame Flex

## Decision

A racing kart is not represented as a four-corner sprung car.
`kart_chassis_flex_v1` removes conventional wheel travel and spring/damper force
from the kart provider. Physical support is shared by the frozen TIRE46 radial
tire model and the existing chassis torsional-flex solver.

## Geometry

The provider requires ten hardpoints: left/right front kingpin upper/lower,
left/right front wheel centre, left/right rear axle bearing and left/right rear
wheel centre. Steering rotates each front wheel centre about the authored
kingpin. Caster/KPI therefore create real vertical steering jacking and lateral
wheel motion. SUSP05 preserves the complete kart kingpin displacement in the
1 kHz support query, including vertical jacking. Rear wheel centres are fixed to
the rigid axle and have no independent suspension travel.

## Force authority

`SuspensionModel` returns zero for the kart provider even if nonzero fake spring
values are supplied. The support-query length is fixed at rest length. Tire
radial normal force is applied directly to the chassis, while
`chassis_torsional_mode_v1` owns frame twist. VehicleDefinition rejects nonzero
kart bump/droop and rejects kart definitions without enabled frame torsion.

This lets inside-rear unloading emerge from steering jacking, tire compliance,
load transfer and frame torsion instead of a canned lift percentage.
