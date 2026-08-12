# ADR-062 — Driven-Surface Presentation State

**Status:** Candidate with TIRE15B2; requires Windows build/launch/drive validation.

## Context

TIRE15/TIRE15B1 established authoritative world-owned deformable surface state, authored material mechanics and live wetness/temperature. Visible ruts, spray, dust and loose debris must reflect that state without creating a second simulation that can diverge from tire/collision physics.

## Decision

Heritage owns a bounded `SurfacePresentation` under `Physics/Surfaces/Presentation` as a strictly one-way consumer of authoritative wheel-contact and `SurfaceWorld` state.

- Persistent visual track marks are keyed from FP64 global positions so floating-origin rebases cannot duplicate/move an existing mark.
- Track marks visualize actual rut depth/displaced volume and are bounded to 8,192 active slots.
- Transient water spray, dust, mud, snow and loose-debris particles are bounded to 2,048 active slots and generated deterministically from contact speed/slip/load/wetness/material state.
- Presentation is advanced on simulation time, so pause/fixed-step behavior remains coherent.
- A dedicated renderer converts global positions back to camera-relative local coordinates, distance-culls them and uses bounded dynamic GPU buffers.
- Presentation never feeds tire forces, collision support, `SurfaceField`, or vehicle state.
- `Physics.GetSurfacePresentation()` exposes read-only normalized rolling/spray/dust/debris audio-mechanism intensities. Real sound assets remain authored content; this checkpoint does not synthesize placeholder sounds.
- Tire marbles/rubber remain a dedicated TIRE15C rubber subsystem. The generic driven-surface presentation path may later render rubber-owned data, but it does not own rubber physics.

## Consequences

Visual fidelity can improve independently without changing physics results, long tracks remain bounded, and floating-origin behavior is robust. Future rendering may replace the current ribbon/point representation with tessellation, decals, meshes, GPU particles or terrain displacement while keeping the same authoritative source contract.
