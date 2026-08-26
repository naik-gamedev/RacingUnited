# ADR-139 — Code Health and Legacy Retirement Program

## Status

Proposed / roadmap authority, 2026-08-25. No runtime behavior change in this checkpoint.

## Context

The CLEAN01–CLEAN13 program successfully decomposed earlier engine monoliths, but subsequent tire, water, rubber, weather and cloud development created new concrete architecture blockers. A fresh project inspection found a 4,832-line hydrology file containing both current prebaked data and retired live-solver generations, a 1,568-line uncompiled TireCarcass implementation, 51 fake `.cpp` architecture scaffolds, an unreachable Cloud Lab panel, and several active renderer/runtime files again approaching dumping-ground size guards.

The existing CLEAN13 stop rule explicitly permits further architecture work when a concrete blocker is found. That condition is now satisfied.

## Decision

Adopt `Docs/CODE_HEALTH_OPTIMIZATION_ROADMAP_2026_08_25.md` as the active code-health roadmap. Work proceeds in independently buildable checkpoints. Proven dead code is deleted rather than refactored; current production authority is extracted before historical generations are removed; fake source scaffolds are replaced by architecture documentation; active files are split only along enduring responsibility boundaries; and performance changes require pass-level measurements rather than assumptions.

Validators are part of the architecture and must be updated when they accidentally fossilize retired implementation details. Behavioral regressions and public contracts remain protected.

## Consequences

- Historical CLEAN milestones remain documented but are no longer the active cleanup queue.
- Source inventory becomes more truthful: code files contain implementation.
- Current `.hhyd` prebaked hydrology can be separated from retired adaptive solvers.
- The current Dynamic Surface GPU authority will be renamed/split as production code.
- Future FPS investigations gain pass-level GPU timing boundaries.
- Large first-party Lua global state is migrated toward explicit Racing United namespaces.
