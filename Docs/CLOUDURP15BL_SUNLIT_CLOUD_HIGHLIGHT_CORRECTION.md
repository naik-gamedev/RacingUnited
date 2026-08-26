# CLOUDURP15BL — Sunlit Cloud Highlight Correction

This pass keeps the BK Sun performance overhaul intact, but corrects the remaining noon cloud overexposure. The direct cloud Sun gain is reduced, the extra noon/morning solar amplification is trimmed, and the daytime cloud highlight shoulder is strengthened so sun-facing cloud lobes retain shape instead of collapsing into flat white.

## Adjustments

- `cloudSunGain`: `0.42..0.36` → `0.38..0.32`
- `noonIllumination`: `1.00..1.08` → `1.00..1.04`
- morning Sun lift: `1.00..1.03` → `1.00..1.015`
- cloud daytime highlight shoulder: `0.24 * cloudDay` → `0.32 * cloudDay`

## Preserved

- BK Sun/cloud-shadow performance savings
- BK1 validation compatibility
- BH horizon gradient calibration
- BI mid-day cloud de-cartoonization
- BJ more reasonable distance-cloud LOD
