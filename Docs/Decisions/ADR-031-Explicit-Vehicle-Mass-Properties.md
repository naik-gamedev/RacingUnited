# ADR-031: Explicit Vehicle Mass Properties

**Status:** Accepted / MASS01  
**Date:** 2026-08-09

## Context

Heritage already separates the creator-facing chassis origin from the physical center
of mass, so lateral tire forces can create real roll torque. Rotational inertia was
still largely inferred from attached collision primitives, however. That makes a
collision proxy an accidental handling parameter: changing a body collider, visual
bodywork, wheel fitment or another creator-facing asset could silently alter pitch,
yaw or roll response even when the vehicle's actual mass distribution had not been
re-authored.

Exact mass-distribution and inertia measurements will rarely be available for creator
vehicles. Racing United therefore needs an evidence-aware estimate path, while still
allowing CAD/component reconstruction or measured values to replace estimates later.

## Decision

Vehicle mass properties are explicit simulation data, independent from collision
shape geometry.

A compiled vehicle body may carry:

- total mass in kg;
- body-local center of mass;
- diagonal local inertia in kg*m^2;
- front/rear and left/right static load fractions as authoring evidence;
- provenance and confidence.

Heritage local axes are X=right, Y=up, Z=forward. The current diagonal inertia fields
therefore correspond to pitch inertia about X, yaw inertia about Y, and roll inertia
about Z.

`RigidBodySystem` owns an explicit local-inertia override. Once present,
`CollisionSystem` continues to use attached shapes for contact generation but must
not overwrite the authored/estimated inertia while rebuilding collider-derived mass
properties.

The initial `VehicleMassPropertiesEstimator` uses broad, versioned radius-of-gyration
priors from vehicle class, wheelbase, track, total mass, COM height and static load
fractions. These priors are engineering starting points, not named-vehicle lookup
claims. The Racing United compact-road-car prototype is deliberately low confidence
(`estimated_mass_properties_road_car_v1`, confidence 0.20).

MASS01 also establishes `VehicleMassPropertiesAccumulator`. Installed components can
later contribute mass, local COM and component inertia, and the accumulator combines
them with the parallel-axis theorem. This is the intended path for wheels/tires,
spacers, batteries, bumpers, wings, cages, cargo and other modifications to influence
total mass, COM and inertia without moving suspension hardpoints.

## Precision and tensor scope

High-rate vehicle mechanisms continue to use FP64 `VehicleScalar` where already
specified. The current generic rigid-body storage uses a diagonal FP32 inertia vector,
so MASS01 does not claim that the entire rigid-body solver is FP64 or that it supports
a full 3x3 inertia tensor. The public mass-property contract is deliberately isolated
so a future full-tensor/greater-precision rigid-body backend can be introduced without
changing creator vehicle definitions.

For ordinary approximately symmetric road vehicles, a body-axis diagonal tensor is a
useful first representation. Strongly asymmetric vehicles or movable cargo may later
justify products of inertia and principal-axis orientation.

## Customization rule

Wheel/tire/offset/spacer changes must never regenerate suspension pickup geometry.
They may alter installed component mass, unsprung mass, total mass, COM and inertia.
Likewise, body customization may affect mass properties only when the modification has
mass/location evidence; visual mesh replacement alone must not change physics.

Aerodynamic changes from bumpers, diffusers, spoilers or wings are separate from mass
properties, although a physical part may legitimately contribute to both systems.

## Validation

MASS01 regressions require:

- bounded, deterministic low-confidence whole-vehicle estimates;
- component accumulation to obey the parallel-axis theorem;
- explicit inertia to survive collider mass-property rebuilds;
- equal angular impulses to produce axis response inversely proportional to the
  explicit pitch/yaw/roll inertia;
- VehicleDefinitionV2 compilation/loading to preserve and apply explicit mass, COM,
  inertia, static-load evidence, provenance and confidence.
