# Peugeot 206 RC suspension definition

Status: `PEUGEOT_SUSP01` — live, source-labelled production prototype setup.

Static kerb-mass stance, GLB underside inspection and calculated spring/torsion
preload are documented separately in
[RIDE_HEIGHT_CALIBRATION.md](RIDE_HEIGHT_CALIBRATION.md).

## What is authoritative

The 206 GTI 180 / RC uses independent MacPherson struts and coil springs at the
front, independent trailing arms with transverse torsion bars at the rear, and
anti-roll bars on both axles. Peugeot also describes two rear tie rods used to
stabilise the rear axle. These statements are represented by the live
`macpherson_strut_v1` and `trailing_arm_torsion_bar_v1` native providers.

Sources:

- Peugeot UK, *Peugeot 206 GTi 180*: modified front geometry, stiffer springs,
  and two rear tie rods: <https://www.peugeotpress.co.uk/releases/843>
- Peugeot 206 technical specification: front MacPherson, rear independent
  trailing arms/transverse torsion bars, and front/rear anti-roll bars:
  <https://pscuk.net/wp-content/uploads/2024/05/3090-206-hatch-spec.pdf>
- Peugeot 206 workshop alignment characteristics and adjustment policy:
  <https://www.clubpeugeot.es/html/206/info/sp/b3cb0ik7.htm>
- 206 GTI 180/RC-specific alignment range used by alignment equipment:
  <https://www.jltechno.com/en/alignment_specs.php?ModelID=615344&ModelName=206%C2%A0GTI%C2%A0180%2FRC&brand=PEUGEOT>

The stock runtime reference is the midpoint of the published RC range:

| Axle | Camber per wheel | Toe per wheel | Caster |
| --- | ---: | ---: | ---: |
| Front | 0.00 deg | 0.06 deg toe-out | 3.20 deg |
| Rear | -1.00 deg | 0.26 deg toe-in | fixed/not applicable |

## Wheelbase and track audit

The production reference geometry used by the module is:

| Measurement | Factory reference |
| --- | ---: |
| Wheelbase | 2442 mm |
| Front track | 1425 mm |
| Rear track | 1416 mm |

Track is the lateral distance between left and right wheel centres at the hub
plane. It is not the outside tire width and it is not recomputed from the tire's
ground-contact edge under camber. The native `Vehicle.MeasureWheelGeometry`
query projects the four current suspension wheel centres onto averaged upright
axes, so chassis pitch/roll and the mirrored internal camber signs do not
artificially increase the reported value.

The Visual > Wheels audit deliberately shows three independent authorities:

1. the published factory reference above;
2. the loaded GLB's authored tire-centre geometry, reconstructed from accessor
   bounds;
3. the live native hub-centre geometry after suspension and fitment.

This separation exposes an over-wide model or non-stock spacer/offset as a
delta; it never edits the factory reference merely to make a model agree.

Period dimension sources:

- Peugeot 206 / GTi 180 owner's handbook dimension drawing:
  <https://manuals.plus/m/766a1624154f6c6404f288b58a71c4ac0d3a525d10ed67b7e7f4c4c5206bb8a0.pdf>
- Peugeot UK 206 / GTi 180 technical specification:
  <https://pscuk.net/wp-content/uploads/2024/05/3090-206-hatch-spec.pdf>

Front toe is the normal factory alignment adjustment. Front camber/caster and
rear camber/toe are represented as fixed stock geometry, matching the workshop
adjustment policy. The complete min/max envelope remains available to the
Workshop as reference evidence and does not clamp custom setups.

## What remains estimated

Peugeot's public material does not publish the three-dimensional production
hardpoint coordinates or spring, damper, torsion-bar, anti-roll-bar and bushing
curves. The current GLB also has no authored suspension hardpoint nodes.
Consequently, Racing United creates deterministic package estimates constrained
by the published wheelbase, tracks, wheel centres, caster, steering-axis
inclination and suspension architecture. They carry the explicit provenance
`reference_constrained_estimate` and low confidence. This opt-in belongs only to
the Peugeot definition: a generic estimate still cannot become runtime physics
authority.

Each hardpoint may later be replaced independently by asset-authored or measured
data. Those sources have higher priority and automatically supersede estimates.

The current rear provider does not contain a separate compliance model for the
two Peugeot stability tie rods. Their road-car effect is presently represented
by fixed stock rear toe. Adding compliance steer requires measured tie-rod and
bushing geometry; no invented curve is treated as Peugeot data.

## Runtime ownership

- Neutral GLB wheel centres remain immutable reference geometry.
- The fitment layer applies stock alignment with correct left/right native signs.
- Workshop/UI camber and toe convert those mirrored local rotations back to a
  same-sign per-side convention: negative camber means top-in on either side.
- The MacPherson provider owns front wheel path, camber, bump steer, steering
  axis and strut motion ratio.
- The trailing-arm provider owns rear wheel path and torsion-bar/damper motion.
- Tire size or wheel-offset changes never regenerate chassis hardpoints.
- Generic linear raycast suspension remains the fallback if the required
  hardpoint evidence is incomplete.

This is a physically coherent stock 206 RC baseline, not a claim that unpublished
millimetre hardpoints or force curves have been reverse-engineered exactly.
