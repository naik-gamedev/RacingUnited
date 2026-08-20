# WATER15I — Film / Puddle Decoupling

WATER15H proved that even a filtered high-resolution atlas still exposed adaptive hydrology control-volume ownership when sub-millimetre/millimetre rain film directly changed PBR roughness. WATER15I separates two physically different visual states.

* **Thin rain film / dampness** comes from the persistent `SurfaceWeather` film state and is spatially smooth. It changes the existing material's roughness and darkness without sampling adaptive-cell depth.
* **Standing puddles** are local hydrology depth in excess of the smooth weather film. Only cells more than 1.5 mm above the baseline are gathered into the hydraulic-head clipmaps.

The visible scene is still drawn once. There is no generated water mesh, duplicate scene pass, seam welding, quadtree presentation geometry, polygon bias, or collider-normal offset.

The puddle clipmaps are 512²/128 m, 256²/512 m and 128²/2000 m, refreshed at 10/3/0.5 Hz with at most one refresh per frame. Ordinary uniform rain therefore performs zero atlas rebuild work.

The shoreline breakup mask may erode only excess-puddle edges. It cannot add water or change authoritative hydrology volume.
