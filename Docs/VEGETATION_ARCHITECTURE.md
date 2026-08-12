# VEG01 — Vegetation / Biome Foundation

VEG01 establishes one shared Heritage Engine vegetation contract for trees,
shrubs, bushes, grass, reeds, flowers, crops and future plant families. It is
intentionally safe to ship before any octahedral impostor asset exists.

## Design rule

Octahedral impostors are a representation option, not the vegetation system
itself. Different plant classes may choose different representations.

- Trees: real trunk/meaningful branch geometry nearby, optional octahedral
  foliage clusters, merged foliage clusters farther out, optional whole-tree
  octahedral impostor at long range, then future forest HLOD/terrain coverage.
- Shrubs: small real stem structure where useful, cluster octahedrals nearby,
  whole-shrub octahedrals farther out.
- Grass: cheap real blade/card geometry nearby. An authored clump around
  0.5 m is a good candidate for an octahedral clump at medium distance. Far
  grass becomes patch/terrain coverage instead of millions of plants.

No plant is forced to use an octahedral representation when ordinary geometry
is cheaper or looks better.

## Large-world instance storage

VEG01 uses 64 m signed world chunks and 16-bit local coordinates for stored
vegetation placement. The local step is approximately 0.98 mm, avoiding FP16's
loss of precision at large world coordinates while keeping placement compact.
The current foundation instance is 32 bytes and can be packed further when the
streaming/terrain contract is finalized.

The registry is dormant when no instances exist. It does not iterate an empty
forest every frame.

## Species data

A species has:

- stable ID
- plant kind
- species-specific LOD distance bands
- optional cluster-octahedral capability
- optional whole-plant-octahedral capability
- optional terrain-coverage capability
- trunk/branch/foliage wind response weights

Lua can register species and placements through the `Vegetation` API. This is
primarily a stable configuration boundary for future generated assets rather
than a mandate to create vegetation in Lua.

## Reserved GLB metadata contract

VEG01 reserves these Blender/glTF Custom Properties for later asset discovery:

- `heritage.role = vegetation`
- `heritage.vegetation_type = tree | shrub | grass | reed | flower | crop`
- `heritage.species = <stable species id>`
- `heritage.wind_profile = <profile id>`
- foliage child: `heritage.role = foliage_cluster`
- foliage child: `heritage.impostor_type = octahedral`

VEG01 does not yet bake or render octahedral assets. VEG02 should implement the
first real branch/grass-clump bake and rendering shader against a reference
asset instead of guessing the final texture/depth layout.

## Wind contract

The native system already accepts one vegetation wind state:

- velocity vector in m/s
- normalized gust
- normalized turbulence

Species response is hierarchical: trunk, branch and foliage weights are
separate. Future weather can feed this field without creating separate tree,
shrub and grass wind systems.

## Performance direction

Future rendering must be chunk/visibility driven. A million plants in the
world database must not mean a million CPU updates or a hundred million active
foliage clusters. Near detail, medium merged clusters, whole-plant impostors,
HLOD and culling are progressively cheaper representations of the same world.
