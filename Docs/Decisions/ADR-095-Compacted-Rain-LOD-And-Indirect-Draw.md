# ADR-095: Compacted rain LOD and GPU indirect drawing

## Status
Accepted for WEATHER07C1.

## Context
WEATHER07B7 proved that multi-million representative rain can look convincing on modern OpenGL hardware, but submitting roughly 4.2 million textured instances every frame wastes compute, vertex, fragment, and SSBO bandwidth on rain that is off-screen or visually redundant at distance. The project no longer targets legacy GPU support for this weather path.

## Decision
Use an OpenGL 4.6 GPU-driven precipitation presentation pipeline:

1. Compute generates deterministic WEATHER07A physical rain candidates.
2. Candidates are distributed through distance tiers rather than uniformly:
   - approximately 0-10 m: very high density;
   - approximately 10-35 m: strongly reduced density;
   - approximately 35-90 m: sparse textured streaks;
   - beyond that: world-space volumetric precipitation/atmospheric extinction.
3. The compute shader rejects candidates outside the tier radius and outside an expanded view frustum.
4. Surviving records are compacted with an atomic counter into a contiguous SSBO.
5. The atomic counter is the `instanceCount` field of a `DrawArraysIndirectCommand` buffer.
6. `glDrawArraysIndirect(GL_TRIANGLES, ...)` consumes exactly the compacted population without CPU readback.
7. Each streak is one UV-unwrapped triangle (three vertices), not a rectangular two-triangle quad.
8. Airborne rain uses only the tiny `RainDrop_BC.png` base-colour/alpha texture plus procedural/environment reflection. Normal and thickness maps are no longer required for this fast-moving material.
9. Racing United's current rain texture is reduced from 64x128 to 16x32 texels while preserving the authored alpha silhouette.

## Consequences
- Candidate compute work at maximum storm intensity falls from roughly 4.2 million to roughly 2.1 million before visibility rejection.
- The graphics pipeline sees only compacted, in-frustum representatives.
- Near-camera density can remain extremely high while distant per-drop cost falls rapidly.
- No synchronous visible-count readback is allowed; diagnostics report candidate submissions while the exact compacted count remains GPU-owned.
- Far rainfall remains spatially visible through the existing kilometre-scale precipitation volume instead of requiring microscopic textured particles at extreme distance.

### Modern-only cleanup
The obsolete CPU rain fallback shader/VBO and unused legacy rain-cover texture are removed from `WeatherPresentationRenderer`. The distant precipitation pass owns an independent `gl_VertexID` fullscreen triangle, so it cannot accidentally depend on the three-vertex streak mesh.

## WEATHER07C4 calibration update
The current Racing United visual calibration supersedes the older approximate/overlapping tier example above while retaining the same GPU-compaction architecture:

- 200,000 GPU rain candidates own 0-1 m.
- 300,000 GPU rain candidates own 1-10 m.
- 10,000 GPU rain candidates own 10-100 m.
- The world-space water-curtain / ray-marched presentation owns 100-1000 m.

The temporary WEATHER07C3A no-cull diagnostic is removed. Compute-stage radial and expanded-frustum compaction is restored, while the old vertex-stage 78-112 m fade is deliberately not restored because it would silently shorten the calibrated 10-100 m tier. The three particle bands are therefore defined in one place by compute radial limits, and the distant shader begins its world-space samples at 100 m.

## WEATHER07C5 calibration update
The WEATHER07C4 visual population calibration is superseded by a lighter near-field distribution while preserving the same GPU-compaction architecture and the same 100-1000 m distant curtain:

- 10,000 GPU rain candidates own 0-2 m.
- 300,000 GPU rain candidates own 2-10 m.
- 10,000 GPU rain candidates own 10-100 m.
- The world-space water-curtain / ray-marched presentation remains 100-1000 m.

The near tier cell footprint is widened from the prior 0-1 m calibration so its generated world-space population can actually cover the full 2 m radial band before compute culling. No hidden vertex-distance fade is reintroduced. Total candidate submission is therefore 320,000 per frame before radial/frustum compaction.


## WEATHER07C6 density-test update

The visual distance bands remain unchanged from WEATHER07C5, but the dominant 2-10 m presentation population is reduced for a direct visual/performance comparison:

- 10,000 GPU rain candidates own 0-2 m.
- 100,000 GPU rain candidates own 2-10 m.
- 10,000 GPU rain candidates own 10-100 m.
- The world-space water-curtain representation remains 100-1000 m.

No cell footprints, radial handoffs, culling rules, optical tier weights, microphysics, or curtain sampling distances are changed. Total particle candidate submission is 120,000 per frame before radial/frustum compaction.
