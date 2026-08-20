# DSURF02 — Heritage Dynamic Surface GPU Persistent Page Pool

Status: **implemented foundation**

DSURF02 turns the logical 4096 x 4096 Dynamic Surface domain from DSURF00/01 into a bounded software virtual-texture system. It does **not** migrate water, rubber, marbles, dirt or temperature yet; that begins in DSURF03+.

## Spatial/storage contract

- Each 100 x 100 m surface sheet still owns a logical 4096 x 4096 domain (~2.44 cm/texel).
- The logical domain is divided into 16 x 16 pages.
- Each physical page is 256 x 256 texels and covers 6.25 x 6.25 m.
- Page identity is `(FP64 chunk X/Z, surface-sheet ID, page X/Z)` and is independent of camera position or rendering LOD.
- There is no 4096 x 4096 CPU image allocation.

## Typed physical page storage

A resident page has three GPU texture-array layers sharing the same physical slot:

- Hydro: `RGBA16F` — free water, moisture, flow X, flow Z.
- Track: `RGBA16F` — temperature, adhered rubber, loose-rubber mass, marble maturity.
- Contamination: `RGBA8_UNORM` — dirt, mud, debris, reserved contamination.

Each physical page owns a normal 256 -> 1 mip chain. Distance rendering therefore samples lower mips of the **same persistent page**, rather than rebuilding camera-relative water/rubber shapes.

## Budget and residency

The initial software budget is 96 MiB. At the current three-plane format + full mip chain this permits 57 physical page slots. The budget is explicit and reported in F8 diagnostics.

`DynamicSurfacePagePool` owns virtual-to-physical assignments and uses clean-page LRU replacement:

- dirty pages are never silently evicted;
- pinned pages are never evicted;
- re-requesting the same virtual page preserves its physical slot/generation;
- a replacement receives a new generation so the renderer can initialize the recycled GPU layer safely;
- freshly allocated pages begin clean/default; simulation writers explicitly mark only the planes they change as dirty.

Later milestones may expose user/platform-specific budgets and independent per-plane residency without changing virtual page identity.

## GPU mirror

`DynamicSurfaceGpuPagePool` allocates three `GL_TEXTURE_2D_ARRAY` stores and mirrors CPU residency through an SSBO page table. A second SSBO carries dirty-page work to future compute simulation passes.

New/recycled physical layers are initialized on GPU by a compute shader:

- Hydro = dry / no flow;
- Track = 20 C / no rubber / no marbles;
- Contamination = clean.

Mipmaps are generated from the persistent page. DSURF03+ can replace full-array mip regeneration with dirty-layer compute mips when page updates become frequent.

## Renderer integration

`EntityMeshRenderer` owns only the OpenGL mirror. Lifecycle/synchronization is isolated in `EntityMeshDynamicSurface.cpp` so the root renderer remains below its orchestration-size guard. `SurfaceWorld::DynamicSurfaceSystem` remains the world authority for page identity/residency.

Every render frame performs a cheap generation check. Page-table uploads occur only when virtual residency changes. With DSURF02 alone there are normally zero resident pages because no dynamic state has migrated yet.

F8 now reports:

- GPU pool readiness;
- resident / capacity / dirty page counts;
- committed VRAM budget;
- page-table generation/upload count;
- initialized/recycled pages;
- mip regeneration count;
- synchronization CPU time.

## Regression guarantees

The native DSURF02 regression proves:

1. stable virtual page identity;
2. a strict physical-page budget;
3. least-recently-used eviction of only clean unpinned pages;
4. dirty-page eviction refusal;
5. pinned-page eviction refusal;
6. generation changes when a physical slot is recycled.

## Next milestone

**DSURF03 — water + moisture migration** moves conserved hydrology into these persistent Hydro pages, connects tire clearing/drainage/evaporation/flow updates to dirty page work, makes the ordinary material shader sample these world-anchored pages, and retires the WATER15-18 camera-relative puddle state once parity is reached.
