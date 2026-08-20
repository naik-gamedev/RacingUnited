# ADR-079 — Rain Wetting Fronts and Terrain Wetness Receiver

## Status
Accepted for WATER04.

## Context

The authoritative hydrology layer already produces spatial water depth, downhill
flow, drainage and tire clearing on a 0.5 m world grid. Rendering every shallow
wet cell as an independent raised quad made that implementation detail visible:
sloped terrain showed overlapping/faceted cards and rain looked like a tiled blue
overlay rather than a road becoming wet.

Rain also needs a convincing dry-to-wet transition. The desired presentation is
not an instantaneous whole-material switch: individual drops first make small,
dark circular marks, those marks spread and overlap, then the surface becomes a
continuous darker/smoother wet film. This visual process must remain a consumer
of hydrology rather than becoming a second water simulation.

## Decision

WATER04 adds a near-field **surface wetness receiver** to the normal PBR mesh
shader. A compact renderer-owned atlas is rebuilt from authoritative hydrology at
bounded presentation cadence. It is not authoritative state.

The atlas:

- is 256×256 texels at the active hydrology cell size (normally 0.5 m);
- is aligned to exact world hydrology-cell boundaries so camera motion cannot
  make wetness swim;
- is FP64-addressed on CPU and supplied camera-relative to the shader;
- stores up to two vertical surface layers per X/Z texel as
  `(wetness, elevation)` pairs so road/bridge or road/tunnel stacks do not
  blindly project one layer onto another;
- recentres only after a camera safety margin is crossed and otherwise refreshes
  no faster than 10 Hz when hydrology advances;
- uses a generic `SurfaceWetnessReceiver` entity tag. Racing United's creator
  world opts in through that tag; the engine does not hard-code a Racing United
  scene/entity name.

The PBR surface shader samples the atlas only for tagged geometry and validates
fragment height and upward-facing orientation. Hydrology depth drives a
world-anchored procedural wetting pattern:

1. first visible rain produces small dark circular impact marks;
2. authoritative depth makes those circles persist and expand;
3. more circles activate as depth rises;
4. overlapping circles merge into a connected film;
5. wet coverage darkens diffuse response, reduces roughness and strengthens the
   dielectric wet sheen while retaining material character.

The procedural droplet lattice uses a 4096 m presentation period exactly divisible
by its 0.125 m spacing. Camera-global phase is passed modulo that period, so the
pattern remains stable through floating-origin rebases without requiring low
precision global shader coordinates.

Individual visual impacts never add water mass. Rainfall authority remains
`SurfaceWeather -> SurfaceHydrology`.

## Standing-water presentation

The separate water pass remains for real standing/free-surface water. WATER04
changes it so:

- near-camera micron film is suppressed where the terrain receiver represents it;
- visual water elevation follows `surface elevation + authoritative water depth`
  rather than a fixed 6 mm lift;
- deeper water normals tend toward world-up, reflecting the behavior of a free
  surface instead of inheriting every terrain-cell facet;
- the old 1% quad overlap is nearly removed to stop alpha seams;
- water tint is much less blue/opaque and reflections carry more of the look;
- three non-commensurate world-space wave bands replace obvious repeated sine
  streaks;
- deterministic world-space rain impacts perturb standing-water normals as
  expanding circular ripple rings when precipitation is active.

## Consequences

- Thin rain wetness now conforms to the authored render mesh exactly instead of
  floating over it as 0.5 m cards.
- The hydrology grid remains authoritative and unchanged in spatial resolution.
- The near wetness atlas is a bounded presentation cost and can be scaled later
  without changing physics.
- More than two vertical water-bearing surfaces at the same X/Z are reduced to
  the two closest to the active view height. A future truly volumetric/multi-view
  wetness cache can supersede this without changing hydrology authority.
- Future WEATHER06 visible rain streaks should derive impact timing/seed from the
  same world precipitation field if exact streak-to-impact correspondence is
  desired. WATER04 establishes the world-anchored surface response first.
