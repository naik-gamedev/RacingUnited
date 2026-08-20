# ADR-106 — Virtual-Pipe Shallow-Water Authority

**Status:** Accepted

## Context

The earlier Heritage hydrology stored spatial water depth but transported mobile water by calculating a downhill weight distribution and releasing a bounded fraction of the source depth each update. That approach was inexpensive, but flow had no persistent flux/momentum state and was difficult to reason about across the engine's 30/20/6/2/0.5 Hz distance cadences.

The renderer is also being decoupled from the 0.5 m simulation lattice. A stable scalar height/depth authority is therefore more valuable than presentation-driven water cards.

## Decision

Heritage uses a layered 2.5D shallow-water height field with persistent virtual pipes as the authoritative surface-water transport model.

Each hydrology cell stores water depth and four N/E/S/W volumetric pipe outflows. Static pipe sill offsets/elevations are derived from baked neighboring terrain elevations. Hydraulic-head differences update pipe flux; roughness damps flux; the total outflow is conservatively limited to mobile source volume. Open boundaries use a separate bounded outfall pipe.

The CPU solver remains authoritative for deterministic gameplay/network physics. GPU systems may mirror/consume the scalar field for rendering and presentation, but may not own water mass.

## Consequences

- Rain, drainage, infiltration, evaporation and tire displacement continue using the existing SurfaceHydrology API.
- Curbs and terrain steps behave as hydraulic spill barriers through baked sill elevations.
- Flow retains short-term momentum instead of being recomputed as a stateless release fraction.
- Distance-adaptive cadence remains possible because outflow is volume-limited for arbitrary elapsed `dt`.
- The simulation grid is not a rendering topology. Wet-film and puddle presentation remain independent of cell rectangles.
- Literal particle/molecular fluid simulation is reserved for transient spray/splash presentation, not authoritative track-scale water mass.
