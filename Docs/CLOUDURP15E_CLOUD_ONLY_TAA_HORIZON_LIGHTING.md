# CLOUDURP15E — cloud-only TAA, farther horizon range, lighter thickness

This overlay changes the volumetric cloud integration to temporal-filter **only the clouds**. The upscaled full-resolution cloud buffer now stores cloud RGB scattering and cloud transmittance only; the temporal history stores that cloud-only result; the present pass composites the stabilized cloud layer over the current-frame scene.

Additional tuning:
- global cloud max ray distance: 200 km -> 350 km
- cloud density multiplier: 0.32 -> 0.28
- extinction range: 0.04..0.12 -> 0.03..0.09
- retains CLOUDURP15D denser overcast field
