# ADR-069 — Poisson and PCSS shadow filtering

## Status
Accepted for SHADOW04.

## Decision
Heritage Engine exposes shadow filtering under a dedicated **Shadows** section in Video Settings. The available filtering modes are **Nearest**, **Poisson PCF**, and **PCSS + Poisson**. Shadow resolution remains a separate Low/Medium/High/Ultra setting (1024/2048/3072/4096 per CSM layer). Fresh defaults use Ultra/4096 with PCSS + Poisson.

The CSM depth array remains a single four-layer D32F texture. Two sampler objects view that same storage: a raw nearest-depth sampler and a GL_LINEAR comparison sampler using `GL_COMPARE_REF_TO_TEXTURE` / `GL_LEQUAL`. This allows PCSS blocker search to read raw depth while the final Poisson PCF stage uses proper hardware-filtered depth comparisons without duplicating shadow-map memory.

Poisson PCF uses a stable 16-sample disk rather than a rigid square kernel. PCSS + Poisson performs a compact raw-depth Poisson blocker search, estimates average blocker depth, derives a clamped penumbra from blocker/receiver separation, then scales the final Poisson PCF disk. The Poisson pattern is deterministic (stable per cascade) rather than temporally randomized, avoiding intentional frame-to-frame shadow shimmer until Heritage has a dedicated temporal shadow filter.

## Performance policy
Nearest is the cheapest diagnostic/low-cost mode. Poisson PCF is the normal smooth fixed-radius mode. PCSS + Poisson is the highest-quality mode and intentionally spends more GPU texture samples, while SHADOW02 keeps CPU submission low through layered cascades and contiguous range batching.
