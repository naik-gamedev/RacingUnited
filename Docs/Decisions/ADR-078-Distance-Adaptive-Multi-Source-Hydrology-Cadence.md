# ADR-078 - Distance-adaptive multi-source hydrology cadence

**Status:** Accepted for JOB03; cadence distances revised by PERF11.

## Context

JOB01 made hydrology genuinely multicore and JOB02 removed render-rate CPU rebuilding from normal water presentation. The remaining authoritative solver still performed expensive weather and downhill-flow source work across the whole baked field at the 30 Hz maximum cadence. Large persistent tracks need the same physical water resolution without paying near-player temporal resolution everywhere.

Split-screen and future multiplayer also make a single camera/player center invalid. Averaging two players would be especially wrong: it could promote water near an artificial point between them while reducing cadence around the actual players.

## Decision

Hydrology keeps one authoritative world-space field and one 30 Hz scheduler base clock. Spatial 32x32-cell chunks choose source-solve cadence from their **minimum distance to every supplied simulation-interest source**:

- 0–25 m: 30 Hz;
- 25–50 m: 20 Hz;
- 50–100 m: 6 Hz;
- 100–200 m: 2 Hz;
- beyond 200 m: 0.5 Hz background persistence.

The multi-source rule is a union of influence regions. Heritage never computes an average or midpoint source.

Slow chunks receive a deterministic spatial phase offset so 6 Hz, 2 Hz and 0.5 Hz work is distributed across base ticks rather than synchronizing into periodic CPU spikes. Scheduling phase is separate from physical elapsed time. When a chunk runs, rainfall, drainage, infiltration, evaporation and flow consume the actual elapsed interval since that chunk's previous update.

The existing 27-colour neighbour-safe flow partition remains authoritative. Cadence-due source cells are filtered into those same colours. Water transferred into a slower neighbour is applied immediately. The conservative apply pass is restricted to due source cells and their immediate neighbours instead of traversing the complete field.

Runtime simulation interest is vehicle/player based, not render-camera based. The current single-player runtime supplies `Player Vehicle Root`; the API accepts multiple future sources for split screen/server relevance.

## Consequences

- Near-player hydroplaning/puddle evolution retains 30 Hz authority.
- Distant persistent water becomes much cheaper without lowering spatial resolution.
- Two split-screen players create two local high-rate islands and no artificial midpoint island.
- Work spikes from synchronized low-frequency regions are reduced by deterministic temporal staggering.
- Background water remains at 0.5 Hz rather than full sleep until a later lazy catch-up design can guarantee globally correct rainfall/drainage/runoff after wake-up.
- Visual water remains a separate GPU presentation concern; render LOD/camera count does not change authoritative rainfall mass.


## PERF11 revision

PERF11 narrows the high-rate radii to the ladder above after live profiling showed CPU frame-time pressure while the GPU retained substantial headroom. The nearest-interest-source union rule, deterministic chunk phase staggering, elapsed-time integration and 0.5 m authoritative spatial resolution are unchanged. Explicit water presentation is now bounded to 200 m and is governed separately by ADR-096.
