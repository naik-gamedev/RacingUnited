# PERF12 - Adaptive Water Mesh v2

## Goal

PERF11 proved the cadence-aligned water-cache architecture but deliberately kept the 0–100 m presentation at 0.5 m tiles. Live debug visualization on the flat Racing United parking-lot surface therefore still showed a dense checkerboard around the vehicle even though the authored support plane was effectively identical across large regions.

PERF12 makes adaptive presentation a property of the entire rendered 0–200 m water field rather than only the 100–200 m ring. The authoritative hydrology grid remains 0.5 m everywhere.

## Authoritative cadence — unchanged

Distance is evaluated to the nearest hydrology simulation-interest source.

| Distance | Authoritative source-solve cadence | Explicit water presentation |
|---|---:|---|
| 0–25 m | 30 Hz | adaptive, 0.5 m fallback; up to 8 m planar patches |
| 25–50 m | 20 Hz | adaptive, 0.5 m fallback; up to 8 m planar patches |
| 50–100 m | 6 Hz | adaptive, 0.5 m fallback; up to 16 m planar patches |
| 100–200 m | 2 Hz | adaptive, 0.5 m fallback; up to 16 m planar patches |
| >200 m | 0.5 Hz persistence | not rendered |

The update cadence and the render mesh resolution are deliberately independent. A 16 m render patch does **not** mean a 16 m physics cell; it is only one GPU presentation record standing in for many compatible 0.5 m authoritative cells.

## Planar adaptive hierarchy

Adaptive collection uses a dense local hierarchy rather than rebuilding hash maps every refresh. In the 0–50 m field, complete 2×2 groups of 0.5 m hydrology cells form the first 1 m candidate. From 50–200 m, the hierarchy begins at 2 m to reduce collection work while retaining 0.5 m fallback cells wherever a coarse block is invalid. Compatible groups may recursively merge through:

`0.5 m fallback -> 1 m -> 2 m -> 4 m -> 8 m -> 16 m`

The renderer always prefers the largest valid patch up to the budget of the current cadence region. The dense arrays keep grouping and parent traversal cache-friendly; no per-refresh unordered-map hierarchy is required.

A patch is allowed to merge only when all required source cells exist, share a material, are wet when dry cells are excluded, and fit one support plane within bounded normal/elevation error. A sloped but planar road is valid; horizontal-only merging is explicitly not required.

Small rain-film depth and flow differences no longer force a flat parking lot back to 0.5 m tiles. Those values are averaged for presentation only. Wet/dry topology, missing cells, material changes and non-planar support still refuse the merge, protecting puddle boundaries, curbs, drains and sharp geometry.

## Smooth distance handoffs

There is no longer a special 100 m fine/coarse LOD switch. Every explicit-water presentation slice uses the same adaptive representation.

The 0–25 m and 25–50 m cadence regions each retain one cache. To protect 0.1% lows, the 50–100 m 6 Hz region is split into two phased radial presentation slices and the 100–200 m 2 Hz region into four phased slices. This does **not** change hydrology cadence: every point in those regions still uses 6 Hz or 2 Hz authoritative updates respectively. It only prevents one large adaptive collection/upload from landing on a single frame.

The shader evaluates distance per fragment and cross-fades adjacent slices. The last slice fades to zero by 200 m. Because a large patch is faded per fragment rather than by its center, an 8 or 16 m planar quad can safely straddle a slice boundary without appearing or disappearing as one block.

World-anchored ripple, wet-spot and reflection coordinates remain independent from patch size, so coarser geometry does not enlarge the procedural texture pattern.

## Diagnostics

F8 reports:

- total water GPU records;
- 0.5 m fallback record count;
- merged record count;
- 1/2/4/8/16 m patch counts;
- largest active patch;
- cache refresh count;
- collect, pack/upload and draw CPU timings.

The cyan hydrology presentation overlay renders the **actual presentation patches**. On a sufficiently flat uniformly wet parking lot, PERF12 should therefore visibly show large cyan blocks instead of a uniform 0.5 m checkerboard.

## Non-goals

- PERF12 does not merge or delete authoritative hydrology cells.
- It does not change the 30/20/6/2/0.5 Hz solver cadence.
- It does not average split-screen/player positions into a midpoint.
- It does not render explicit water beyond 200 m.
- It does not force a merge across a curb, puddle shoreline, material boundary or support-plane break merely to reduce draw records.
