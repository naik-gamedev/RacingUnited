# CLOUDURP15EA — Extreme cloud denoiser

This milestone intentionally pushes the existing single-path volumetric-cloud denoiser to an extreme setting.

- Current cloud upscale/denoise kernel: **11×11 Gaussian** (previously 7×7).
- Current-frame cloud blend is strongly biased to the denoised neighbourhood.
- Temporal history: **0.9995 coherent**, **0.9998 mild stochastic**, **0.99995 strong stochastic**.
- Stochastic classifier engages earlier.
- The existing 5-pixel current-frame AABB clamp, reprojection bounds and clear-sky rejection remain the anti-ghosting authority.

The requested “9,999,999×” is treated as an intentionally extreme practical denoiser rather than a literal temporal multiplier: the cloud history target is RGBA16F, so weights arbitrarily closer to 1.0 cease to produce useful sub-quantum updates and effectively freeze the history.
