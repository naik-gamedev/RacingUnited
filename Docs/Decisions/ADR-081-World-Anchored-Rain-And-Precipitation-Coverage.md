# ADR-081 — World-Anchored Rain and Precipitation Coverage

## Status

Accepted for WEATHER06D.

## Context

The first live WEATHER06C rain finally made precipitation visible, but it also
exposed two unacceptable presentation shortcuts. The fullscreen mid/far streak
veil was screen-space and visibly translated with the chase camera/vehicle,
recreating the classic “rain follows the player” illusion. The hard-visibility
pass also disabled depth rejection, so streaks could draw through scene geometry,
and the world-space drop lattice had no knowledge of covered volumes below
bridges, roofs or tunnel ceilings.

Heritage already owns a layered, world-space hydrology topology baked from
upward-facing collision surfaces. That data can provide a cheap precipitation-
exposure approximation without one CPU raycast per visible drop.

## Decision

1. The live fullscreen streak overlay is removed. Near visible streaks are the
   deterministic world-cell GPU lattice only. Middle/far precipitation remains
   atmospheric haze until a future true world-space volumetric rain integration
   replaces it.
2. Drop identity and jitter are hashed from absolute precipitation cells. The
   per-view lattice is only a recyclable presentation window; camera movement
   changes which world cells are visible and never translates the rainfall field.
3. The rain pass again uses Heritage reversed-Z scene depth (`GL_GREATER`) with
   depth writes disabled. Opaque scene geometry therefore clips streak fragments.
4. A cached 64×64 R32F precipitation-cover texture spans a 128 m square around
   the view. It is rebuilt only after meaningful camera translation or hydrology
   topology change. Each texel stores the highest upward-facing hydrology surface
   elevation in that world-space column.
5. A streak is suppressed when its lowest point falls below the sampled cover
   height. This prevents direct rain from existing in the covered air volume under
   bridge decks, roofs and tunnel/terrain cover without per-drop collision work.
6. The same layered topology also derives an authoritative `precipitationExposed`
   flag for every hydrology cell. Only the highest upward-facing surface in an
   X/Z column receives direct rainfall. Lower covered surfaces may still become
   wet through real flow/traffic redistribution, but rain does not pass through
   the bridge/tunnel into their water mass. The flag is derived after bake/cache
   load, so no hydrology cache-format change is required.
7. Cover heights are stored relative to an FP64 cache centre and sampled in FP32,
   preserving large-world precision.
8. Visible streak fall speed is presentation-only and may be tuned independently
   from authoritative precipitation mass. WEATHER06D uses a brisk 18–30 m/s
   visual streak velocity so heavy rain reads correctly at driving speeds.

## Consequences

- The Skyrim-style camera-attached rain layer is structurally absent.
- Rain is occluded both by current view depth and by world-space overhead cover.
- Hydrology remains the sole water-mass authority and now respects solid layered
  precipitation cover for direct rainfall.
- The cover map is coarse presentation data; it does not alter collision, drainage,
  rain mass or tire wetness.
- Vegetation transmission and partially porous cover remain future extensions.
- A later world-space volumetric mid/far precipitation tier should consume the same
  cover/exposure concept rather than reintroducing screen-space streak textures.
