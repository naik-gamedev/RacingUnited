# CLOUDURP15H7 — visible-linear cloud coverage

This pass removes the H6 98..100% endpoint closure and fixes the mismatch between regional cloud occupancy and actual volumetric cloud-body occupancy.

- 0% remains clear.
- 1% selects sparse real cloud regions and those selected regions now form robust visible bodies.
- Regional occupancy is no longer multiplied back down by moisture after selection.
- Selected cloud regions use a continuous 3.75..4.75 formation strength across the authored slider.
- Secondary macro cloud-body blend is continuous 0.70..1.00 across the slider.
- No special 98..100% overcast fill remains in raymarch or cloud-shadow shaders.
- Regional weather texture resolution is 512x512 across the existing 2000 km span to preserve sparse low-coverage cloud cells more reliably.
- H6B module-owned startup weather remains intact.
