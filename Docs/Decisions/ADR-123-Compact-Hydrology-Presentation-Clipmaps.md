# ADR-123 – Compact Hydrology Presentation Clipmaps

## Status
Accepted for WATER15G.

## Decision
Keep authoritative adaptive hydrology separate from rendering, but replace the WATER15F 4K presentation hierarchy with a compact 1024/512/256 hydraulic-head mip chain covering 128/512/2000 m squares. Presentation refresh runs at 15/5/1 Hz and topology churn must respect those cadences. At most one clipmap rebuild is submitted per frame. The material shader selects one level by distance and samples a second only during transition bands; sub-2.2 mm film takes a two-sample hardware-filtered fast path while pooled near/mid water uses exact support-aware reconstruction.

Thin water is a wet-material response. The shoreline breakup asset may perturb presentation depth at sub-millimetre scale but never authoritative hydrology. Standing-water optics require real millimetre-scale pooling.

## Rationale
The 4K experiment spent bandwidth and memory to magnify solver-cell structure rather than adding information. Its all-level per-fragment sampling and large offscreen raster work were not scalable. Compact clipmaps preserve exact-surface, bridge-aware hydraulic-head presentation while restoring a sane GPU/CPU budget.

## Consequences
Near presentation pitch is 12.5 cm rather than 2.44 cm, but the system is no longer expected to encode visual shoreline detail solely through atlas resolution. Sub-cell appearance comes from exact authored geometry plus presentation-only shoreline erosion that never increases physical depth. True higher physical precision must come from hydrology/collider data, not oversized presentation textures.
