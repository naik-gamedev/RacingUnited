# CLOUDURP15ED — Refined Reconstruction Anti-Banding

CLOUDURP15ED addresses the two main residual artifacts from CLOUDURP15EC:

1. fixed-step banding / "slices of ham" visible on tall cloud bodies;
2. residual grain / shimmer at cloud edges.

The production-quality cloud renderer now uses 64 primary samples, full-resolution cloud targets, midpoint interval sampling, a second dense sub-sample inside occupied intervals, a 7x7 transmittance-aware reconstruction filter and slightly stronger moderate temporal reconstruction.

The single RGB+coverage temporal authority, point-history sampling and current-frame AABB clamp remain authoritative.
