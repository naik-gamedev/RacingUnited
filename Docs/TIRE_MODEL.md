# Heritage Tire Model

> **Status note (2026-08-14):** this document retains mechanism detail and milestone history.
> `CURRENT_TIRE_STATUS.md` is authoritative for implemented/partial/missing status and the active
> TIRE18 sequence.

## TIRE12 status

TIRE12 adds `TireWetSurfaceInteraction.*` as a compiled clean-room hard-surface water layer around
the existing MF6.2/SWIFT tire stack. It does not reproduce or claim Siemens/Simcenter Tire 2512
proprietary wet-road equations. The provider works with physical water-film depth while the current
scene `surfaceWetness` field is retained as a compatibility input mapped through tire-authored
`WETNESS_ONE_WATER_DEPTH_M`. Racing United's synthetic prototype road tires currently map a fully
wet normalized surface to a 3 mm film for development/testing.

Drainage demand compares water entering the footprint with a bounded groove-flow capacity derived
from remaining TIRE08 tread depth, authored tread void ratio/drainage efficiency, dynamic inflation
pressure and reference drainage velocity. Thin-film lubrication reduces hard-surface friction before
full hydroplaning. As water demand outruns drainage, a water wedge produces hydrodynamic lift and
progressively reduces pavement-supported normal contact; water-plowing drag is added as a fluid
force rather than being hidden inside the pavement friction circle. Wet state also modifies
relaxation length, rolling resistance and tread-to-road thermal coupling. A classical
pressure-derived hydroplaning-speed estimate remains telemetry only; force generation is continuous.

The 16x3 TIRE08 tread field now also stores retained water film per material-fixed cell. Contacting
cells pick up water and shed it progressively on drier pavement/with rotation and speed. This is
cheap state/history, not 48 hydrodynamic or MF solves. Hard-surface footprint blends restore dry
base coefficients before TIRE12 is applied, preventing the historical scalar wet multiplier from
being counted twice. See ADR-045.

## TIRE11 status

TIRE11 promotes `TireSurfaceInteraction.*` from an empty architecture destination into the first
compiled tire/surface material-transfer mechanism. The existing TIRE08 16-sector x 3-band tread
field now retains five independent normalized local channels: organic/grass contamination, mineral
dirt/dust, gravel fines, rubber pickup/marbles and mud-film groundwork. Pickup and cleaning use the
same exported tread-contact weighting as wear, so the material history is fixed to the rotating tire
and remains spatial rather than collapsing into a global dirty-tire scalar.

Grass, dirt and gravel provide distinct pickup compositions; TIRE06 adaptive 2D footprint material fractions are blended into TIRE11 so edge contact can transfer material before the centre ray crosses the boundary; wetness can increase mud-film pickup.
Clean hard surfaces progressively remove deposited material while speed, contact slip and hot tread
increase scrubbing/release. A separate dynamic `surfaceRubberDebrisFraction` hook permits future
rubber marbles or a dynamic racing line without creating a fake collision surface type. Snow/ice are
left untouched for TIRE13.

The blended active contamination state feeds the existing single MF6.2 evaluation through bounded
contact-friction, rolling-resistance and tread-to-road heat-transfer scales. Forty-eight cells remain
cheap state/history, not forty-eight force solvers. Racing United authors the current development
coefficients in `[HERITAGE_CONTAMINATION]`; they are synthetic tuning data rather than measured
historical Pirelli data. See ADR-044.

## TIRE10 / VIS02 status

TIRE10 closes the gap between TIRE08 wear history and the physical support radius. The 16x3
spatial tread field now reports average tread-radius loss, the interpolated radius loss under
the current contact sector/band blend, and the signed local radius variation relative to the
mean. `VehicleSystem` subtracts the current contact tread loss from the nominal support radius
before converting the center road ray into the hub/suspension datum. The same worn radius is
used by TIRE04 contact/effective-radius evaluation. A material-fixed braking flat spot therefore
creates a periodic geometric radius disturbance as the wheel rotates and naturally excites the
existing radial tire, unsprung mass and suspension instead of injecting a canned vibration
force. Uniform tread wear also slowly reduces the physical radius.

VIS02 extends the TIRE09 GPU presentation bridge with the authoritative native road-contact
normal and wheel-center-to-contact-plane distance. The main and shadow tire shaders transform
the world contact normal into the spinning tire-node frame and flatten the tread against that
plane. World gravity is now only the airborne/fallback direction. This makes visual deformation
follow banking/crossfall/irregular contact rather than assuming every road is horizontal. The
Peugeot GLB remains untouched. See ADR-043.

The project is now deliberately staying on tire + driven-surface work until that stack is
substantially complete. `TIRE_SURFACE_ROADMAP.md` owns the sequence from contamination through
wet pavement/hydroplaning, ice/compacted snow, shallow gravel/dirt, deformable terramechanics,
specialty tire families and the final calibration/scalability gate.

## TIRE09 / VIS01 status

TIRE09/VIS01 adds physics-driven visual deformation to the actual rendered tire mesh without
turning mesh vertices into physics particles. `Entity.SetMeshNodeTireFlexibleRingFromWheel`
carries a
compact presentation state from native wheel telemetry to a named GLB node. `Mesh.cpp`
automatically recognizes tire/tyre node names and infers centre, axle axis, section half-width,
inner/bead radius and outer radius from indexed authored geometry. The source asset is not
modified and no tire-specific bones or vertex colours are required for this first path.

The main and shadow vertex shaders deform only tire nodes. Physical radial deflection produces
a finite road-facing tread flattening and lower-sidewall bulge; non-radial SWIFT-like ring
translation, yaw and wind-up provide bounded outer-carcass motion; and TIRE08's deepest material-fixed
circumferential wear sector creates a local visual flat-spot dent. The bead region remains
nearly rigid, so separate wheel/rim/brake nodes are not squashed. Physics dimensions are
converted into authored local mesh scale from the tire's physics reference radius.

TIRE41 keeps radial presentation under the flexible-ring contact solve alone. The road-envelope
radial support mode is not added again as a translation of the complete belt relative to the rim;
doing so double-counts a curb and makes the unloaded crown balloon while the footprint collapses.
Dynamic pressure supplies reduced-order hoop tension, authored vertical construction stiffness
modulates the permitted compliance, and a pressure-dependent radial envelope retains bead/rim
clearance. Over-pressure permits only bounded construction strain at the crown rather than a
load-driven increase of the complete unloaded radius.

TIRE17C7/VIS10 adds an exact-collider presentation constraint for reference-quality
deformation. The collision BVH selects up to 64 real nearby static creator triangles per wheel
at a 500 Hz reference refresh rate. Their world-space vertices and normals are copied through a
native-to-native bridge and the GPU tests every relevant tire mesh vertex against those exact
planes/faces after pneumatic and force-shear deformation. This is intentionally separate from
the 3x3 road-envelope field used by tire-force support: vertical kerb faces and chamfers are
therefore represented as actual collider geometry rather than inferred from heights. The first
implementation is a quality ceiling; triangle count/update rate are expected to be reduced only
after visual correctness is established.

TIRE17C8/VIS11 fixes two correctness defects exposed by the first curb test. Heritage renders
entity meshes with camera-relative `uModel` matrices, but TIRE17C7 uploaded the exact collider
triangle vertices in local-world coordinates. The vertex shader was therefore comparing tire
vertices and collider faces expressed in different coordinate spaces. Main-pass and layered
shadow uploads now subtract the same camera eye used to construct `uModel`, while triangle
normals remain unchanged because translation does not affect direction vectors. The BVH result
ranking also now uses the exact closest point on each triangle to the wheel centre instead of a
vertex/centroid heuristic. This matters for large creator road and kerb triangles whose surface
can pass directly through the wheel query volume while all three vertices and the centroid are
far away. A native regression locks that selection behaviour down.

TIRE17C9/VIS12 changes the exact stage from cumulative plane projection to a bounded elastic
constraint. Live curb testing of C8 showed two distinct failure modes: repeated projections against
several nearby faces could tear individual vertices into spikes, and exact-triangle availability
disabled the smoother support-field curb shaping entirely. VIS12 reconstructs the geometric plane
from each triangle, orients it from the real rendered tire centre, uses metric closest-point edge
tolerance, selects only the dominant contact plus one genuinely different corner face, and converts
penetration into mostly radial-inward carcass motion. The available sidewall span bounds travel and
the bead remains anchored. The 3x3 support field now continues to provide broad, tire-like curb
conformity; exact triangles are the final bounded anti-clipping layer rather than the shape generator.

Racing United currently drives the four embedded Peugeot nodes `WH_FL_Tire`, `WH_FR_Tire`,
`WH_RL_Tire` and `WH_RR_Tire` from live tire telemetry in `VisualWheels.lua`. The current
provisional Peugeot mesh is sufficient for development (approximately 205 mm wide by 595 mm
diameter), but its detailed shape remains non-authoritative; explicit technical tire metadata
and physics dimensions continue to outrank the mesh. TIRE09/VIS01 itself was presentation-only at introduction; TIRE10 now consumes the same tread
state for physical radius variation and VIS02 contact-plane locking. See ADR-042 and ADR-043.

## TIRE08 status

TIRE08 promotes `TireWear.*` from architecture scaffold to compiled spatial tread state. Each
tire owns 16 circumferential sectors x 3 lateral bands (inside/center/outside), for 48 local
cells. The cells retain surface-temperature deviation and remaining tread depth while the
existing MF6.2/SWIFT stack still performs one force/moment solve per tire. TIRE07 continues
to own the mean tread/carcass/gas energy and inflation pressure, so TIRE08's spatial surface
temperatures are an energy-neutral redistribution layer rather than a second bulk thermal
model.

Slip dissipation is deposited into the currently contacting circumferential sector and into
three lateral bands whose load fractions respond to inflation pressure and camber. A stationary
locked/sliding wheel therefore localizes heating and abrasion, while normal wheel rotation
spreads the same work around the circumference. Circumferential/lateral diffusion and
surface-to-bulk relaxation smooth local hot spots. Temperature- and load-dependent abrasion
reduces remaining tread depth per cell; severe local wear produces measurable flat-spot depth.
The current force feedback is deliberately bounded: early wear is subtle, while severe tread
depletion/flat spotting can reduce the single MF contact friction scale. Physical flat-spot
radius/vibration is a follow-up mechanism, not hidden inside this state provider.

Racing United's synthetic `.tir` datasets use `[HERITAGE_TREAD_STATE]` for these development
parameters. They are not measured Pirelli data and do not claim Live for Speed or proprietary
Simcenter implementation parity. Live telemetry reports inside/center/outside surface
temperature, tread depth, minimum depth, wear fraction, flat-spot depth and active/hottest
sector.

## TIRE07 status

TIRE07 adds a compiled clean-room tire thermal/pressure mechanism around the existing MF6.2,
contact-geometry and SWIFT-like structural stack. `TireThermal.*` owns three lumped energy
states: tread, carcass and contained gas. Slip work heats the tread/carcass; radial damping
and rolling-resistance loss feed the carcass; configurable conduction/cooling exchanges energy
with the road, air and internal gas. The state advances in the 1000 Hz tire loop and airborne
wheels continue to cool when road contact is absent.

Dynamic contained-gas pressure is derived with an ideal-gas absolute-pressure relationship and
feeds both the MF operating pressure and TIRE04 footprint/contact geometry. Tread temperature
provides a bounded grip scale around an authored optimum and carcass temperature provides a
bounded stiffness scale. Both scales are normalized to the reference temperature so the
existing fitted/synthetic reference behavior is continuous when TIRE07 is enabled. Heat
generated during the current substep updates the next substep's force calculation, keeping the
feedback causal.

The prototype Racing United tire files contain an explicit `[HERITAGE_THERMAL]` section with
synthetic development values. This section is Heritage-owned and deliberately separate from
proprietary Simcenter Temperature & Velocity parameterization; it is not measured Pirelli tire
data and does not imply numerical parity with a commercial MF-Tyre/MF-Swift release. Live
telemetry exposes tread/carcass/gas temperature, dynamic pressure, grip/stiffness scales,
dissipation and heat-flow state.

## TIRE06 status

TIRE06 upgrades TIRE05's longitudinal road-envelope probes into an adaptive 2D footprint
sampler and activates rigid-ring yaw/wind-up. The road sampler uses the imported
`ELLIPS_NWIDTH/ELLIPS_NLENGTH` values as bounded fidelity hints rather than blindly issuing
that many expensive force evaluations. On the current 3x3 Racing United development setup,
smooth homogeneous contact uses a five-location cross; detected height, support, material or wetness
discontinuities refine to all nine locations. The maximum axis resolution is bounded by the
provider description for predictable large-grid cost.

Each road sample is reduced relative to the smooth local road plane in both longitudinal and
lateral directions. The resulting envelope reports effective longitudinal/cross slope,
roughness height range, support fraction and sample count. Additional road queries default to
125 Hz on quiet contact and 250 Hz when the footprint is complex while the tire/ring state
continues at the normal 1000 Hz vehicle rate.

Footprint samples also inspect road material/wetness. Supported samples are mapped through the
existing surface profile and averaged into friction, stiffness, rolling-resistance and
relaxation multipliers. The tire still performs one MF6.2 force/moment evaluation per wheel per
high-rate step. That is intentional: TIRE06 can resolve a tire spanning different surfaces
without turning every wheel into a dense brush/FEM solver. A later fidelity tier may distribute
local shear/forces across contact cells where the gameplay/vehicle warrants the extra cost.

`TireRigidRing` now owns yaw angle/rate and longitudinal wind-up angle/rate in addition to the
three translational states. Yaw is excited by the prior aligning moment using yaw stiffness and
belt diametral inertia; wind-up is excited by the prior longitudinal reaction torque using belt
polar inertia and the configured wind-up mode. The exact second-order transition used by the
translational modes also advances these rotational modes. Ring yaw modifies structural slip
angle and wind-up angular velocity modifies effective belt circumferential speed on the next
MF evaluation.

This is a clean-room architecture based on public MF-Swift concepts and property vocabulary,
not a claim of proprietary Simcenter Tire parity. Racing United's current structural and
enveloping values remain synthetic development seeds rather than measured historical Pirelli
data.

## TIRE05 status

TIRE05 promotes the public MF-Swift structural architecture into two independent native
mechanisms while keeping MF6.2 itself as the force/moment provider. `TireRigidRing.*` owns
the belt/ring structural state. This branch activates longitudinal, lateral and radial
translation with stiffness, modal-frequency and damping inputs; it uses an exact damped
second-order state transition so the roughly 50–100 Hz structural modes remain stable in
the 1000 Hz tire loop and behave consistently at lower regression rates. Public yaw and
wind-up structural parameters are imported and preserved, but rotational ring DOFs are not
yet active.

`TireRoadEnveloping.*` is Heritage's clean-room tandem-cam-inspired road filter. It uses
the TIRE04 finite footprint to place front/rear support probes and converts short road
features into an effective road height/plane. Vehicle integration subtracts the smooth
local plane inferred from the center contact normal before enveloping, so constant road
grade does not excite the carcass as roughness. The effective height excites the radial
ring mode; ring in-plane velocities feed the next slip calculation.

The ring state is integrated at the normal 1000 Hz vehicle rate. Extra front/rear road
queries are cached at 250 Hz by default, separating structural bandwidth from expensive
collision sampling for large-grid scalability. TIRE05 currently uses one longitudinal cam
row; the imported `ELLIPS_NWIDTH/ELLIPS_NLENGTH` vocabulary is retained for a later full
2D/3D footprint expansion. This is an independent implementation of public architecture
and parameter semantics, not a claim of Siemens/Simcenter proprietary numerical parity.

The prototype Racing United `.tir` datasets carry explicitly synthetic belt mass, structural
stiffness/frequency/damping and enveloping seed values. They are development data, not
measured Pirelli values. Wheel telemetry exposes envelope road offset/slope/sample count and
all active translational ring offsets/velocities.

## TIRE04 status

TIRE04 introduces `Vehicles/Tires/TireContactGeometry.*` as the explicit geometry layer
between the wheel/unsprung state and the MF force equations. It evaluates free rolling
radius, loaded radius, effective rolling radius, vertical deflection and a finite footprint
for every active contact unit. Imported `BREFF`, `DREFF`, `FREFF`, `Q_RE0` and `Q_V1`
activate the public MF6.2 load/velocity-dependent effective-radius relation. The high-rate
wheel path now uses effective rolling radius for circumferential speed/slip kinematics and
the longitudinal reaction-force lever arm while retaining unloaded radius `R0` as the MF
reference radius.

`[CONTACT_PATCH]` `Q_RA1/Q_RA2` now drive the public square-root + linear finite contact-
length relation. `Q_RB1/Q_RB2` are imported and preserved for the subsequent structural
provider, but TIRE04 intentionally does not invent an unverified width exponent/equation.
Instead, Heritage estimates pneumatic footprint area from `Fz / inflation pressure` and
reconstructs a bounded elliptical width from that area and the calculated length. This is
a transparent Heritage engineering approximation, not a claim of proprietary MF-Swift
contact-width parity.

Where the scalar unsprung-mass/radial-tire model is active, its tire deflection is
authoritative. Compatibility wheels without that state infer `Fz/Cz` only for contact
geometry. Suspension ray length, support query geometry and static ride-height semantics
remain untouched in TIRE04; actual road enveloping belongs to TIRE05. Wheel telemetry and
the Racing United Tires panel expose free/loaded/effective radius and footprint dimensions
so the mechanism can be inspected live.

## TIRE03 status

TIRE03 activates the public MF6.2 turn-slip coefficient vocabulary and makes low-speed
steering/parking behavior stateful. `TireContactPatch.*` now owns elastic torsional tread
deformation at standstill and creep speed. Wheel/chassis yaw winds the patch, rolling
distance releases it, and a regularized yaw-rate/forward-speed quantity provides finite
turn slip through zero speed. `QCRP1` supplies the zero-forward-speed parking-moment coefficient and `LMP` scales it;
the rolling MF turn-slip path remains separate.

`[TURNSLIP_COEFFICIENTS]` is now mapped by the `.tir` importer. The active clean-room
provider uses PDXP* for longitudinal-peak reduction, PDYP* for lateral-peak reduction,
PKYP1 for cornering-stiffness reduction, PECP* for camber-stiffness reduction, QDTP1 for
pneumatic-trail reduction, QBRP1 for residual spin-torque reduction, and QDRP1/QCRP2 for
a bounded rolling turn-slip moment. PHYP* is preserved but deliberately not assigned an
unverified formula yet. This is coefficient-compatible public MF6.2 behavior, not a claim
of numerical parity with proprietary MF-Tyre/MF-Swift releases.

The transient path now consumes PTX1..3, PTY1..2 and LSGKP/LSGAL from imported tire data.
Longitudinal and lateral relaxation lengths are derived per wheel from load, nominal load,
unloaded radius and camber when valid coefficients are present. Missing/invalid transient
data falls back to the existing engineering relaxation-length values. Exponential state
integration keeps the result stable across update rates.

`Vehicle.GetWheelState` and Racing United's Tires live panel expose turn slip [1/m],
normalized turn slip, elastic patch twist, parking/rolling turn moments and the active
Fx/Fy/cornering/trail reduction factors. The existing very-low-speed translational lateral
damper remains as a temporary creep stability bridge; torsional parking behavior no longer
depends on that damper.

TIRE04 owns loaded/effective radius and finite contact geometry. TIRE05 now layers clean-room rigid-ring structural dynamics and road enveloping on top.

## TIRE02 status

TIRE02 adds the parameter/data boundary around the TIRE01 MF6.2 solver.
`Vehicles/Tires/MagicFormula/TirePropertyFile.*` parses human-readable `.tir`
property files into an immutable native data object before any vehicle owns the
parameters. The importer currently accepts `FITTYP=62` and the MF6.2 steady-state
subset of `FITTYP=70`. FITTYP70 files may be inspected and use their MF6.2 core. TIRE07
adds an independent Heritage thermal/pressure state, but proprietary Temperature & Velocity
coefficient equations remain explicitly unsupported rather than being guessed. Obfuscated
property data is rejected.

The importer currently consumes declared length/force/angle/mass/time units; model,
dimension and operating-pressure data; tire/belt inertias; vertical stiffness/damping;
MF validity ranges; scaling coefficients; the steady-state longitudinal/lateral/
overturning/rolling/alignment coefficient families used by Heritage; and motorcycle
`MC_CONTOUR_A/B`. TIRE02 originally preserved PTX/PTY relaxation coefficients and LSG
scaling for later use; TIRE03 now consumes that transient data in the high-rate path.
Unknown sections/keys are retained as diagnostics. Imported files do not inherit
Heritage's synthetic compatibility coefficients when an entry is absent; TIRE02
requires the core longitudinal/lateral shape, peak and stiffness terms explicitly.
`TYRESIDE` measurement metadata is preserved for diagnostics, but mounted-side
asymmetry/mirroring is not yet claimed by the current provider.

`TireModelDescription` records whether parameters came from a property file together
with FITTYP, source path, provenance, confidence and mapped/unsupported counts. A
vehicle definition may specify `tireParameterFile`, `tireParameterProvenance` and
`tireParameterConfidence`; module-relative paths are validated before loading. Lua also
provides `Vehicle.LoadWheelTirePropertyFile(vehicle, wheel, path, provenance, confidence)`
for explicit per-wheel tuning. `Vehicle.GetWheelTireParameterInfo` exposes the
imported flag, FITTYP, source, provenance, confidence and mapped/unsupported counts
for diagnostics and authoring UI.

Racing United's `PrototypeRoadFront_MF62.tir` and `PrototypeRoadRear_MF62.tir` are
synthetic compatibility datasets that keep the current prototype behavior while proving
the import route. They are not measured production-tire data. The authoritative long-
term workflow is measured/identified tire data -> property file -> immutable imported
parameters -> MF provider.


## TIRE01 status

TIRE01 replaces the Step 29G generalized road curve as the default native road
provider with a clean-room implementation of the publicly documented
MF-Tyre 6.x steady-state force/moment family. The implementation uses the public
TNO coefficient vocabulary so future identified FITTYP=61/62-style parameter
sets can be mapped without inventing a second Heritage-only coefficient language.

This is **not** a copy of the proprietary Siemens Simcenter Tire 2512 solver.
Simcenter Tire 2512 publicly advertises a newer MF-Tyre/MF-Swift wet-road model,
but its implementation equations/identification data are not public. Heritage
therefore implements the public MF6.x branch and keeps wet-road, thermal and
future structural extensions behind independent mechanisms.

## Runtime providers

- `advanced_road` — compatibility alias; now selects the MF6.2 road core.
- `mf62_road` — explicit MF6.2 road provider ID.
- `motorcycle_profile` — compatibility alias for the motorcycle-capable MF6.2 path.
- `mf62_motorcycle` — MF6.2 forces/moments plus motorcycle contour geometry.
- `legacy_generalized_road` — retained Step 29G fallback/regression provider.

The tire provider is selected per contact/wheel. It is not selected from vehicle
classification, so mixed or unusual vehicles can compose tire types explicitly.

## Implemented MF6.x outputs

`Vehicles/Tires/MagicFormula/MagicFormula62.*` evaluates:

- pure and combined longitudinal force `Fx`;
- pure and combined lateral force `Fy`;
- load-sensitive longitudinal and lateral friction;
- pressure-sensitive terms exposed by the public equation set;
- explicit camber/inclination influence and camber stiffness;
- overturning moment `Mx`;
- rolling-resistance moment `My` with load/pressure/speed dependence;
- pneumatic trail, residual aligning moment and total self-aligning moment `Mz`;
- longitudinal slip stiffness, cornering stiffness and camber stiffness telemetry.

`Fx` and `Fy` are active chassis contact forces. `Mx`, `My` and `Mz` are now
computed and carried as native wheel telemetry for subsequent steering/FFB and
wheel-dynamics integration. TIRE01 deliberately keeps the previous calibrated
speed-proportional rolling-resistance force active for vehicle behavior continuity;
a direct switch to MF `My` as axle torque failed existing parked/suspension stability
regressions and was therefore rejected by the safety gate rather than silently
retuning the vehicle. That integration belongs in a separately calibrated follow-up.

The long-standing Heritage tire controls remain available through a compatibility
seed bridge. They generate a coherent nominal MF coefficient set so existing
vehicle definitions do not require an immediate tire-rig dataset. This bridge is
an engineering approximation only; a fitted tire parameter set is authoritative
when one becomes available.

## Motorcycle contour branch

`Vehicles/Tires/MotorcycleTireProfile.*` implements the documented MF-Swift 6.2
motorcycle contour convention. `MC_CONTOUR_A` and `MC_CONTOUR_B` are treated as
dimensionless lateral/radial ellipse semi-axes divided by tire width. The module
computes the support point, lateral contact offset and wheel-centre-to-road
support distance on a locally flat road at large inclination angles.

The MF force core itself accepts large camber/inclination and produces camber
thrust. The contour calculation is kept separate because road enveloping,
loaded-radius calculation and future rigid-ring dynamics belong to contact/
structural mechanisms rather than the steady-state force law.

The present four-wheel `raycast_wheel_v1` vehicle topology still does not claim
to be a complete motorcycle solver: lean dynamics, fork/swingarm kinematics and
motorcycle body balance remain separate future providers. The tire branch is
already motorcycle-capable and can be reused by that solver.

## Transient slip

`Vehicles/Tires/TireSlipDynamics.*` is now a compiled mechanism instead of an
empty scaffold. It owns the first-order transport/relaxation state previously
embedded in `VehicleSystem.cpp`. The exponential integration is timestep-stable
and remains evaluated in the high-rate vehicle loop (normally 1000 Hz).

This is the handling-model transient branch. MF-Swift rigid-ring belt/carcass
modes and road enveloping are intentionally not represented by pretending that a
larger relaxation length is equivalent physics.

## TIRE13 compacted-snow / hard-ice winter surface

`Vehicles/Tires/TireWinterSurfaceInteraction.*` is a clean-room hard-winter surface provider that
keeps MF6.2 as the pneumatic tire force/moment core. The adaptive footprint supplies separate snow
and ice fractions. Hard ice responds to local surface temperature, contact slip speed, wetness and
a bounded slip/flash-heating melt-film state. Winter compound and siping are explicit tire-authoring
inputs; optional studs use count and protrusion to contribute separate mechanical ice traction.

Compacted snow adds tread interlock and packed-snow state without claiming deep-snow terrain physics.
Every existing 16x3 tread cell can retain packed snow, so the state rotates with the tread and
progressively self-cleans. Current static scenes provide -5 C through a compatibility bridge until
the planned `SurfaceField` supplies local dynamic surface temperature. Synthetic Peugeot winter
parameters are deliberately low-capability development data and retain that provenance.

## TIRE14 shallow granular gravel / hard dirt

`Vehicles/Tires/TireShallowGranularInteraction.*` handles a shallow loose granular layer over a
load-bearing base. It deliberately keeps one MF6.2/SWIFT evaluation per tire and adds a separate terrain
reaction around that base response. Current mechanisms include bounded pressure-dependent sinkage,
Mohr-Coulomb-style shear capacity, Janosi/Hanamoto-style longitudinal/lateral shear mobilization,
slip-angle-dependent passive-wedge bulldozing, and longitudinal plowing/compaction resistance.

TIRE14 also consumes the adaptive footprint's gravel/dirt fractions and the 48-cell wear state. Tire
`.tir` metadata owns tread aggressiveness, biting-edge density, open void, granular coupling and worn-tread
effectiveness. The current gravel/dirt soil constants are synthetic provider-side compatibility presets,
not tire metadata and not measured track data; they move to `SurfaceMaterial` / `SurfaceField`. Calculated
sinkage changes the actual wheel support/contact datum so a tire can physically settle into the loose
layer. Persistent ruts, compaction memory and fully deformable terrain remain TIRE15.


## TIRE15 persistent deformable-terrain terramechanics

`Vehicles/Tires/TireDeformableTerrainInteraction.*` handles mud, sand, soft soil and deep snow where
the ground itself yields and therefore becomes the primary traction mechanism. The provider reuses
the pneumatic tire's MF6.2/SWIFT geometry, pressure, structural, thermal and wear state but reduces
the direct hard-interface MF contribution. It computes reduced-order pressure-sinkage, soil shear
capacity/mobilization, lateral bulldozing, longitudinal plowing/compaction and tread/flotation
coupling. `HERITAGE_DEFORMABLE_TERRAIN` in `.tir` files contains only tire-side tread/flotation
traits; ground properties stay provider/world-side until SurfaceMaterial authoring supplies them.

`Physics/Surfaces/SurfaceWorld.*` owns the shared persistent driven-surface service and `Physics/Surfaces/SurfaceField.*` implements its deformable-terrain layer. It is a bounded chunked sparse
world X/Z grid with a coarse global-Y layer for stacked roads, keyed by surface material, currently storing loose depth, compaction, moisture, rut
depth, longitudinal/lateral shear history, displaced volume and approximate pass count. Field state
is owned by PhysicsWorld and shared across every vehicle consuming that world. Rut depth contributes to the wheel support datum,
so a later wheel can physically enter a previously formed rut. The current field is intentionally
reduced-order; it does not tessellate/deform the render mesh, simulate individual grains, or claim
DEM/CRM fidelity.

## Current limits / next tire + surface work

After TIRE14, Heritage still does not include:

- complete validated PHYP* turn-slip lateral-shift equation parity;
- distributed per-cell contact-patch shear/force integration beyond the current single-MF adaptive footprint;
- evidence-identified full 3D obstacle/contact parameterization beyond the current bounded clean-room sampler;
- proprietary MF-Tyre Temperature & Velocity coefficient-equation parity;
- deeper puncture/material variation, carcass fatigue, bead/rim damage and detached-tread mechanics
  beyond the current reduced-order pressure-loss/blowout/failure stages;
- spatial rain/puddle flow and drying-line fields beyond the deterministic global rainfall,
  road-film, drainage, evaporation, wind and road-temperature baseline;
- proprietary Simcenter Tire 2512 wet-road implementation/parity (intentionally not a Heritage goal without public equations/data);
- dynamic scene `SurfaceField` temperature/state authoring beyond the current explicit static-scene winter-temperature bridge;
- final specialty low-pressure/off-road tire families and large-grid fidelity tiers.

The authoritative sequence is now `TIRE_SURFACE_ROADMAP.md`: TIRE15 deformable terramechanics plus TIRE15C5A1/TIRE16L track-rubber presentation and TIRE16 tire marks are established baselines; TIRE17A established specialty family baselines, TIRE17B maps creator surface/endurance biases into physical pre-solver parameters, and TIRE17C resolves reusable tire parts into per-wheel runtime fitments with vehicle-owned cold pressure; TIRE17C1/C2 add pressure-aware curb conformity and full three-axis visual carcass/belt deformation. Remaining TIRE17 topology-aware assignment/tooling is followed by TIRE18 calibration/performance.
Hard-surface MF6.2 and deformable-terrain mechanics remain separate composable layers rather than
being folded into one monolithic magic coefficient function. Distributed contact-cell shear/force
integration remains an optional future fidelity tier.

## Regression contract

The native physics suite now additionally verifies:

- MF6.2 road forces/moments are finite with correct restoring-force signs;
- combined slip reduces the corresponding pure-slip demand;
- rolling resistance opposes forward motion;
- a motorcycle tire remains finite at 40 degrees inclination and at a 60-degree high-lean probe;
- mirrored motorcycle inclination mirrors the contour offset and camber force;
- the first-order tire relaxation state agrees across 1000 Hz and 120 Hz
  integration over the same elapsed time;
- MF6.2 turn slip reduces the configured force/stiffness/trail terms and produces finite Mz;
- standstill contact-patch torsion agrees across 1000 Hz and 120 Hz integration;
- imported PTX/PTY data produces load-dependent relaxation lengths;
- MF6.2 loaded/effective rolling radii respond correctly to load, wheel speed and authoritative radial deflection;
- finite contact-patch length/width/area remain bounded and grow plausibly with load;
- adaptive 2D footprint sampling expands from the bounded coarse cross to the requested full grid;
- laterally asymmetric support produces finite cross-slope/support/roughness diagnostics without confusing a smooth local plane for an obstacle;
- rigid-ring yaw and wind-up structural modes agree across 1000 Hz and lower-rate integration;
- the four-node tire thermal state (tread, carcass, contained gas and wheel/rim) heats under slip,
  carcass and brake losses, conducts rim heat into the tire, increases contained-gas pressure and
  remains close across 1000 Hz and 120 Hz integration;
- TIRE08/TIRE10 spatial wear produces a positive material-fixed contact-radius deficit under a locked-wheel flat spot, while normal rotation distributes wear and strongly reduces the local radius variation;
- warm tread/carcass state changes grip/stiffness in the intended bounded direction while an overheated state loses grip;
- human-readable `.tir` import converts units, maps MF6.2 ranges/scaling/coefficients,
  recognizes motorcycle contour data and transfers provenance without silently
  accepting unsupported future mechanisms;
- clean-room rigid-ring translational modes agree across regression rates and respond to
  longitudinal/lateral force plus enveloped radial road input;
- tandem-cam-inspired road enveloping filters short obstacles while a flat local road
  remains neutral;
- TIRE11 spatial contamination picks up wet grass/material transfer in the active 48-cell footprint and progressively self-cleans on hard pavement;
- TIRE12 distinguishes thin wet film from flooded contact, increases drainage demand with tread wear, reflects inflation-pressure sensitivity, builds progressive hydrodynamic lift/contact loss and retains/releases spatial water state.
- TIRE13 distinguishes cold/dry from near-melt hard ice, builds bounded interface melt film with slip/temperature, improves ice response with winter compound/siping/studs, models compacted-snow tread interlock and retains/releases packed snow in the 16x3 material-fixed tread cells.
- TIRE14 shallow gravel/hard dirt produces bounded sinkage, slip-dependent granular shear mobilization, tread-dependent available traction, lateral bulldozing, plowing/compaction loss and partial-footprint mixing without replacing the one-MF tire path.
- TIRE15 persistent deformable terrain produces bounded mud/sand/soft-soil/deep-snow sinkage, terrain-primary shear/bulldozing/plowing forces, shared sparse rut/compaction/moisture/shear history, location-local multipass behavior and timestep-stable field evolution.


## TIRE15C dynamic track rubber

`Physics/Surfaces/Rubber/TrackRubberState` owns shared deposited racing-line rubber and loose marble concentration at stable global coordinates. The tire samples this world state before its force solve; loose rubber feeds the existing TIRE11 pickup channel and both bonded/loose rubber contribute only bounded modifiers around the existing tire/surface model. Contact usage then deposits/migrates new rubber after tire-state update. Rain ageing is lazy/global-exposure based so cost does not scale with every track cell each frame. Presentation darkens rubbered pavement and procedurally renders deterministic ribbon/clump marbles without requiring authored geometry or thousands of rigid bodies.

## TIRE20 unified lower-hemisphere 3D contact and deformation

TIRE20 makes the lower half of the tire a shared physics/presentation contact domain
instead of treating obstacle deformation as a renderer-only correction. A 9 x 7
reduced-order structural topology spans forward equator -> bottom -> rear equator and
the full tire width (sidewall/shoulder/tread/centre/tread/shoulder/sidewall). The
upper half is deliberately outside this expensive contact domain. Collision remains
continuous between the 63 structural locations; they are deformation/traction
coordinates, not 63 blind world raycasts.

The normal road support is explicitly published as the primary carcass contact so
normal pavement deformation and road traction refer to the same physical support
point. Additional lower-half tread/shoulder contacts against kerbs, rocks and other
geometry use local contact frames and MF6.2 where appropriate; sidewall contacts use
carcass compliance and bounded friction. All secondary forces act at their real
contact positions and therefore generate both force and moment. Dynamic obstacles
receive equal-and-opposite impulses.

GPU tire deformation consumes those same physical structural contacts. The retired
exact-triangle renderer projection and visual-only 3x3 curb compression are no
longer active obstacle-deformation authorities. Ordinary flat-road loaded-radius
flattening remains physics-driven. For large grids, flat-road tires evaluate only
the cheap primary support contact; detailed cached geometry is admitted only when a
250/500 Hz neighborhood classifier detects a real 3D lower-half contact opportunity.
