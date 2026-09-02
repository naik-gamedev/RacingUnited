# Static ride-height calibration

Status: `RIDE01` - native static-equilibrium calibration is live.

## Physical contract

Ride height is a chassis datum measured from the road, not a visual body offset
and not the distance from the tire to the wheel arch. For a loaded vehicle at
rest, each corner must satisfy:

`spring/torsion support force = supported corner weight`

The target suspension length also includes tire compression:

`target length = mount height - unloaded tire radius + tire deflection`

where `tire deflection = supported load / vertical tire stiffness` for the
current reduced-order tire model. Spring preload is solved so the suspension
supports that load at the target length. The transient damper is intentionally
absent from this calculation because its velocity, and therefore its force, is
zero after the car has settled.

The native solver currently supports the linear raycast, MacPherson strut and
trailing-arm/torsion-bar providers. It rejects a target outside bump/droop
travel or one that would require negative preload.

## Asset-derived datums

The lightweight GLB inspector reads each mesh POSITION accessor's authored
minimum and maximum bounds and transforms those bounds through the node
hierarchy. It does not decode the complete mesh a second time.

For vehicle assets it:

1. identifies tire geometry and averages the four tire-bottom heights into the
   authored road plane;
2. uses front/rear tire centres to split the chassis longitudinally;
3. excludes wheel, tire and brake assemblies from body clearance;
4. reports the lowest front and rear chassis geometry above the road plane.

These minima are clearance diagnostics and guards. The suspension settles to
the authored suspension/body datum, so a low bumper cannot masquerade as the
manufacturer's official H1/H2 reference point.

## Peugeot 206 RC baseline

Peugeot's workshop procedure measures front H1 and rear H2 between defined body
flanges/jacking datums and the ground. It requires correct tire pressures and
notes that alignment, especially front tracking, changes with ride height.
The exact RC H1/H2 values and factory force curves were not found in public
Peugeot material, so the game does not fabricate them.

The current Peugeot GLB is already authored at its intended kerb-mass stance.
Racing United therefore targets zero front/rear body offset and derives preload
from:

- the official 1125 kg GTi 180/RC kerb mass;
- the current estimated 58.19% front static load distribution;
- each corner's suspension provider and estimated 35 kN/m reference rate;
- the authored wheel mounts and 205/40 R17 unloaded radius;
- the current tire vertical stiffness.

The GLB bounds currently report approximately 153 mm at the lowest front body
geometry and 168 mm at the lowest rear body geometry. These are asset geometry
checks, not claims of Peugeot factory clearance. A secondary 110 mm published
minimum-clearance figure is retained only as a sanity check.

## Evidence and future replacement

Authoritative or high-confidence:

- front MacPherson struts and rear independent trailing arms with transverse
  torsion bars;
- stiffer GTi 180/RC front springs than the ordinary GTi;
- 1125 kg three-door GTi 180 kerb weight;
- Peugeot's H1/H2 measurement procedure.

Estimated and explicitly labelled:

- spring and torsion-bar rates;
- damping curves;
- exact hardpoints and motion ratios;
- static axle distribution and center of mass;
- exact RC H1/H2 values.

Measured corner weights, wheel rates, damper dyno data, tire vertical stiffness
and RC-specific H1/H2 values can replace those fields independently. No solver
rewrite is required.

## Verification

`HeritagePhysicsTests` reconstructs the supported front/rear loads from the
calculated preloads, checks tire deflection and suspension travel, and requires
the chassis to return to the authored datum within numerical tolerance. The
Vehicle Suspension panel's `RIDE HEIGHT` tab shows the same per-corner results
and the loaded GLB's front/rear clearance diagnostics.

Sources:

- Peugeot workshop reference-height procedure:
  <https://www.peugeot206cc.co.uk/repair-206/206/info/gb/b3bf05k3.htm>
- Peugeot UK GTi 180 release (modified front geometry/stiffer springs and two
  rear tie rods): <https://www.peugeotpress.co.uk/releases/843>
- Peugeot UK 206 technical specification (suspension architecture and 1125 kg
  kerb weight):
  <https://pscuk.net/wp-content/uploads/2024/05/3090-206-hatch-spec.pdf>
- Secondary 110 mm clearance cross-check:
  <https://www.largus.fr/fiche-technique/Peugeot/206/I/2003/Berline%2B3%2BPortes/20%2B16v%2B%2BRC%2B3p-719068.html>
