# Suspension sign-off rule

The physics core in this package closes the remaining abstraction gaps found after SUSP26.

## Current repository status (2026-08-31)

The Windows Release regression executable now compiles and executes the SUSP27-SUSP30
certification as part of the ordinary `HeritagePhysicsTests` project.  It proves the bounded
physical-element graph, deterministic state round-trip, component breakage/degraded dynamics,
provider-catalog completeness and a 150-vehicle synthetic workload.  This is certification of the
new core, not evidence that every live vehicle has already migrated to it.

The current `VehicleSystem` still runs the established SUSP05-SUSP13 provider/scalar force path.
That path is tested and must remain the authority for existing vehicles, including the Peugeot,
until each vehicle has an explicit V3 element graph and a calibrated provider adapter.  Never
auto-convert a vehicle behind the author's back or run V2/scalar and V3 forces in parallel.

The remaining production migration gate is:

1. split the 1000 Hz wheel pass into contact sampling and force application so all current corner
   coordinates are available to one whole-vehicle V3 graph step;
2. provide real V3 frame/derivative/reaction-load adapters for every canonical geometry provider;
3. compile stable element/frame IDs from module vehicle definitions and expose them in Lua and
   Heritage Studio;
4. persist V3 state in save, replay, rollback and later network snapshots;
5. transition broken constraints to degraded multibody dynamics; and
6. prove an authored V3 vehicle against the current calibrated vehicle before making V3 default.

Until those gates pass, the accurate label is **V3 core certified; live migration pending**.

Do **not** call the live repository suspension-complete merely because the headers compile. The final sign-off requires the live integration audit to pass.

When it passes, the intended terminal milestone is:

`SUSP30_SUSPENSION_DOMAIN_COMPLETE`

At that point future vehicle work should consist of authored hardpoints/topology, masses/inertias, component specifications and calibration—not new suspension physics architecture.
