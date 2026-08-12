# ADR-025: Suspension Geometry Is Chassis Data, Not Wheel Fitment Data

**Status:** Accepted  
**Date:** 2026-08-09

## Decision

Suspension pickup points, linkage geometry and assisted-authoring estimates belong
to the chassis/suspension package. Installing a different rim or tire must not
regenerate or move those pickup points.

Wheel/tire fitment is downstream of the suspension upright/hub. Diameter, width,
offset, tire section, rolling radius, mass and inertia may change contact-patch
location, track, scrub radius, clearance and vehicle response, but they do not
move chassis pivots, strut towers, control-arm pivots or steering-rack anchors.

Assisted suspension estimators therefore consume an explicit immutable
`referencePackageScaleM` authoring value rather than the currently installed
tire radius. Rebuilding estimated hardpoints may change them only when suspension
authoring evidence/settings change, not when a wheel/tire package is swapped.

## Consequences

- Custom wheels cannot silently rewrite suspension geometry.
- Estimated geometry remains reproducible and provenance-labelled.
- Later wheel-fitment simulation can calculate scrub radius, track change,
  clearance, rolling radius, unsprung mass and rotational inertia against one
  stable suspension reference.
- Better GLB/measured hardpoints can still replace estimates point-by-point.
