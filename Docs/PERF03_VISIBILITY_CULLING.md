# PERF03 — Visibility Culling Foundation

## Goal

Heritage should refuse render work that cannot contribute to the current view.
This is the first scalable visibility layer for large Racing United worlds.

## Implemented

- Every imported mesh draw range receives a conservative local bounding sphere once at asset upload time.
- Every camera/render pass extracts a six-plane frustum from the actual projection * view matrix.
  This works with the 100 km reversed-Z projection and asymmetric/triple-monitor projections.
- Non-skinned draw ranges outside the frustum are rejected before material texture binding, shader-per-draw state, skin setup, and `glDrawElements`.
- Skinned ranges are deliberately conservative and remain visible until deformation-aware bounds exist.
- Collision/spawn authoring ranges cache their hidden/render decision once when the asset is loaded rather than repeatedly walking GLB metadata/name hierarchies each frame.
- The renderer reuses a scratch MeshInstance vector, removing a fresh per-frame instance-array allocation.
- F8 performance diagnostics now report visible/candidate ranges plus frustum-culled ranges and triangles.

## Why this matters

A 100 km visual horizon must not mean that every object inside 100 km reaches the GPU. Terrain, buildings, vegetation and infrastructure outside the current camera frustum should cost essentially no draw submission.

This is intentionally CPU-side and conservative. It is a foundation for later systems:

1. chunk / cell visibility and streaming;
2. static render-batch compilation;
3. LOD / HLOD;
4. Hi-Z occlusion culling for objects hidden behind hills/buildings;
5. GPU-driven indirect rendering and instancing;
6. vegetation cluster / whole-tree impostor visibility.

## Correctness policy

Culling is allowed to draw too much, but it must never hide visible authored content. Therefore uncertain cases (currently skinned ranges) are not culled.
