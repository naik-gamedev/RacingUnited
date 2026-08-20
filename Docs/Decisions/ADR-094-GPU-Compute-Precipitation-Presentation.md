# ADR-094: GPU Compute Precipitation Presentation

## Status
Accepted for WEATHER07B7.

## Context
WEATHER07B6 proved that scientifically sized textured rain can look convincing when the visible representative population is increased by roughly three orders of magnitude. The diagnostic rendered approximately 4.6 million precipitation candidates per frame through a CPU loop and dynamic VBO. The visual result was strong, but runtime fell to about 1 FPS.

The project no longer targets legacy graphics hardware for this subsystem. Racing United's current Windows renderer requests an OpenGL 4.6 core context, so high-density rain may depend on compute shaders and shader-storage buffers.

## Decision
High-density near/mid precipitation presentation is generated on the GPU.

- OpenGL 4.6 compute shaders generate deterministic camera-relative rain records into an SSBO.
- The CPU submits only aggregate weather state, camera/world-cell origins, rain-population parameters and dispatch sizes. It does not iterate individual drops.
- WEATHER07A remains authoritative for rainfall mass, drop-size distribution, terminal-speed law, wind and precipitation time. GPU representatives are visual/statistical samples of that authority; they do not change hydrology water mass.
- Physical liquid diameters remain 0.20-6.00 mm in normal rain presentation.
- Streak length remains velocity integrated over an optical exposure interval. Sub-pixel raster support is area compensated rather than interpreted as physically enlarged water.
- Storm presentation may use up to 4,194,304 textured GPU representatives. Density is reduced dynamically for lighter rain.
- The GPU population is split into a dense near tier and a cheaper mid-distance tier. Individual textured drops fade before approximately 100 m.
- Beyond the resolvable individual-drop range, world-space precipitation curtains provide atmospheric rain visibility out to roughly 1 km. Distant rain is treated as extinction/shaft structure rather than millions of individually resolved millimetre drops.
- The WEATHER07B6 1000x CPU fallback is diagnostic only and is removed from normal execution.

## Synchronization
The compute stage writes rain records through an SSBO. Before the vertex stage reads that buffer, Heritage issues `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT)` as required by the OpenGL memory model.

## Performance intent
The purpose is not to render every real drop. It is to preserve the WEATHER07B6 visual density while moving population generation and record updates from millions of CPU operations to massively parallel GPU work. F8 diagnostics expose compute instance count and dispatch count for live profiling.

## Follow-up
- profile GPU time and choose final storm representative budget;
- add view-frustum/occlusion compaction if fixed instance submission remains too expensive;
- add resolved scene depth/colour for robust shelter occlusion and rain refraction;
- couple impact events to wet-material/puddle presentation;
- evolve far precipitation into cloud-driven rain shafts/fronts.
