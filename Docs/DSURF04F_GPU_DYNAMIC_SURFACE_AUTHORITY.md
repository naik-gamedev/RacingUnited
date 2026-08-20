# DSURF04F — GPU Dynamic Surface Authority Cutover

## Status

DSURF04F promotes the centimetre-scale GPU LOD workload from the DSURF04E profiling gate into the live Water/Snow/Mud Dynamic Surface authority.

The fundamental rule is now:

> The GPU fields advanced by compute are the same fields sampled by the road/terrain material shader. There is no permanently separate low-resolution water simulation plus a cosmetic puddle map.

The former 64x64 CPU Hydro implementation remains compiled as startup/regression fallback, but `SurfaceWorld` stops advancing it after the GPU authority reports ready.

## Camera-bounded 100 m tile LOD envelope

Each logical world tile is 100 m x 100 m. The active camera-centred envelope is:

| LOD | World ownership | State resolution | Cell size | Publication cadence |
|---|---|---:|---:|---:|
| LOD0 | camera's current 100 m tile | 8192 x 8192 x 1 | 1.220703125 cm | 2 Hz |
| LOD1 | first ring of 8 neighbour tiles | 4096 x 4096 x 8 | 2.44140625 cm | 1 Hz |
| LOD2 | second ring of 16 tiles | 1024 x 1024 x 16 | 9.765625 cm | 0.5 Hz |
| dormant | beyond the second ring / 300 m visual horizon | no high-resolution rendering | — | 0 Hz |

Work is split into 128-row units and accumulated continuously across render frames. A published field swaps only after a complete logical cycle, avoiding partially updated visible images. The renderer fades Dynamic Surface optical detail from the near field toward 300 m and does not render the high-resolution authority beyond that horizon.

## Live state formats

### WaterState — `R32UI`

One 32-bit word per water cell:

- bits 0..15: nonlinear water depth, 0..0.50 m;
- bits 16..23: signed X velocity;
- bits 24..31: signed Z velocity.

The depth encoding uses a square-root transfer before quantization. This keeps substantially more precision near the shallow films and millimetre-scale puddles that matter on roads while retaining a 0.5 m emergency range. Flow direction and speed are derived from the stored X/Z velocity; there is no redundant direction channel.

Water compute reads neighbouring water-head values, static support height and a deterministic millimetric world-space microtopography field. That microtopography participates in the physical authority rather than being a presentation-only puddle mask, so fine organic boundaries originate in the state being simulated. Atmospheric precipitation is temperature-partitioned so the same precipitation mass is not independently added to both WaterState and SnowState around freezing.

### SnowState — `R16UI`

- low 12 bits: nonlinear snow depth, 0..1 m;
- high 4 bits: compaction, 0..15.

Snow accumulation/melt is computed directly in this field. Tire-contact compute depresses and compacts the same cells the renderer samples.

### MudState — `R8UI`

The 8-bit value is persistent rut/depression depth at 1 mm per code step. Tire-contact compute modifies the same field used by rendering. Broad mud relaxation/soil transport remains a later specialization; DSURF04F establishes the live centimetre-scale authority and contact path.

## Ping-pong ownership

Water, Snow and Mud each own two identically sized GPU images per LOD. Compute reads the current published image and writes the next image. Both sides are initialized, and tire-contact events update both sides so a contact cannot be lost when the next scheduled publication swaps buffers.

This is logically one authority per phenomenon; the second image is simulation scratch required for deterministic neighbour reads, not a second world state.

## Tire contact bridge

High-rate CPU tire contacts are aggregated into world-anchored 10 cm buckets before the renderer consumes them. The GPU then issues small footprint compute dispatches that:

- clear/accelerate WaterState;
- depress/compact SnowState;
- deepen MudState only on mud/soft-soil contacts rather than rutting asphalt.

This prevents 1000 Hz tire contacts from becoming thousands of tiny unbounded GPU submissions while keeping the disturbance located at the real contact patch.

The immediate CPU tire-force query currently uses the smooth weather-film fallback while the GPU-authority path is active. This deliberately avoids running a second CPU water solver. A future small asynchronous GPU contact-sample bridge can return exact centimetre WaterState to tire physics without changing authority ownership.

## Static support and vertical sheets

The existing Dynamic Surface static bake/page pool remains the source of sheet-aware support height and therefore still distinguishes bridge deck, road below, sidewalks and other vertically stacked receivers. It is static/support infrastructure, not a second dynamic Water/Snow/Mud authority.

## Camera-tile rebasing

DSURF04F1 remains bounded to the active 5x5 camera-centred 100 m envelope, but 100 m centre-tile changes no longer erase the authority. Every destination ring layer is populated from the same overlapping world tile in the old envelope when available; only tiles newly entering the 300 m envelope start empty. Cross-ring moves are resampled between 8192/4096/1024 storage so water/snow/mud stay world anchored while the camera moves.

This limitation is preferable to camera-relative false persistence and is documented explicitly so it cannot become accidental permanent behavior.

## Rendering

The normal authored surface material samples WaterState/SnowState/MudState directly:

- Water depth is manually interpolated after packed integer decode;
- water ripples and Fresnel strength are progressively simplified with LOD/distance;
- the complete Dynamic Surface visual contribution fades between roughly 250 m and 300 m;
- Snow and Mud use the same world/ring addressing and fade horizon;
- no generated water mesh, clipmap, puddle canvas or separate high-resolution cosmetic water texture is introduced.

The DSURF04D2 64x64 organic reconstruction remains only as an initialization/failure fallback when the GPU authority cannot be created.

## Memory envelope

A completely allocated ping-pong envelope is intentionally large and is measured live:

- WaterState: approximately 1664 MiB;
- SnowState: approximately 832 MiB;
- MudState: approximately 416 MiB;
- total: approximately 2912 MiB before unrelated renderer resources.

DSURF04F requires all three live state families to allocate successfully before authority activation. F8 reports allocation readiness, committed MiB, CPU dispatch time, asynchronous GPU compute time, cells processed, per-LOD water dispatches, publication cycles and tire-event work.

This milestone is therefore both the authority cutover and the first truthful hardware gate for the complete requested state envelope.

## DSURF04F2 static-support ownership correction

The high-resolution GPU authority must not depend on retired CPU Hydro revisions to obtain static world/surface-sheet support. Resident support metadata is uploaded once per physical-slot generation directly from immutable baked Dynamic Surface geometry when needed. This keeps Water/Snow/Mud compute coverage valid after CPU Hydro advancement is disabled.

## DSURF04F3 wet-film / free-surface optical correction

Live DSURF04F2 testing proved that the GPU authority was finally accumulating and
rendering water, but total millimetric rain depth was being interpreted as a
free-standing dielectric layer over every receiver. That made the entire road
read like a translucent plastic cover rather than wet asphalt plus local puddles.

DSURF04F3 does **not** introduce a second water authority. `WaterState` remains
the sole physical depth/velocity field and remains the exact field sampled by
the renderer. The material merely separates two optical regimes already required
by ADR-124:

- the continuous weather film controls ordinary wet-material darkening and PBR
  roughness;
- only GPU WaterState depth that exceeds the continuous film baseline by at
  least 1.5 mm is eligible for free-surface puddle Fresnel/transmission/ripples;
- Beer-Lambert optical depth and pooled-water normals use only this excess
  free-surface depth, preventing a grey environment map from coating the whole
  scene in an opaque/silver plastic look.

This is an optical interpretation of one authoritative field, not a return to a
coarse physics / separate pretty-water reconstruction.

## DSURF04F4 single local water authority correction

Live DSURF04F3 testing disproved the assumption that the old `SurfaceWeather`
film could remain part of normal GPU-authority optics. The legacy scalar was
still painting every wetness receiver uniformly while the centimetre-scale GPU
`WaterState` accumulated independently, and its depth was also being subtracted
from local GPU depth before free-surface puddles were allowed. That created a
visual double authority even though only one hydrology solver was advancing.

DSURF04F4 removes that coupling. While GPU authority is active:

- local wet-material film and free-surface puddles are both derived from the
  exact local `WaterState` depth sampled by the road/terrain shader;
- `SurfaceWeather.waterFilmDepthM` / `effectiveWetness` remain compatibility,
  telemetry and startup-fallback values only and do not paint the live GPU
  receiver uniformly;
- immutable baked per-surface hydrology metadata is uploaded alongside static
  support as `RGBA16F`: infiltration capacity, drainage capacity, signed
  roughness/exposure and depression storage;
- the Water compute shader consumes those static parameters plus current
  weather drainage/evaporation, so shelter, infiltration and drainage shape the
  same `WaterState` that is rendered;
- no second high-resolution presentation water field or CPU Hydro solver is
  introduced.

This correction makes the architectural invariant explicit: **one local GPU
WaterState is the water/wetness authority; static support/material metadata and
weather are inputs, not competing state.**

## DSURF04F5 transport/optical correction

The 8192² authority exposed a temporal-resolution problem: one immediate-neighbour flow exchange per 0.5 s publication interval allowed heavy rain to accumulate as a broad shallow sheet faster than centimetre cells could transport it downhill. DSURF04F5 keeps the requested 2/1/0.5 Hz weather-source ladder but gives WaterState 4/2/1 smaller internal GPU transport solves at LOD0/1/2. Snow and Mud retain their original cadence. Pending work above the per-frame dispatch guard remains queued rather than silently dropped.

World-stable authority microtopography is now multi-scale value noise with millimetric amplitude rather than long periodic sine waves. It participates in the water-head solve, so irregular retained pools are physical WaterState rather than a presentation mask.

Optically, up to 2.5 mm of local mobile runoff remains in the wet-material regime. Free-surface reflection/normal flattening is gated by both excess depth and WaterState flow speed, preventing a moving rain film from turning an entire receiver into a plastic-looking sheet.

## DSURF04F6 world-neighbour topology correction

Live F5 testing exposed a solver-topology bug: `stateAt()` clamped neighbour coordinates to the current texture-array layer, so every 100 m world tile behaved as a closed hydrological box even though rendering could sample across tile boundaries. F6 removes that clamp. Neighbour reads that leave the current layer are converted to world coordinates, mapped to the adjacent LOD0/LOD1/LOD2 world tile, and sampled from the currently published WaterState authority. This makes a 100 m ownership edge transparent to the hydraulic solve.

F6 also replaces the cardinal-only N/E/S/W flow stencil with an eight-neighbour Sobel gradient and weighted diagonal conservative exchanges. The previous four-neighbour stencil had a square/diamond anisotropy that could become visible in runoff and puddle contours.

The F5 procedural value-noise microtopography is removed from physical hydraulic head. It could generate visible contour streaks on sloped receivers. F6 uses authored/static support elevation only. A future high-resolution static support bake may increase geometric detail, but it must be derived from authored surface geometry rather than presentation noise.

## DSURF04F7 periodic optical-pattern removal

Live F6 testing showed a repeated short-wave highlight motif across unrelated road
and hillside receivers. The remaining repetition was not WaterState tile ownership:
the material shader still superimposed three deterministic sine-wave normal fields
with fixed sub-metre wavelengths on every free-surface fragment. Once broad rain
coverage existed, the same interference pattern appeared everywhere and visually
resembled copied water tiles.

F7 removes those synthetic periodic ripple normals from the GPU-authority path.
Centimetre WaterState depth remains the spatial authority and pooled water only
relaxes its optical normal toward gravity-up. Any future rain-impact/ripple system
must be driven by non-periodic world/event data and must not stamp a small reusable
wave basis across all receivers. This change intentionally does not hide or modify
the underlying hydraulic state; if coarse repetition remains after F7, the next
input to promote is the current 64x64 static support/topography field.

## DSURF04F8 exact high-resolution geometry authority

Live F7 testing exposed that WaterState storage/rendering was centimetre-scale while hydraulic ground/support still came from the legacy 64×64 support raster. F8 removes that mismatch. Water/Snow/Mud compute no longer binds or samples `uSupportPages`, `uSupportIndirection`, or `uStaticHydrologyPages`. Instead, the immutable authored collision triangles for the active 5×5 world window are uploaded to GPU SSBOs. A 256×256 bin index per 100m tile only narrows the candidate triangle list; it stores no height, water, snow, mud or material state. Each 8192/4096/1024 authority texel evaluates its own exact world-space position against the candidate triangle planes and obtains the local height and authored hydrology parameters directly.

Therefore the live water chain is now: exact authored triangle geometry at the high-resolution texel coordinate → R32UI WaterState compute → the same published R32UI WaterState sampled by the material shader. The legacy 64×64 GPU Hydro/support page pool remains compiled only as startup/failure fallback and is not synchronized once the exact-geometry authority is active.


## DSURF04F9 finite-volume high-resolution water

Live F8 testing proved that centimetre storage and exact triangle evaluation alone were not sufficient: the prototype eight-neighbour head-relaxation solver could settle into visible band/lattice patterns and did not evolve conservative momentum through physical cell faces. F9 replaces that transport core while preserving the exact same GPU WaterState authority and renderer source.

WaterState now advances as a finite-volume shallow-water control volume. Each authority texel stores depth and X/Z velocity; compute decodes these into conserved depth and momentum, evaluates exact authored support at the center and four face neighbours, performs hydrostatic reconstruction at each face, and uses a local Lax-Friedrichs/Rusanov numerical flux for mass and momentum. Hydrostatic pressure correction balances bed elevation so a lake-at-rest does not accelerate merely because neighbouring authored triangle elevations differ.

The requested 2/1/0.5 Hz publication ladder and 4/2/1 internal Water solves remain unchanged for the first live gate. Because centimetre cells can have a tighter mathematical CFL limit than those externally scheduled intervals, every face applies a symmetric donor-volume limiter: no one face may remove more than 22 percent of the actual donor depth in one solve. This preserves non-negative water and bounds large-step transport without converting the renderer into a second water model. Authored roughness is applied as semi-implicit Manning-style momentum friction after conservative face transport; infiltration, drainage, evaporation and precipitation remain source/sink terms on the same WaterState.

F9 also removes camera height from exact-geometry receiver selection. At each exact X/Z the authored persistent `surfaceSheetId` deterministically chooses the receiver; camera Y is not a water-ownership input. The current single WaterState layer per world tile can still represent only one vertically overlapping sheet at a given X/Z, so fully sparse multi-sheet high-resolution allocation remains future work. This limitation is explicit rather than hidden behind camera-relative selection.

F9 also changes the signed 8-bit velocity quantizer from a linear ±4 m/s mapping to a square-root encoded mapping. The physical range remains ±4 m/s, but shallow/slow runoff gets much finer precision near zero. This avoids turning the renderer's still-water classification into visible bands simply because linear 8-bit velocity had ~3.15 cm/s steps.


## DSURF04F10 concurrent visible-ring scheduling

F9 still serialized row work by texture-array layer: a whole LOD1 or LOD2 world tile could be serviced before the next tile in the same visible ring. F10 changes the compute topology so one row work unit spans every layer of a ring using the dispatch Z dimension. Thus all 1 + 8 + 16 visible world tiles are live simultaneously at their requested LOD cadence. The total cell workload is unchanged; CPU dispatch overhead is lower because one dispatch services 8 or 16 layers together. State migration on camera-tile changes is now only LOD reassignment for already-live tiles.


## Superseded by DSURF04G

The DSURF04F9/F10 100 m 8192²/4096²/1024² simulation-resolution LOD scheme is superseded by ADR-136. Current WaterState uses 10 m / 512² fixed physical resolution everywhere resident; only cadence and rendering presentation vary with distance. This file remains the historical record of the GPU-authority cutover.
