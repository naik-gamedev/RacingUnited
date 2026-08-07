# Heritage Vehicle Workshop

## Purpose

Step 29J.6 establishes the first versioned, topology-first authoring contract
for ground vehicles. A category such as car, Formula car, IndyCar, kart,
sprint car, ATV, motorcycle, or truck selects convenient starting data in the
editor. It does not select a separate physics engine.

A complete `VehicleDefinitionV2` describes:

- presentation assets and their coordinate convention;
- one or more rigid bodies;
- zero or more power units;
- zero or more transmissions;
- one or more reusable suspension components;
- wheel, track-patch, or other ground-contact units;
- explicit transmission-to-contact drive connections; and
- required provider capabilities such as lean dynamics, articulation, or
  continuous track contact.

This admits ordinary four-wheel cars, single-seaters, karts, multi-axle
trucks, trailers, motorcycles, and independent front/rear powertrains without
putting vehicle-category conditionals in the generic loader.

Step 29J.6A makes the Workshop controls responsive. Topology templates and
related choices wrap into two-column rows instead of disappearing beyond the
right side of narrower panels.

Step 29J.6B uses three topology columns when at least 540 pixels are available,
then automatically returns to the 29J.6A two-column arrangement in narrower
panels. Longer asset and output actions remain in two columns.

Step 29K sends every refreshed definition through the native compiler. The
provider shown in the Workshop is therefore the engine's answer, and supported
live preview is created by `VehicleDefinitionLoader` rather than by Lua-side
drive-layout reconstruction.

Step 29L moves spring, damper, travel, motion-ratio, and force-limit data into
stable suspension components. Contact units reference those components by ID.
Templates now request their intended suspension family; the Workshop does not
silently run a Formula pushrod, kart-flex, motorcycle-linkage, or truck
live-axle definition through the linear provider.

Step 29M definitions also retain non-linear spring, damper and travel-stop
parameters. The first values are authored defaults rather than measured vehicle
data; the Dynamics Lab force and damper-power channels exist so they can be
replaced through repeatable testing instead of visual guesswork.

Step 29O definitions retain unsprung mass and radial tire stiffness, damping,
deflection and load bounds per contact unit. The live Vehicle `SUSP. > UNSPRUNG`
page can tune these values on the running prototype, while the Workshop remains
the topology-first source for exported definitions.

## Current workflow

1. Run `Tools/LaunchExactFreshRelease.cmd`.
2. Enter the prototype and open `Vehicle > WORKSHOP`.
3. Choose a starting topology.
4. Edit its ID, name, mass, torque, body/power/transmission/contact counts,
   gearing, driven layout, power-unit placement, and provider requirements.
5. Type an Assets-relative OBJ path or use the Windows asset picker. The picker
   accepts only an OBJ already beneath the active module's `Assets` directory.
6. Read the structural validation report.
7. Apply configurations supported by the current native solver as a live
   prototype, or export any structurally valid definition.

The draft is stored in the module save store. Exported definitions are written
below:

`UserData/Modules/RacingUnited/Saves/VehicleWorkshop/`

They are data artifacts, not automatically installed source files. A creator
reviews an export and deliberately moves the final definition into the
module's `Scripts/Vehicles/Definitions/` content tree.

## Honest capability boundary

The Step 29L `raycast_wheel_v1` provider can live-preview a single-body,
single-power-unit, single-transmission, four-wheel configuration with 1 to 16
forward ratios. Its suspension components must request `linear_raycast_v1`.
The preview uses the current raycast-wheel solver; changing a template does not
pretend that Formula suspension, kart chassis flex, sprint-car stagger, or ATV
tires have already been modeled.

Definitions requiring any of the following remain valid and exportable but are
marked as awaiting native providers:

- multiple rigid bodies or articulation;
- multiple power units or transmissions;
- two-wheel lean and large-camber motorcycle dynamics;
- continuous track contacts;
- contact counts other than the current four-wheel preview; or
- drivetrain capabilities beyond the current native ratio limit.

This distinction is permanent: definition validity and current-solver support
are separate answers. Tooling must never silently discard or fake unsupported
topology.

## Security and module isolation

Lua remains sandboxed from arbitrary filesystem access. `Module.AssetExists`
and `Module.SelectAssetFile` are read-only and constrained to the active
module's Assets directory. `Module.WriteSaveText` writes only bounded `.lua`,
`.json`, or `.txt` content beneath that module's private Saves directory.

## Next extensions

1. Add visual gizmos for centers of mass, wheel/contact centers, suspension
   anchors, steering axes, and collision volumes.
2. Implement the first linkage-geometry provider after its anchors can be
   authored and inspected.
3. Replace scalar engine torque with reusable measured torque-curve assets.
4. Add component-graph native powertrain routing for multiple engines,
   transmissions, differentials, transfer cases, chains, and hub motors.
