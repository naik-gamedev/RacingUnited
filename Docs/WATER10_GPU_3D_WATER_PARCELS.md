# WATER10 — GPU 3D Water Parcels

WATER10 replaces the visible near-field 0–25 m puddle mesh with a bounded true-3D GPU parcel simulation. WATER09 virtual-pipe hydrology remains authoritative for water mass, tires, weather and multiplayer-relevant surface state.

## Why

The previous scalar/connected presentation could still expose the 0.5 m/adaptive rectangular support lattice. WATER10 deliberately separates the simulation lattice from the final visible liquid. A parcel has an XYZ position, XYZ velocity, radius and represented volume. The renderer never draws hydrology-cell quads in the near field.

## Low-cost architecture

- 32,768 fixed GPU parcel slots; inactive slots consume storage but no raster work after GPU compaction.
- 20 Hz GPU simulation tick with previous/current position interpolation at render rate.
- 10 Hz CPU support-field refresh from authoritative SurfaceHydrology; roughly 64 x 64 m around the view at 0.5 m/texel and up to two stacked surface layers.
- Fixed 96 x 16 x 96 occupancy grid. Parcels atomically accumulate occupancy; a cheap density gradient gives pressure-like spreading without particle-neighbour sorting or iterative PBF constraints.
- No GPU-to-CPU active-particle readback. Compute compacts active parcel indices and writes the indirect draw instance count.
- Half-resolution screen-space fluid rendering. Particles are sphere impostors only while generating depth/thickness; a two-pass bilateral depth filter reconstructs one smooth visible fluid surface.
- The final fullscreen liquid pass writes reconstructed reversed-Z depth into the scene, so rain/particles behind the water are depth-occluded normally.

## Relationship to WATER09

The global virtual-pipe height field is retained. It remains far cheaper for kilometres of track and remains the physics authority. Near-field parcels are generated/recycled from that field and provide the true-3D presentation: gravity, free vertical motion, surface collision, downslope motion and pressure-like parcel spreading.

This is intentionally a hybrid. It avoids paying true-3D particle cost for every wet square kilometre while removing the square near-field visual representation.

## First calibration

- Particle cap: 32,768
- Parcel simulation: 20 Hz
- Support field refresh: 10 Hz
- Support field: 128 x 128 at 0.5 m/texel (64 x 64 m)
- Screen-space fluid buffers: half render resolution
- Depth smoothing: one separable bilateral horizontal/vertical pair
- Explicit/adaptive water mesh: skipped in the first 0–25 m presentation ring

Future work should only increase neighbour fidelity (full PBF/XPBD, adaptive merge/split, tire parcel impulses) if the measured visual benefit justifies the GPU cost.

## Research basis and cost choices

This first tier deliberately does **not** run a full PBF neighbourhood solver. Position Based Fluids uses iterative density constraints over particle neighbourhoods; that is a good higher-quality option, but neighbour construction plus several solver iterations is exactly the work WATER10 avoids initially.

The architecture follows the cost lesson from Chentanez, Müller and Kim's large-scale hybrid liquid work: use a cheap height-field representation for the broad domain and spend 3D particle/grid work only in a bounded region where three-dimensional detail is visible. WATER09 therefore stays global/authoritative while WATER10 is a local 3D representation.

The visible surface follows the screen-space particle-fluid approach of van der Laan, Green and Sainz: render particle sphere depth/thickness, filter the visible depth field, then shade that reconstructed surface instead of polygonizing the particle volume. WATER10 uses half-resolution buffers and only one horizontal/vertical bilateral pair as its initial low-cost quality setting.

The 20 Hz parcel tick and 10 Hz support refresh are Heritage engineering budget choices, not constants prescribed by those papers. Render-rate interpolation hides the 20 Hz stepping. If later profiling shows headroom, fast splash events can use adaptive substeps without increasing the base rate for slow runoff.

References:
- N. Chentanez, M. Müller, T.-Y. Kim, *Coupling 3D Eulerian, Height Field and Particle Methods for the Simulation of Large Scale Liquid Phenomena*, SCA 2014.
- M. Macklin, M. Müller, *Position Based Fluids*, SIGGRAPH 2013.
- W. J. van der Laan, S. Green, M. Sainz, *Screen Space Fluid Rendering with Curvature Flow*, I3D 2009.
