# ADR-065: Master LOD Transitions And Presentation Precision

## Status
Accepted for Heritage Engine.

## Context
Hard distance cutoffs and hard representation swaps create visible popping. The
problem is generic: tire marks, rubber/marbles, particles, vegetation and future
mesh/terrain LOD systems all need the same visual continuity rule. Separately,
bounded presentation effects do not require FP64 GPU vertex coordinates even
when their persistent world state benefits from FP64 addressing.

## Decision
Heritage Engine owns one reusable master LOD transition policy in
`Graphics/LodTransitionPolicy.hpp`.

- Final visibility boundaries fade continuously to zero before culling.
- Neighboring LODs receive complementary smooth transition weights through a
  bounded blend band.
- Renderers choose the least expensive correct realization: geometry/material
  morphing for translucent overlays, dither/crossfade for opaque assets, or
  dual representation only inside the transition band when required.
- Asset/subsystem code should not invent unrelated hard-pop thresholds when the
  master transition policy can express the same boundary.

Heritage also owns a presentation precision helper in
`Graphics/PresentationPrecision.hpp`.

- Persistent/authoritative large-world state may remain FP64 or another
  precision-preserving storage format.
- Bounded visible presentation is rebuilt each frame from FP64 world position
  minus FP64 camera origin, then cast once to camera-relative FP32 for GPU use.
- Tire marks, marbles/rubber and transient particles follow this rule. They are
  not repeatedly rebased/stored in FP16 or player-relative low precision.

## Initial integrations
- Tire marks morph from detailed pressure-resolved presentation into far uniform
  strips after 200 m and fade to zero at 500 m.
- Settled marbles crossfade near/far representation around their detailed LOD
  boundary and fade at their outer draw range.
- Moving rubber, surface particles and driven-surface marks fade at their draw
  boundaries.
- Vegetation exposes a representation blend query using the master policy so
  future vegetation rendering can crossfade/dither without changing authoring.

## Performance
The policy itself is allocation-free scalar arithmetic. Expensive dual drawing
is not mandatory. Translucent systems should morph when possible. Opaque asset
renderers may use dither/crossfade only inside the narrow transition band.
