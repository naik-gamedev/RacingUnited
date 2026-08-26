# CELESTIAL03 — Lunar Aureole and Receiver Cloud Shadows

CELESTIAL03 responds to visual validation of CELESTIAL02. It keeps one VCLOUD01 density field and one 256x256 celestial cloud-shadow cookie.

## Moonlit cloud appearance

The generic dual-HG cloud phase remains intact. Lunar transport additionally receives a narrow g=0.90 forward-scattering lobe, which is the real water-droplet behavior responsible for the bright aureole and luminous cloud structure around the Moon. Higher-order interior fill begins at lower density so thin cloud near the lunar disc participates without globally whitening the night deck.

## Ground receivers

The detailed 3D cloud-shadow trace remains authoritative. A low-frequency floor sampled from the same regional weather field prevents the coarse 15-sample shadow trace from returning clear-sky transmission when it happens to thread a thin erosion gap inside a cloud cell.

Entity materials now apply the resulting transmission to direct Sun/Moon lighting and a weaker fraction of diffuse sky/IBL. This matches real cloud shadows: the direct celestial beam is strongly removed, while hemispherical skylight remains and shifts slightly cooler.

No second weather simulation, cloud field, Sun shadow map, or Moon shadow map is introduced.
