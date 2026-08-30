# CLOUDURP15E9 — 2x temporal cloud denoiser

CLOUDURP15E9 doubles CLOUDURP15E8 temporal smoothing by halving the current-frame contribution at each existing accumulation tier, rather than adding another blur or TAA pass.

- coherent cloud: history `0.970 -> 0.985` (current 3.0% -> 1.5%)
- mild stochastic grain: history `0.992 -> 0.996` (current 0.8% -> 0.4%)
- strong stochastic grain: history `0.9985 -> 0.99925` (current 0.15% -> 0.075%)

The five-pixel current-frame AABB history clamp, point history sampling, reprojection bounds, clear-neighbourhood rejection and stochastic classifier remain unchanged. This is still one denoiser authority.
