# CLOUDURP15AJ – 2,000 km spherical far-cloud march

- Restores the original artificial spherical-Earth cloud-shell helper for local clouds.
- Removes the temporary flat local cloud slab introduced in CLOUDURP15AI.
- Keeps the 32-step / 250 m high-quality near cloud march unchanged.
- Extends the cheap sparse far-LOD search to the full 2,000,000 m sky-ray budget.
- Uses 48 cheap far-search samples rather than 28.
- Spherical curvature still determines what is physically visible: from ground level the Earth/cloud-shell horizon can end the valid cloud segment long before 2,000 km.
