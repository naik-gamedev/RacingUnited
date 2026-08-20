# PERF11 - Adaptive Water Presentation and Cadence Rings

## Goal

Live F8 captures showed substantial CPU-side surface submission/collection cost while the GPU frame remained comfortably shorter. PERF11 reduces water work without reducing the authoritative 0.5 m hydrology field or deleting persistent distant weather state.

## Authoritative hydrology cadence

Distance is measured from each hydrology chunk to the **nearest** simulation-interest source. Multiple local players therefore create a union of independent influence regions; there is no midpoint source.

| Distance | Authoritative source-solve cadence | Explicit water presentation |
|---|---:|---|
| 0–25 m | 30 Hz | 0.5 m |
| 25–50 m | 20 Hz | 0.5 m |
| 50–100 m | 6 Hz | 0.5 m |
| 100–200 m | 2 Hz | adaptive, 2 m baseline |
| >200 m | 0.5 Hz persistence | not rendered |

The existing deterministic hydrology chunk phase staggering remains authoritative. The distance changes do not average player positions or change the physical cell size.

## Cadence-aligned presentation caches

The old presentation path maintained one visible-water cache and rescanned/reuploaded its whole radius whenever the maximum-rate hydrology step advanced. That made slow distant water presentation wake at the near-water cadence.

PERF11 uses four persistent GPU/VBO caches aligned to the 30/20/6/2 Hz distance regions. Each cache has its own FP64 presentation origin, CPU staging storage, GPU capacity and refresh state. Only a ring that is due, changed topology, or has been outrun by camera motion is collected and uploaded.

Presentation cadence phases are offset deterministically, so the slower 20/6/2 Hz cache refreshes do not intentionally synchronize with one another. Camera-motion safety margins let each cache cover its visible band between refreshes.

## Adaptive 100–200 m water mesh

The far presentation begins with complete 4×4 groups of 0.5 m authoritative cells, producing a 2 m candidate patch. Four compatible patches may merge recursively to 4 m, then 8 m, then 16 m.

A merge is allowed only when the source region is sufficiently redundant. The block must have complete spatial coverage and compatible material, surface plane/normal, water-depth range and flow range. Normalized plane-error checks allow a genuinely planar sloped road to merge; the surface does not have to be horizontal.

A region refuses to merge when information would be lost. Wet/dry boundaries, incomplete topology, curbs, drains, sharp geometry changes, material boundaries, strong water-depth gradients or changing flow fall back toward the original 0.5 m cells. This prevents a small puddle from becoming an averaged giant sheet.

Large patches are also kept away from the 100 m LOD handoff. The transition region uses the conservative 2 m representation.

## Smooth LOD and cadence-ring handoffs

Water records carry a presentation LOD class. The shader evaluates the current camera distance every rendered frame, so cached records do not bake stale fade weights.

The fine 0.5 m and adaptive coarse representations overlap and cross-fade from 94–106 m. The 25 m and 50 m cadence-ring boundaries also use complementary shader fades, while the far representation fades to zero by 200 m. Procedural ripple/reflection animation remains render-rate smooth independently of hydrology update cadence.

## Performance diagnostics

F8 now reports total water GPU records, fine/coarse record counts, the largest active adaptive patch, water cache refreshes during the current frame, and collect/pack/upload/draw CPU timing. This makes it possible to tell whether a future hitch comes from authoritative hydrology, water collection, upload, or another surface subsystem.

## Non-goals

- No physics cells are merged or deleted.
- No water state is simulated at a cosmetic midpoint between players.
- The 0.5 Hz background field beyond 200 m remains authoritative even though it is not explicitly rendered.
- Adaptive merging does not cross wet/dry or material boundaries merely to meet a polygon budget.
