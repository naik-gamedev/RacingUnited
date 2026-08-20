# Surface Hydrology and Dynamic Dry Lines

## Status

Heritage Engine owns a deterministic, reduced-order surface-water layer in
`Physics/Surfaces/Water/SurfaceHydrology`. It is intended for large LiDAR-derived
road and circuit collision meshes without treating water as particles or a free
3D fluid.

The scene author does **not** have to construct the flow network by hand. When a
static triangle scene is loaded, the engine:

1. rasterizes upward-facing collision triangles into a layered 0.5 m grid;
2. retains surface elevation, normal, material and drainage properties;
3. connects compatible neighbouring cells, including diagonal flow paths;
4. identifies open boundaries and depressions; and
5. caches the topology under `UserData/Hydrology`.

The cache fingerprint includes collision geometry and hydrology material data.
Changing either automatically invalidates and rebuilds it. Only topology is
cached; live water, rainfall and dry-line state are never mistaken for authored
scene data.

## Runtime water

At 30 Hz, every occupied cell receives precipitation and resolves infiltration,
authored drainage, evaporation, downhill head-driven flow, depression storage,
boundary runoff and a bounded maximum depth. The tire force loop samples the
latest spatial depth at its real contact points.

Every grounded tire reports its swept contact footprint, load, speed, tread void,
slip energy and travel direction at the tire substep rate. Water is displaced,
carried and sprayed from the cells actually crossed. Consequently the drying line
is not a painted spline: repeated traffic on any chosen line clears that line,
while an unused wide or shallow line remains wetter. Rain and upstream flow can
wet a cleared line again.

The implementation is deterministic and bounded. It is not an unrestricted CFD
or particle simulation, so it remains suitable for large racing fields.

## Collision metadata

Ordinary dense asphalt defaults to very low infiltration and does not secretly
behave like porous asphalt. A collision node may override the material defaults
with glTF custom properties:

- `heritage.surface.infiltration_mm_per_hour`
- `heritage.surface.drainage_mm_per_hour`
- `heritage.surface.flow_roughness`
- `heritage.surface.depression_storage_mm`

Use authored drainage for known drains or deliberately porous construction.
Geometric fall, banking and depressions come directly from the collision mesh.

## JOB01 multicore execution

The 30 Hz solver uses the shared engine `JobSystem`; it does not own a private
thread pool. Precipitation/losses, depth application and aggregate statistics
are partitioned into contiguous cell ranges with per-range scratch reductions.
The caller merges those reductions in stable range order.

Flow cannot be naively parallelized because one source writes into neighbouring
depth deltas. JOB01 assigns each source cell one of 27 colours from
`(x mod 3, z mod 3, layer mod 3)`. A colour executes as a parallel batch, then
the next colour begins. Equal-colour cells have disjoint one-cell neighbourhoods,
so the existing eight-neighbour/layer-aware flow can update shared destination
deltas without atomics or locks. This keeps the physical water model and mass
accounting intact while opening the large cell loops to multiple CPU cores.

Future optimization should reduce the amount of scheduled work through
active/dirty regions and sleeping settled water before simply increasing thread
count. See `PERFORMANCE_MULTICORE_ROADMAP.md`.

## Diagnostics and testing

The Racing United Vehicle > Surfaces panel exposes hydrology counts, water volume,
maximum depth, runoff, infiltration, drainage, evaporation, tire-cleared water,
spray, simulation time, reset controls and a debug overlay. The overlay shows wet
cells and their local flow directions near the camera.

Normal scene presentation does not depend on that debug toggle. Nearby wet cells
are currently expanded on the GPU into overlapping surface-aligned water quads.
The authoritative field is spatially connected, but the current presentation still
exposes its cell construction in some views; a later connected/interpolated surface
reconstruction must hide those cell boundaries without increasing physics resolution.
Two procedural, world-anchored ripple bands are advected in the authoritative local flow direction;
water depth controls film opacity and calm-pool character, while the renderer's
day/night environment cubemap supplies roughness-aware reflections. Soft terrain
uses a weaker response than asphalt/kerbs/paint. This is an animated procedural
texture, so it requires no repeating image sequence and cannot swim when the
camera moves.

`SurfaceWorldRegression` verifies rainfall, depression pooling, exact local water
sampling, tire clearing and reset. The wet 150-car / 600-tire laboratory adds a
synthetic spatial road, one 30 Hz water solve per scheduled interval and every
1000 Hz tire-water contact to its timed workload.

## Deliberate boundaries

- Drain inlets and unusual porous surfaces require metadata; the engine cannot
  infer an underground storm-water system from the visible mesh.
- Soil saturation and groundwater are simplified infiltration, not geotechnical
  simulation.
- Spray mass currently leaves the surface-water layer instead of being advected
  through the air and redeposited behind another car.
- The current water pass reflects the procedural day/night environment. Local
  cars, buildings and neon require a later screen-space/planar reflection tier;
  hydrology and flow animation do not need to change when that tier is added.
- Snow, slush, ice and melt require their own phase-change layer.
- The grid captures sub-metre pooling; phenomena smaller than a cell require a
  finer authored/baked tier rather than per-drop fluid physics.

## JOB02 water-presentation performance policy

The first live multicore capture showed that normal water presentation could
remain expensive even after the hydrology solver's 30 Hz maximum cadence was
distributed across workers. JOB02 separates authoritative update cadence from
render cadence:

- visible-cell selection, far aggregation, record packing and VBO upload occur
  when the hydrology simulation advances (normally 30 Hz), when topology changes,
  or when the camera leaves a small selection-cache margin;
- the cached record positions are FP32 relative to an FP64 presentation origin;
  each rendered view only converts that origin camera-relative, so camera motion
  does not force tens of thousands of cell positions to be rebuilt;
- the GPU buffer remains resident between updates and is orphaned on refresh to
  avoid synchronous reuse stalls;
- water quads are rendered as instanced triangle strips. The old geometry shader
  that expanded one point into a quad has been removed;
- the physical 0.5 m grid is unchanged. PERF12 uses adaptive presentation in every
  explicit-water ring from 0–200 m. A complete planar wet region may merge through
  1, 2, 4, 8 and 16 m patches while 0.5 m remains the fallback/source resolution.
  The 0–50 m presentation begins with 1 m candidates and caps patches at 8 m; 50–200 m begins at 2 m and may reach 16 m. Wet/dry boundaries,
  curbs, material changes and non-planar geometry stop the merge and retain finer
  records. Small rain-film depth/flow differences are tolerated for presentation so
  broad flat surfaces do not remain a checkerboard. Beyond 200 m the authoritative
  field persists but explicit water geometry is not rendered.

Procedural ripple phase and environment reflection still advance every rendered
frame, so visual water keeps moving smoothly while depth/flow authority stays at
the appropriate hydrology cadence. There is no special 100 m fine/coarse switch:
all presentation slices use the same adaptive policy and cross-fade per fragment.
The 50–100 m and 100–200 m cadence regions are internally divided into phased
radial slices to distribute collection/upload work; this does not change their
6 Hz or 2 Hz authoritative cadence. Simulation resolution and visible surface tessellation
remain deliberately independent.


## JOB03 distance-adaptive authoritative cadence

JOB03 keeps hydrology authoritative at its original 0.5 m physical resolution but
reduces how often expensive weather and downhill-flow source work runs as distance
from relevant simulation actors increases. The scheduler uses world-space player /
vehicle interest sources, not cosmetic camera positions.

| Minimum distance to any interest source | Source-solve cadence |
|---|---:|
| 0–25 m | 30 Hz |
| 25–50 m | 20 Hz |
| 50–100 m | 6 Hz |
| 100–200 m | 2 Hz |
| >200 m | 0.5 Hz background persistence |

For multiple local players the distance is evaluated independently for every source
and the minimum is used per hydrology chunk. Two players 800 m apart therefore get
two independent near-field islands; Heritage does **not** synthesize a midpoint
between them. A regression explicitly checks that the midpoint remains on its own
distant cadence.

Slow chunks are deterministically phase-staggered to avoid periodic whole-field
2 Hz/6 Hz spikes. Their physical dt is still the actual elapsed time since that
chunk last ran, so rainfall, infiltration, drainage and evaporation preserve real
time. The existing exponential bounded-flow integration handles the longer source
intervals while retaining the 48% per-solve mobile-water safety cap.

The current background tier remains 0.5 Hz rather than fully sleeping so globally
raining areas do not become incorrectly dry merely because no player is nearby. It
is authoritative persistence only: PERF11 does not render explicit water beyond
200 m. A later true sleep system must provide correct lazy catch-up before 0 Hz is allowed.

## WATER04 terrain-conforming wetness and rain wetting fronts

Thin rain film is no longer meant to be represented near the player by raised
half-metre water cards. `EntityMeshRenderer` maintains a bounded near-field
presentation atlas sourced from `SurfaceHydrology`; tagged creator-world geometry
(`SurfaceWetnessReceiver`) samples that atlas in the normal PBR material shader.
The cache is presentation-only: hydrology depth, flow, drainage and tire clearing
remain authoritative in `SurfaceHydrology`.

The visual dry-to-wet transition is deliberately progressive. A deterministic,
world-anchored droplet lattice first produces small dark circular wet marks.
Authoritative water depth expands those circles, activates more marks and finally
merges them into a connected film. Wet coverage darkens diffuse response and
reduces material roughness rather than painting an opaque blue layer over the
road. Visual impacts do not create water mass.

The atlas is aligned to hydrology cells, uses camera-relative shader addressing
from an FP64 CPU origin, and stores two candidate surface elevations per X/Z
texel so common road/bridge or road/tunnel stacks do not project one wet layer
onto unrelated geometry. Its refresh cadence is bounded independently from the
render rate.

Standing water remains a separate reflective pass. Its presentation height now
uses `surface elevation + water depth`, deeper pools trend toward a horizontal
free surface, shallow near-field cards yield to the terrain receiver, and active
rain produces world-space circular ripple rings on pooled water. See ADR-079.

## WATER05A OpenGL rain-wetting presentation

After WATER04C restored the stable JOB03 renderer, WATER05A keeps the general entity
material path isolated and implements rain-onset wetting only in the dedicated
OpenGL water presentation pass. Thin hydrology film is no longer intended to read
as blue water laid over the terrain. Deterministic world-anchored circular wetting
fronts appear sparsely at first, expand and overlap as authoritative film depth
grows, and finally merge into a continuous darker wet material. Active rainfall
only animates recent impact/ripple detail; `SurfaceHydrology` remains the sole water
mass authority. Deeper water progressively receives stronger Fresnel/environment
reflection. Connected puddle/free-surface reconstruction remains a later visual
step and must operate on coherent regions rather than rotating independent cells.

## WEATHER07A precipitation authority

SurfaceHydrology continues to receive rainfall as authoritative liquid depth in
mm/hour. WEATHER07A adds `Physics/Weather/RainMicrophysics` and
`PrecipitationField` alongside that bulk mass authority so visual/impact systems
can ask what the statistically represented drops are doing without changing how
much water reaches the world.

This separation is deliberate: a graphics quality setting may render a few
thousand or tens of thousands of representative drops, but hydrology receives
the same rainfall mass. The population is numerically normalized back to the
requested mm/hour rate, and world-cell drop identity is independent of camera
position. Explicit world wind heading now drives precipitation trajectories and
cloud advection instead of renderer-local hard-coded direction vectors.

## PERF13 seamless visible-water material

PERF13 keeps PERF12 adaptive presentation but changes how every water patch is
constructed and shaded. Hydrology blocks are axis-aligned in world X/Z, so the
vertex shader now uses those exact X/Z extents and solves corner Y from the fitted
support plane. Water therefore follows a planar road grade/camber while adjacent
0.5/1/2/4/8/16 m patches share exact presentation edges. The previous 1% transparent
quad oversize is removed because alpha overlap itself could reveal patch seams.

Procedural flow waves, roughness variation and rain rings use a continuous bounded
world-X/Z phase. They do not restart per patch and do not scale with adaptive patch
size. Thin authoritative film is presented as a neutral darker/glossier overlay;
standing water progressively becomes smoother, more reflective and more visibly
rippled. The depth response saturates quickly in the thin-film regime so harmless
cell-to-cell micrometre differences do not recreate a checkerboard.

PERF13 requires no water texture asset. Optional future fine/broad seamless normal
maps may be layered in world space after the procedural material is proven live.
They must preserve this continuity contract and are presentation-only.
