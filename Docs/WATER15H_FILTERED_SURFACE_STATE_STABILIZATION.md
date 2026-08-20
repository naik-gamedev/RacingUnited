# WATER15H — Filtered Surface-State Stabilization

WATER15G proved that the compact clipmap sizes are practical, but its material fast path exposed adaptive-control-volume discontinuities directly to the optical water normal and also computed thin-film depth from the sampled support rather than the exact authored receiver fragment. The visible result was a repeated rectangular/dashed reflection pattern and road water leaking onto curb-height surfaces.

WATER15H keeps the authoritative adaptive hydrology and the compact 1024/512/256 clipmaps, but separates raw simulation projection from presentation sampling:

1. Raw top/lower hydraulic-head + support state is GPU-rasterized exactly as before.
2. A support-aware compute pass runs only when that clipmap refreshes. It averages **water depth only** across neighbours whose support elevations are near-coplanar. The center support elevation is never blurred.
3. The normal scene material samples the filtered state with two texture reads and computes local water depth as `head - exact authored fragment height`.
4. Near/mid support matching is deliberately strict, so a normal road curb is rejected rather than borrowing the road state.
5. Optical water normals no longer use screen derivatives of the adaptive hydrology head. Standing water relaxes the authored normal toward gravity-up and receives only small procedural ripples once real pooling exists.
6. Shallow rain remains primarily a roughness/darkening response. Environment reflection is deliberately bounded until a proper SSR path exists.

The compute filter is presentation-only. It cannot alter hydrology volume, flow, drainage, evaporation, tire clearing, or solver cadence.
