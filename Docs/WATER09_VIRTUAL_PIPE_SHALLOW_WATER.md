# WATER09 — Virtual-Pipe Shallow-Water Height Field

WATER09 replaces Heritage's former stateless downhill water-release step with a persistent virtual-pipe shallow-water transport model while retaining the existing world bake, weather, tires, distance cadence and water presentation interfaces.

## Authoritative state

Each baked 0.5 m hydrology cell remains the authoritative local water-depth sample. The terrain bake continues to provide elevation, material, infiltration, drainage, roughness, depression storage, layered topology and precipitation exposure. WATER09 additionally derives four static hydraulic sill offsets/heights for the north/east/south/west edges.

Each cell owns four persistent virtual-pipe volumetric outflows in m^3/s plus one open-world boundary outflow. The solver computes hydraulic head as `ground elevation + water depth`. Head differences accelerate or decelerate pipe flux; surface roughness damps the stored flux. The pipe cross-section is the cell-edge width multiplied by the water depth that actually rises above the baked sill elevation, so raised curbs/steps do not pass water until the source surface reaches their spill height.

## Conservation and stability

For every source update, candidate pipe fluxes are integrated first. The total requested outflow volume over the cell's actual distance-cadence `dt` is then compared with the source's mobile water volume (water above depression storage). All outgoing pipe fluxes are scaled by one conservative factor if required. A cell therefore cannot export more water than it owns, including at 2 Hz and 0.5 Hz cadence bands.

The existing deterministic 27-colour parallel schedule is retained. It guarantees that simultaneously processed source cells do not write overlapping source/neighbor depth-delta slots, so no atomics or locks are required in the CPU authoritative solver.

## Distance cadence

The simulation-interest policy is unchanged:

- 0–25 m: 30 Hz
- 25–50 m: 20 Hz
- 50–100 m: 6 Hz
- 100–200 m: 2 Hz
- beyond 200 m: 0.5 Hz persistence

Each cadence update uses the actual elapsed physical time since that chunk last ran. Virtual-pipe flux limiting makes long `dt` updates mass-safe instead of moving an arbitrary fixed fraction of water.

## Tires and rendering

Tire contact continues to read/write the same authoritative water depth and flow velocity. WATER09 does not create a separate visual water authority.

WATER08 presentation remains in place for this milestone: screen-space shallow wet film, continuous scalar puddle contours at 0–50 m, and adaptive standing-water presentation farther out. Because transport is now a true persistent height-field flow, future GPU texture presentation can consume the same scalar depth/head field without changing tire or weather semantics.

## Diagnostics

F8 now reports active virtual-pipe count and peak virtual-pipe flux in litres per second. Lua hydrology stats expose `solver = virtual_pipe_shallow_water`, `active_virtual_pipes`, and `maximum_virtual_pipe_flux_lps`.
