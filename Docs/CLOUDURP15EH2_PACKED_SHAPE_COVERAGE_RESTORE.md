# CLOUDURP15EH2 — Packed Shape Coverage Restore

CLOUDURP15EH introduced a packed RGBA 128^3 shape volume, but replacing 65% of the established R-channel body field with reconstructed fBm changed the distribution entering the nonlinear cloud coverage remap enough to erase clouds at runtime.

EH2 keeps the original R channel authoritative and blends only 12% reconstructed packed-octave detail. This retains the intended de-terracing benefit while preserving established cloud occupancy and coverage.
