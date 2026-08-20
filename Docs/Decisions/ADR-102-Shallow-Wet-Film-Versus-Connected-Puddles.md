# ADR-102 – Separate Shallow Wet Film from Connected Puddle Geometry

## Decision
Hydrology cells remain simulation data only. Shallow film and fast/steep runoff render directly on tagged authored surface geometry through an isolated wet-film overlay. Explicit connected water meshes are reserved for accumulated, basin-like water.

## Rationale
Live testing showed that no amount of alpha breakup can fully hide polygon topology when shallow water itself is represented by separate geometry. Rendering film on the actual road/terrain guarantees contour conformity, while connected meshes remain appropriate for real standing-water free surfaces.

## Safety
Do not inject hydrology back into the universal entity material shader. WATER04C proved that such integration could corrupt scene rendering. PERF19 owns a separate shader/program, explicit tag opt-in, and independent texture/state bindings.


## Supersession
The separate wet-film overlay presentation described here is **superseded for shallow-film rendering by ADR-103** after live testing showed that the duplicate pass discarded authored normal-map detail. The simulation/puddle classification decision remains valid.
