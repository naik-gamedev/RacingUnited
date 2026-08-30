# CLOUDURP15EH — Packed fBm Shape Volume

The previous Heritage port stored only one R8 channel for the 128^3 base cloud shape. Repeated screenshots showed stable contour-like strata even after increasing primary samples, integrating occupied intervals in microsteps, de-axis sampling the erosion and shape volumes, and increasing the vertical LUT resolution.

CLOUDURP15EH replaces the base shape with a packed RGBA 128^3 tileable volume. R preserves the original base field; G/B/A carry decorrelated higher-frequency periodic octaves. The shader reconstructs a low-frequency fBm composite before density/coverage remapping. This restores the multi-frequency density representation expected by HDRP-style volumetric cloud shaping and greatly increases effective scalar precision at cloud boundaries.

The existing full-resolution 64-step raymarch, two occupied substeps, de-axis sampling, edge-aware spatial reconstruction and single RGB+coverage temporal history path remain.
