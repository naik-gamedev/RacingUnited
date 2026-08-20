# WATER13B – Adaptive Hydrology Overlay Mesh

## Goal
Remove the misleading fixed square near-field hydrology/debug overlay and make the engineering view follow the same adaptive settled-water surface strategy as the actual visible water.

## What changed
- All explicit-water presentation rings now gather **adaptive coarse hydrology source patches** rather than forcing a full-resolution 0.5 m chessboard in the near field.
- Near rings may still stay dense where slope, support, wetness or flow require it, but flat/coherent regions can merge into larger support patches.
- Far rings may merge up to **20 m** support patches.
- The settled-water surface skin now refines between **0.10 m and 20.0 m** edge lengths.
- The debug/engineering overlay no longer draws one cyan quad per collected patch. It now reuses the **actual adaptive triangular water skin** so the overlay represents the surface mesh that is being rendered.
- Flow arrows are preserved, but the filled debug surface is triangular rather than a square checkerboard.
- The requested nanometre clearance experiment remains preserved: the explicit water skin still uses the reversed-Z polygon offset path plus the 1 nm lift already requested in WATER13A.

## Notes
- Authoritative hydrology is still the existing **0.5 m virtual-pipe field**. This change adapts the source/support and debug/presentation mesh, not the underlying conserved water mass grid.
- This is the cheapest step toward what was requested: a variable-density mesh that can be visually dense where needed and very coarse where not, without pretending that a fixed chessboard is the water surface.
