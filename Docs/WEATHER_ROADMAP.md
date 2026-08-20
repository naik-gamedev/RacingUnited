# Heritage Engine Weather, Clouds and Rain Roadmap

## Scope

This roadmap records the intended weather architecture before implementation so
future work does not regress into camera-attached rain or asset-heavy cloud
workflows. The existing `SurfaceHydrology` system remains the authoritative
road-water/wetness consumer. Clouds and rain presentation are new engine systems
that feed and visualize one shared world weather state.

Weather implementation follows the multicore/performance rules in
`PERFORMANCE_MULTICORE_ROADMAP.md`; the JobSystem is established first.

## Authoring goal

A scene creator should not have to model ordinary clouds in Blender. The scene
provides a real-world/reference altitude (or an explicit project datum) and the
weather system can define cloud base/top in metres, coverage, density, wind,
precipitation and storm structure from presets plus optional overrides.

Example conceptual inputs:

- scene/reference altitude;
- cloud type/preset and coverage;
- cloud base and top altitude;
- wind direction/speed and vertical evolution;
- precipitation rate in physical units such as mm/hour;
- temperature/humidity/pressure fields when later simulation justifies them.

Special authored cloud formations may be supported later, but they are not a
prerequisite for normal dynamic weather.

## Volumetric clouds

Heritage should use the best practical real-time techniques available when the
cloud milestone is implemented, selected by quality-per-millisecond rather than
by novelty. The intended high-quality path is a procedural/sparse world-space
density field with accelerated empty-space traversal, low-resolution volumetric
integration and temporal/spatial reconstruction.

Required behavior:

- volumetric density exists in world space, not around the camera;
- clouds integrate with sun/sky/time of day and attenuate direct sunlight;
- self-shadowing and cloud shadows are supported at scalable quality;
- wind and weather evolution move the shared density field coherently;
- cloud shadows may update at a lower cadence than rendering and be
  reconstructed/interpolated;
- high settings may ray march true volumes while lower tiers reduce steps,
  resolution, lighting cost, update cadence or use distant impostor/analytic
  representations;
- LOD transitions obey Heritage's master fade/blend rule and must not pop.

Cloud simulation/density/lighting inputs are shared by every view. A second
split-screen camera does not create a second weather simulation, although each
view needs its own volumetric integration and temporal history because its rays
are different.

## Rain authority versus rain presentation

Physical rainfall is a world weather quantity, not the number of rendered
particles. If a region receives 12 mm/hour, hydrology receives that rate whether
one camera renders 2,000 drops, two cameras each render 20,000 drops, or visual
rain is disabled on a very low setting.

This separation is mandatory for determinism, multiplayer and low-end scaling:

`world weather -> authoritative precipitation -> surface wetness/hydrology`

and independently:

`world weather -> per-view rain presentation`

## World-anchored rain — no camera rain bubble

Heritage must never make the storm translate with the player/camera. Nearby rain
may use a recycled GPU draw/particle budget centred on each view for efficiency,
but drop existence, initial position and trajectory are derived deterministically
from world-space precipitation cells, a weather seed and time.

Moving the camera therefore changes which world-space rain is visible; it does
not drag the rainfall field through the world. At high vehicle speeds activation
bands overlap and fade so new precipitation cells stream in without a visible
ring or spawn wall.

## Rain distance representation

Rain uses a continuous per-view LOD hierarchy rather than individual particles
to the horizon:

1. **Near field:** dense GPU drops/streaks with wind-relative trajectories;
   selected ground/body impacts can spawn visible splashes and ripples.
2. **Middle field:** fewer/simpler streak representatives with little or no
   per-drop collision work.
3. **Far field:** world-space volumetric precipitation density integrated along
   the view ray, producing the real distant rain curtain and visibility loss
   without millions of particles.
4. **Atmospheric distance:** precipitation blends into fog/air-lighting and
   cloud/sky conditions instead of exposing a hard rain LOD boundary.

The exact distances are quality- and scene-dependent. They must not be fixed in
architecture documents as magic numbers.

## Cover, bridges, roofs and tunnels

Rain clipping through cover is unacceptable. World geometry must contribute to
a precipitation-exposure/occlusion representation so direct rain is suppressed
beneath bridges, tunnels, garages and roofs. The solution should avoid a CPU
raycast per visible drop; likely implementations include a world-space
rain-occlusion field/map combined with scene depth for view-dependent volumetric
integration.

Coverage should support more than binary roofs:

- open sky: full exposure;
- solid roof/tunnel: zero direct exposure;
- vegetation/canopies: partial/transmitted exposure where the vegetation system
  provides suitable data;
- roof/bridge edges: later runoff/dripping can be emitted from accumulated water
  rather than allowing rain to pass through the roof.

A camera under a bridge must be able to see heavy rain outside the opening while
the air immediately around the camera remains dry.

## Multiple cameras, split screen, mirrors and replays

There is one world weather state and one authoritative precipitation field. Each
view receives its own presentation work:

- split-screen players may be kilometres apart and each gets nearby rain drawn
  around its own location;
- nearby views sample the same deterministic world rain field so precipitation
  remains spatially coherent;
- cloud density, sun/cloud lighting, weather maps and cloud shadow data are
  shared;
- per-view volumetric histories and rain draw lists are separate;
- mirrors/rear views use reduced quality/budgets where appropriate rather than
  re-running full weather simulation.

World-space splash events (vehicle puddle hits, major spray, runoff) are emitted
once and rendered by any view that can see them.

## Interaction with existing water and tires

Weather feeds the existing reduced-order hydrology system at its own physical
cadence. Individual visual raindrops do not become water particles. Hydrology
continues to own water depth, downhill transfer, drainage, infiltration,
evaporation and tire displacement.

WEATHER06A establishes the first integrated storm presentation on the stable JOB03
hydrology baseline. The existing instanced water pass interprets authoritative
depth as deterministic dark circular wetting spots that expand and merge into a
continuous wet film, then transitions deeper water toward stronger reflection and
world-space rain ripple rings. The effect is deliberately cheap: no CPU raindrop
objects and no 3x3 per-fragment rain search are used. Visual impacts do not add
water mass.

Visible rain is now a separate `WeatherPresentationRenderer`. A bounded instanced
near/mid representative grid is selected from world-space precipitation cells and
weather time, so moving the camera changes which cells are visible instead of
dragging a camera rain bubble through the world. A far rain curtain is integrated
with cloud/sky presentation.

`SkyRenderer` now includes a one-third-resolution procedural volumetric cloud pass
with deterministic world-space density, humidity-dependent base altitude, storm
thickening and wind advection. The pass is reconstructed over the normal sky and
shares the same weather inputs as hydrology/rain. It is a presentation density
field, not a full meteorological pressure/condensation simulation.

Later integration may include:

- rainfall intensity -> hydrology source term;
- cloud cover/solar attenuation -> road and tire thermal state;
- wind -> rain trajectory, spray, vegetation and loose-rubber migration;
- standing-water depth -> tire wet grip/hydroplaning and spray;
- tire/vehicle splash impulses -> local water disturbance plus GPU presentation;
- snow/hail/slush as separate precipitation/phase-change models rather than rain
  shader variants.

## Performance policy

- Keep weather authority low-frequency where physics allows it.
- Reconstruct smooth cloud/rain presentation at render rate.
- Render expensive precipitation only inside relevant view/visibility regions.
- Use GPU generation/indirect drawing where it beats CPU particle bookkeeping.
- Share world data between cameras instead of duplicating weather simulation.
- Let settled water and inactive weather regions sleep or update slowly.
- Scale cloud ray steps/resolution, rain representatives, reflection quality and
  secondary splashes before changing authoritative rainfall or local tire
  physics.

## WEATHER06A implementation status

Implemented together because the first useful visual acceptance test needs the
whole storm to read coherently rather than as isolated features:

- cloud cover and precipitation attenuate direct sunlight and darken/desaturate
  environment lighting;
- the procedural environment cubemap now notices weather-lighting changes, not
  only time-of-day changes, so wet reflections follow a storm;
- volumetric cloud integration runs at one-third view resolution with ten bounded
  ray-march steps and bilinear reconstruction;
- cloud density is world-anchored and advected by the current weather wind speed;
- near/mid rain uses one 32x32x16 instanced representative lattice per rendered
  view with no per-frame particle upload;
- distant precipitation becomes a soft atmospheric rain curtain;
- rainy distance haze is applied to scene geometry as a cheap far-field
  precipitation representation;
- hydrology remains the sole water-mass authority and retains JOB03 distance-
  adaptive cadence; and
- the working INPUT01B paired keyboard pedal arbitration remains unchanged.

Still deliberately deferred: precipitation-exposure maps for roofs/tunnels,
spatial cloud-shadow maps, true meteorological cloud formation/condensation,
lightning, bodywork droplets, wiper clearing and vehicle spray/runoff. Those should
be layered on this architecture rather than folded into the hydrology solver.

## WEATHER06D world anchoring and cover recovery

Live WEATHER06C testing proved the near GPU streaks could be made readable, but
the added fullscreen streak veil visibly moved with the vehicle and rain continued
inside covered bridge/tunnel space. WEATHER06D removes that screen-space rain tier
entirely. The recyclable near lattice remains centred around each view for bounded
GPU cost, while every drop's identity, jitter and trajectory remain derived from
absolute world precipitation cells.

Direct rain now consumes a cached 64x64 precipitation-cover height map built from
the already-baked layered hydrology surfaces. The 128 m map refreshes only after
meaningful camera movement/topology change and stores elevations relative to an
FP64 cache centre. Together with restored reversed-Z scene-depth rejection, this
suppresses streaks beneath bridges/roofs/tunnels without one CPU raycast per drop.
The hydrology topology also derives sky exposure per layered cell, so lower roads
beneath bridge/tunnel cover no longer receive authoritative rainfall through the
solid surface above them.
Middle/far rain remains atmospheric haze until it is replaced by genuinely
world-space volumetric precipitation, not another fullscreen moving texture.

## Proposed milestone order

1. **WEATHER04:** world weather service and altitude/datum authoring contract.
2. **WEATHER05:** procedural volumetric cloud prototype integrated with sun/sky,
   cloud shadows and scalable reconstruction.
3. **WEATHER06:** deterministic world-space rain field plus near/mid/far rain
   presentation.
4. **WEATHER07:** precipitation occlusion/exposure for bridges, roofs and tunnels.
5. **WEATHER08:** multi-view/split-screen budgets, mirrors and replay validation.
6. **WEATHER09:** richer world-space splash/runoff/drip presentation and final
   low-end/large-scene optimization.

Milestone numbering is a roadmap label only; implementation may reorder a step
when profiling or another engine dependency requires it.


## WEATHER06E live rain visibility rule

The explicit instanced world-cell rain lattice remains the normal streak source.
The camera-space fullscreen streak veil stays removed. On the current MSAA path,
fixed-function depth testing of this late transparent pass can reject the complete
rain layer, so WEATHER06E keeps the proven-visible streak draw depth-test disabled
and uses the hydrology-derived cover-height field to reject precipitation beneath
bridges/roofs/tunnels. When the camera is beneath a cover surface, streaks above
that surface are rejected as well so they cannot draw through the deck. A later
resolved scene-depth texture may add general opaque-geometry rain occlusion without
changing the world-anchored lattice.

## WEATHER06F world-space visibility recovery

Live WEATHER06D/E testing showed that removing the old camera-space rain veil
also removed all visible precipitation. WEATHER06F keeps the world-cell near
lattice but adds a guaranteed-visible mid/far OpenGL tier whose fullscreen pass
reconstructs camera rays and evaluates rain at world-space sample positions.
The fullscreen primitive is therefore not itself the rain coordinate system.

Per-drop cover-map rejection is no longer allowed to suppress the entire rain
field on layered/steep LiDAR terrain. Until resolved scene depth is available,
a small layered-hydrology query only classifies whether the camera is directly
under a bridge/roof/tunnel and suppresses local presentation there. Authoritative
hydrology precipitation exposure beneath cover remains unchanged.


## WEATHER06G live rain visibility recovery

Rain shelter classification is constrained to an almost vertical camera column so steep exposed terrain cannot be mistaken for a roof. The world-anchored GPU rain lattice remains the primary near-field representation and is deliberately legible at 4K/MSAA.

## WEATHER07A — physical precipitation foundation

WEATHER07A supersedes renderer-authored rain kinematics with an engine-level
physical/statistical precipitation authority under `Physics/Weather`.

- `RainMicrophysics` derives a Marshall-Palmer baseline drop-size population
  from authoritative rainfall in mm/h, then mass-normalizes the numerical
  population so the integrated liquid flux exactly matches hydrology rainfall.
- ordinary raindrop terminal velocity is size-dependent and bounded around the
  real ~10 m/s large-drop regime rather than the WEATHER06 visibility hack that
  accelerated drops to 22–38 m/s;
- `PrecipitationField` reconstructs deterministic world-cell representatives
  from cell identity, seed, weather time, drop population and explicit wind;
- `SurfaceWorld` owns one field for every camera/view;
- wind now has an explicit world heading, removing renderer-owned direction;
- current WEATHER06 geometry remains only a temporary debug/acceptance consumer
  and now takes physical trajectory inputs. Visual streak length is exposure,
  not fake physical speed.

This is intentionally the last milestone in which the old white-line rain is
considered acceptable as a debug visual. WEATHER07B replaces its optical
presentation with the dedicated textured/GPU rain path while preserving the
07A physical authority.

## WEATHER07B — textured optical rain

WEATHER07B replaces the normal near-field white-line rain with a material-driven
optical raindrop renderer while keeping WEATHER07A as the physical authority.
The standard module authoring convention is:

- `Assets/Weather/Rain/RainDrop_BC.png` — coverage/opacity; RGB remains neutral;
- `Assets/Weather/Rain/RainDrop_N.png` — tangent-space optical normal;
- `Assets/Weather/Rain/RainDrop_TN.png` — linear water-thickness/lensing weight.

The renderer constructs one velocity-aligned quad for each bounded statistical
representative. The supplied image is never interpreted as the physical size or
speed of a drop. Diameter is sampled from WEATHER07A's rain population, terminal
velocity uses the same physical size law, wind coupling remains size-dependent,
and apparent streak length is derived from physical velocity times an optical
exposure interval.

The three maps are loaded as linear data and kept in their original scanline
orientation; UV V is reversed in the vertex shader so the rounded lower bulb is
the falling head without silently inverting the tangent-space normal's green
channel. The thickness texture weights water optics independently from opacity.
The current pass uses the environment cubemap for Fresnel/curvature reflection;
WEATHER07C will add resolved scene-colour/depth so true refraction, surface
impacts and wet-material response do not depend on framebuffer-copy hacks.

The old CPU white-triangle compatibility rain is drawn only when the optical
triplet is unavailable. Mid/far world-space rain is retained as a deliberately
subtle atmospheric-density tier rather than competing with the textured drops.

## WEATHER07B5 scientific rain scale

The WEATHER07B4 two-metre texture proof was diagnostic only and is removed.
Normal rain presentation now preserves WEATHER07A's 0.20–6.00 mm physical drop
diameters. Visible streak length is terminal velocity integrated over a bounded
camera-exposure interval, while sub-pixel drops use area-compensated raster
support rather than physically enlarged water geometry. See ADR-093.

## WEATHER07B7 — OpenGL 4.6 GPU-compute rain density

The WEATHER07B6 1000x CPU-density experiment established the desired visual
population but reduced runtime to approximately 1 FPS. WEATHER07B7 therefore
moves high-density representative generation to OpenGL 4.6 compute shaders and
an SSBO. Storms can submit up to 4,194,304 textured representatives without a
per-drop CPU loop. A dense near tier preserves the successful B6 look, a lower
weight mid tier extends individual streaks to roughly 80-100 m, and a world-space
volumetric precipitation field now samples rain curtains out to approximately
1 km. Physical drop size, terminal velocity, wind and optical exposure remain
WEATHER07A/B5 authorities; GPU density changes presentation sampling only.

## WEATHER07C1 - GPU-compacted precipitation LOD
- Dense textured rain is concentrated inside ~10 m of each view.
- 10-35 m uses a sharply reduced textured representative density.
- 35-90 m uses sparse textured representatives; kilometre-scale volumetric rain carries distance visibility.
- Compute culls radial-tier and off-frustum candidates, atomically compacts survivors, and drives `glDrawArraysIndirect` without CPU readback.
- Airborne rain is a single UV-unwrapped triangle using a tiny base-colour/alpha map plus environment reflection; normal/thickness are reserved for effects where they are visually justified (windshield/body/puddles).
