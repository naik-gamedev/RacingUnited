# ADR-124: Decouple Thin Film from Puddle Hydrology Presentation

**Status:** Accepted for WATER15I.

## Decision

Do not use adaptive hydrology control-volume depth to drive ordinary wet-material roughness. Use the persistent world `SurfaceWeather` film as the smooth thin-film state. Use hydraulic-head clipmaps only for spatial depth that exceeds the world film by at least 1.5 mm.

## Rationale

Adaptive control volumes are appropriate simulation authority but their ownership boundaries are not a visual parameterization. Filtering those boundaries cannot reliably make a uniform rain film look continuous and also wastes large GPU/CPU budgets. Thin-film and standing-water optics are different regimes and should be represented separately.

## Consequences

Ordinary rain remains smooth and cheap. Local tire-cleared thin-film detail will require a dedicated moisture/drying-line field later rather than abusing puddle depth. True puddles retain exact-surface hydraulic-head reconstruction and curb isolation.
