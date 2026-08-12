# ADR-024: Assisted Suspension Hardpoint Estimation

**Status:** Accepted

## Context

Heritage Engine needs mechanism-specific suspension geometry, but a small
creator team will rarely have factory CAD or coordinate-machine measurements for
every vehicle. Requiring authoritative hardpoints before a real provider may run
would leave many vehicles permanently on simplified suspension despite having
useful known data such as wheel centres, chassis package dimensions,
suspension family and approximate alignment.

At the same time, inferred geometry must never be mistaken for measured data.
An estimate is useful engineering input only when its epistemic quality remains
visible and it can be replaced progressively.

## Decision

Suspension hardpoints carry independent `provenance` and `confidence` metadata.
Supported authoring sources may include `measured`, `asset_authored`,
`legacy_authored` and `estimated`. Simulation consumes coordinates; provenance
is retained for creator tooling, validation and later data upgrades.

Heritage Engine may provide deterministic mechanism-specific estimators. An
estimator must:

- use only explicit known inputs plus a documented versioned profile;
- emit all inferred points as `estimated`, never as measured or asset-authored;
- attach a conservative confidence value;
- reject nonsensical inputs rather than manufacture arbitrary output;
- produce separate per-corner geometry without assuming hidden mirroring;
- allow higher-quality sources to replace individual inferred points; and
- be regression-tested for finite, non-degenerate and physically plausible
  kinematic behavior.

`estimated_macpherson_road_v1` is the first profile. It uses wheel centre, an
explicit chassis suspension-package reference scale, approximate caster and
steering-axis inclination to construct a compact road-car MacPherson package.
SUS03B adds `estimated_trailing_arm_torsion_bar_road_v1` for the rear mechanism.
Neither profile derives chassis pickup points from the currently installed
wheel/tire package; ADR-025 makes fitment a downstream concern. Their coordinates
are not factory measurements. Profiles are versioned so future improvements
cannot silently change an existing authored vehicle.

GLB vehicle nodes may supply suspension hardpoints using stable node names such
as `SUS_FL_strut_top_mount` or semantic extras. Asset-authored coordinates have
higher priority than estimates and may therefore upgrade a vehicle without
changing the suspension provider contract.

This ADR supersedes ADR-022 only where ADR-022 said the engine must never
manufacture unmeasured linkage locations. The stronger rule is now: **the engine
must never manufacture unmeasured locations without labeling them as estimates.**

## Consequences

- A vehicle can use real linkage kinematics before factory-quality geometry is
  available.
- Creator tools can honestly distinguish what is known from what is inferred.
- Assets can improve incrementally: estimated -> GLB-authored -> measured.
- Versioned estimation profiles prevent silent physics drift.
- Estimated geometry still requires validation against photos, service data,
  alignment data and driving/telemetry whenever better evidence becomes
  available.
