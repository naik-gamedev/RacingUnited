# WATER10B - Continuous Fluid Reconstruction

WATER10B corrects the fundamental presentation error in WATER10/WATER10A: particle splats were visible because the depth filter refused to reconstruct through empty pixels. Parcels are now hidden sampling primitives only. Four sub-cell candidates per hydrology texel provide coverage, settled parcels contribute a flat physical free-surface depth, and a two-iteration half-resolution gap-closing bilateral reconstruction produces the displayed liquid surface. The old explicit puddle mesh remains retired.

This is intentionally still a bounded near-field experiment. The authoritative global mass/flow model remains WATER09 virtual-pipe shallow water.
