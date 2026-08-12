# ADR-043 - Physical flat-spot radius coupling and authoritative visual contact plane

## Status

Accepted - TIRE10 user-validated.

## Context

TIRE08 stores 16 circumferential x 3 lateral tread cells and identifies localized flat spots.
TIRE09/VIS01 uses that state to dent the rendered tire, but the physical support radius was still
the nominal wheel radius. A flat spot therefore changed friction and presentation without
exciting the radial tire, unsprung mass or suspension.

VIS01 also derived the road-facing deformation direction from world gravity. That is acceptable
on flat ground but cannot guarantee coincidence with the native contact plane on banking,
crossfall or irregular contact.

## Decision

TIRE10 derives three geometric quantities from the existing tread state:

- average tread radius loss;
- current contact tread radius loss, interpolated between material-fixed circumferential sectors
  and weighted across the three lateral bands;
- signed current contact radius variation relative to average wear.

The current contact tread-radius loss is subtracted from the nominal physical support radius
before converting the road ray hit into a hub/suspension datum. The same worn radius becomes the
unloaded-radius input to finite contact/effective-radius calculation for that substep. This makes
localized wear a geometric input to the already-authoritative radial tire and unsprung-mass
mechanics instead of injecting an artificial vibration force.

The ray reach remains based on nominal radius so wear cannot create a false missed-support query.
Road-enveloping relative-height calculations may keep a common nominal radius because it cancels
between center and offset samples.

VIS02 extends the generic tire-deformation presentation bridge with the native world contact
normal and the measured wheel-center-to-contact-plane distance. The shader transforms that normal
into the spinning tire-node frame and clamps the road-facing tread to the physical contact plane.
Gravity remains only an airborne/fallback presentation direction. Rendering remains downstream of
physics.

## Consequences

- Uniform wear slowly reduces actual tire radius.
- A material-fixed flat spot produces a periodic radius disturbance as the wheel rotates.
- The disturbance naturally enters tire deflection/normal load, unsprung mass and suspension; no
  canned `flatSpotVibrationForce` is added.
- Future steering/FFB can consume the resulting real hub/suspension load history.
- Visual footprint orientation works on sloped/banked contact and uses the same physical contact
  plane as the native wheel solver.
- 16 sectors remain state resolution; interpolation between adjacent sectors avoids a 16-sided
  polygonal support radius.
