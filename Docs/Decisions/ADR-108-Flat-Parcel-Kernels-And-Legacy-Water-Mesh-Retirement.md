# ADR-108: Flat Parcel Kernels and Legacy Water-Mesh Retirement

## Decision

During the WATER10 parcel cutover, Heritage does not render or tessellate the previous explicit connected/adaptive puddle mesh. The WATER09 virtual-pipe field remains authoritative, screen-space wet film remains the fallback outside the parcel region, and near visible liquid is reconstructed from GPU parcels.

A parcel has separate horizontal influence radius and vertical physical half-thickness. Shallow surface water may use a broad horizontal reconstruction kernel for cheap overlap while remaining millimetres thick vertically. Only airborne parcels may become volumetric.

Half-resolution invalid fluid depth (zero) is never hardware-bilinearly mixed with valid eye depth during composite; valid samples are manually gathered and renormalized. The parcel renderer snapshots/restores depth, blend, cull, framebuffer, viewport, program, VAO, indirect-buffer and active-texture state.

## Reason

Live WATER10 testing exposed three artifacts: residual square puddle geometry, shallow parcels reconstructed as giant sphere marbles, and renderer-state leakage that made later surface effects opaque/depth-writing. WATER10A removes those failure modes before adding further fluid complexity.
