# ADR-047: Tire Engineering Metadata and Surface Authoring

## Status
Accepted.

## Context
Racing United will contain multiple accurately modeled tires and scenes with asphalt, kerbs, grass, gravel, dirt, snow, ice and later deformable terrain. Runtime render meshes may be low-poly assets with baked high-poly tread normals, while physics still needs stable engineering data and precise local surface identity.

## Decision
Separate tire presentation geometry from tire engineering metadata. `.tir` / Heritage metadata is authoritative for measured, fitted or explicitly estimated tire mechanisms such as tread depth/void ratio, water drainage, compound behavior, siping, winter tread interlock, studs and future loose-surface self-cleaning/aggressiveness. These values feed physical mechanisms instead of becoming a list of direct dry/wet/snow grip multipliers.

Author scenes with one local `SurfaceMaterial` identity that survives import into render, collision/surface lookup, audio, particles and weather. Blender vertex/material/custom-property painting is the preferred source workflow. Collision simplification must not erase precise asphalt/kerb/grass/gravel boundaries; tire footprint samples query local surface identity/mixture independently of render triangle density.

Layer a future spatial `SurfaceField` over static `SurfaceMaterial` identity. The field owns dynamic local water depth, temperature, rubber/debris, loose-layer depth, snow depth/packing, moisture/mud state, compaction, rut depth and shear history.

A future authoring tool may analyze sufficiently accurate tread geometry to estimate void ratio, groove area, tread depth or siping as a starting point, but geometry-derived estimates remain optional and carry provenance/confidence. A simple runtime tire mesh must still be able to use serious physics data.

## Consequences
- High-to-low baked tire workflows do not reduce simulation fidelity.
- Visual, physics, audio, particles and weather share one surface identity/state source.
- Tire engineering data remains portable across render LODs and replacement meshes.
- Dynamic weather/terrain state gains a clear future ownership boundary.
