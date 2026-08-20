# PERF14 — Adaptive Contour Water Tessellation

## Problem

PERF12/PERF13 proved adaptive 0.5/1/2/4/8/16 m water presentation and seamless world-space shading, but every surviving presentation patch was still rendered as one rigid fitted plane. On curved terrain a large patch could therefore bridge a crown, gutter, hillside transition, or other non-planar collision surface and visibly stick out of the world as a tall transparent "water sail".

## Architecture

The authoritative hydrology remains unchanged: 0.5 m cells store surface elevation, normal, water depth and flow. PERF14 only changes presentation meshing.

The adaptive hierarchy now uses an error-bounded quadtree tessellator:

1. A base candidate tests all authoritative 0.5 m source-cell centres **and all four corners of every source footprint** against the fitted presentation plane.
2. Every accepted node stores its maximum surface-plane residual.
3. A parent candidate tests the four corners of each child footprint against the parent plane and adds the child's inherited residual. This provides a conservative contour-error bound for all descendant hydrology samples instead of checking only child centres.
4. If the error exceeds the patch-size tolerance, the parent is rejected and the renderer naturally falls back to its smaller children; if a base node fails it falls back to 0.5 m source cells.
5. Very steep surfaces additionally cap maximum presentation patch size. This is presentation-only and does not alter runoff/hydrology simulation.

Flat parking lots therefore remain cheap 8/16 m patches, while crowns, ditches, gutters, curved hillsides and collision-mesh breaks automatically tessellate toward 4/2/1/0.5 m as needed.

## Collision-contour support heights

Each presentation record now carries four support elevations for the exact world-X/Z patch corners:

- (-X,-Z)
- (+X,-Z)
- (-X,+Z)
- (+X,+Z)

The GPU record stores these as centre-relative Y offsets. The water vertex shader uses those supports directly instead of extrapolating a single averaged normal all the way to every patch corner. The fixed WATER05C surface lift is applied after this support, so the liquid remains just above the collision contour.

This is intentionally not hardware tessellation. Large planar regions still cost two triangles. Curved regions are subdivided by the CPU presentation quadtree only when the authoritative collision surface contains information worth representing. This avoids adding tessellation-shader cost to millions of flat-water pixels while achieving the requested adaptive contour behavior.

## Continuity and material

PERF13 world-X/Z patch edges, world-space ripple coordinates, transmittance-preserving LOD blending, thin-film/pool optics and the 0–200 m cadence-ring policy remain intact. Geometry LOD therefore changes cost without restarting water texture/ripple phase.

The cyan hydrology debug overlay now consumes the same corner supports so engineering visualization follows the collision contour too.

## Regression contract

The native surface regression keeps both sides of the policy:

- a flat 32×32 m wet surface must still produce at least an 8 m adaptive patch;
- a shallow quadratic collision surface with gradually changing normals must subdivide to at most 4 m patches, proving that accumulated contour deviation cannot be bridged by an apparently-compatible large plane.
