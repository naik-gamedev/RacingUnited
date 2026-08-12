# ADR-030: Chassis Torsional Compliance

**Status:** Accepted / FLEX01  
**Date:** 2026-08-09

## Context

Heritage vehicles already use one rigid 6-DOF chassis body for gross translation,
pitch, roll and yaw. A perfectly rigid shell, however, cannot represent the small
front-to-rear torsional deformation produced by diagonal suspension loading. That
compliance can matter to wheel-load distribution and suspension pickup geometry,
while a fully flexible finite-element body would be far too expensive and data
hungry for the project's large vehicle counts.

Exact torsional-rigidity measurements will also be unavailable for many creator
vehicles. Missing factory data must not force the mechanism off, but estimated data
must never masquerade as measured truth.

## Decision

Heritage Engine implements chassis flex as a reusable **first torsional structural
mode** named `chassis_torsional_mode_v1`.

The main chassis remains one ordinary rigid body. FLEX01 adds a small relative twist
state between front and rear structural reference stations:

- creator-facing torsional rigidity is expressed in Nm/degree;
- runtime stiffness, damping, modal inertia, twist and twist rate use
  `VehicleScalar` (FP64);
- gross body roll remains entirely on the rigid body;
- only the difference between front and rear suspension roll reactions drives the
  structural torsion mode;
- front and rear suspension pickup frames receive opposite half-twist, with
  continuous interpolation between their reference stations;
- the flex transform modifies suspension mount/direction/upright geometry before
  contact forces are applied;
- a configurable maximum twist is a numerical/authoring safety bound, not a normal
  operating target.

This is physical simulation state, not a camera effect or cosmetic mesh animation.
The rendered chassis mesh is not yet deformed by FLEX01.

## Estimation and provenance

`ChassisFlexEstimator` may generate low-confidence starting values from broad known
properties such as construction family, model year, mass, wheelbase, track and COM
height. The estimator is intentionally broad and versioned. Its output carries
provenance and confidence and is replaceable by stronger evidence without changing
the solver.

The Racing United prototype currently uses a low-confidence closed-unibody estimate.
It is not claimed to be a measured Peugeot 206 RC torsional-rigidity value.

Future creator modifications such as roll cages, seam welding, braces, structural
damage or open-roof conversions may alter the configured stiffness, but such effects
must remain data/mechanism changes rather than vehicle-name special cases.

## Why not split the chassis into many rigid bodies?

A multi-body shell can represent more modes but adds constraints, solver cost,
network state, tuning complexity and instability risk to every vehicle. The modal
approach captures the first useful torsional compliance effect with tiny state and
cost, making it suitable for large grids and future physics-LOD tiers.

More structural modes may be added later behind the same architecture if evidence and
profiling justify them.

## Validation

FLEX01 regressions require:

- equal front/rear roll reactions to produce no structural twist;
- diagonal/front-rear reaction mismatch to produce a bounded signed twist;
- mirrored loading to mirror the twist sign;
- released twist to decay back toward neutral under stiffness/damping;
- the low-confidence estimator to preserve construction ordering and provenance;
- the mechanism to run inside the real 1000 Hz vehicle loop while remaining stable;
- VehicleDefinitionV2 compilation/loading to preserve and instantiate the component.
