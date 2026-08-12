# ADR-045 - Wet hard-surface water film, tread drainage and progressive hydroplaning

## Status

Accepted - TIRE12 candidate.

## Context

Heritage already has MF6.2 hard-surface forces, SWIFT-like structural/footprint state, dynamic tire
pressure, TIRE08 spatial tread depth and TIRE11 material transfer. The historical surface system,
however, represented wet pavement mainly through scalar friction/stiffness multipliers. That cannot
represent tread drainage, partial standing water, pressure/tread-depth sensitivity, progressive
loss of pavement support or water-plowing drag.

Current commercial MF-Tyre/MF-Swift releases publicly advertise variable water-layer behavior, but
the proprietary equations and fitted data are not public. Heritage therefore needs an independent,
explicitly clean-room wet-surface layer rather than inventing undocumented commercial parity.

## Decision

Add compiled `Vehicles/Tires/TireWetSurfaceInteraction.*` and compose it around the existing
hard-surface tire stack. The provider accepts physical water-film depth plus adaptive-footprint
wetness, speed/slip, normal load, dynamic inflation pressure, footprint dimensions/area and current
spatial tread depth. Existing normalized `surfaceWetness` remains a compatibility bridge whose
water-depth scale is tire-authored through `[HERITAGE_WET_SURFACE]`.

Drainage demand compares incoming water transport with a bounded tread-groove capacity driven by
remaining tread depth, void ratio, drainage efficiency and inflation-pressure ratio. Thin-film
lubrication applies before severe drainage failure. As drainage demand grows, a bounded water-wedge
state produces hydrodynamic lift; the lift fraction continuously reduces pavement-supported contact
instead of switching at one critical speed. Water-plowing drag is a separate fluid force outside the
pavement friction circle. Wet state also scales tire relaxation, rolling resistance and tread-road
thermal coupling.

A classical pressure-based hydroplaning-speed estimate is retained only as telemetry and a sanity
check. It is not the force law or an on/off threshold.

Extend each existing 16x3 `TireTreadCellState` with retained-water film. Only currently contacting
material-fixed cells pick up local water, and retained water progressively sheds on dry contact or
with time/speed. Forty-eight cells remain state/history; the normal fidelity tier still performs one
MF6.2 force/moment evaluation and one wet interaction evaluation per tire.

For hard surfaces, `VehicleSystem` reconstructs the dry base surface blend before applying this wet
provider so the older scalar wet friction path is not double-counted. Non-hard surface materials
retain their previous behavior until TIRE13-TIRE15 replace them with dedicated snow/ice/granular/
terramechanics providers.

Racing United's `[HERITAGE_WET_SURFACE]` coefficients are synthetic development data. They are not
measured historical Pirelli values and are not proprietary Simcenter Tire coefficients.

## Consequences

- Damp pavement, standing water and severe hydroplaning become a continuum rather than surface IDs.
- Worn tread and low pressure can become more hydroplaning-prone through explicit drainage/load
  mechanisms.
- The adaptive footprint can represent split wet/dry support without multiplying MF evaluations.
- Retained water can persist spatially around the rotating tread after leaving a puddle.
- Future weather/track-water systems can feed real water-film depth directly without rewriting the
  tire solver.
- TIRE13 can reuse the same water-film state for ice/compacted-snow melt-water effects.
