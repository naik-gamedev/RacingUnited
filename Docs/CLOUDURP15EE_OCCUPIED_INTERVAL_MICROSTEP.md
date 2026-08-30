# CLOUDURP15EE — Occupied Interval Microstep

CLOUDURP15EE addresses the residual fixed-step banding / "slices of ham" artifact still visible after CLOUDURP15ED.

Rather than only increasing blur or freezing history, the cloud marcher now resolves each occupied primary march interval as four shorter substeps. This increases integration fidelity inside actual cloud density, which directly smooths the layered appearance on tall cloud towers.

The full-resolution 64-step path from CLOUDURP15ED remains in place. The 7x7 transmittance-aware reconstruction filter and the single RGB+coverage temporal history path remain authoritative, with slightly stronger but still moderate temporal accumulation to calm residual shimmer.
