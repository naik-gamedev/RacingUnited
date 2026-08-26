# CLOUDURP15E2 — Reactive cloud-only TAA

The first cloud-only TAA pass over-trusted clear-sky history. Because alpha is cloud transmittance, a history pixel near 1.0 could suppress a newly opaque cloud pixel for many frames.

CLOUDURP15E2 derives a reactive mask from the difference between current and reprojected cloud opacity (`1 - transmittance`). Stable interiors retain 0.97 accumulation. Emerging/disappearing cloud structure rapidly reduces the history weight, preventing clouds from vanishing or smearing while keeping temporal cleanup in stable regions.
