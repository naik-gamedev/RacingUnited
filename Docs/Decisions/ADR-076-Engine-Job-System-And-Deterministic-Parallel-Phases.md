# ADR-076 — Engine Job System and Deterministic Parallel Phases

**Status:** Accepted for JOB01  
**Date:** 2026-08-15

## Context

Heritage's expensive native simulation has historically been main-thread
dominated. Surface hydrology is already a large 30 Hz cell solver and future
large-grid vehicles, tires, traffic, weather and world systems require a common
way to consume multi-core CPUs. Creating a private thread/pool for each subsystem
would oversubscribe low-core machines, complicate shutdown and make phase
ownership difficult to reason about.

Authoritative simulation also requires predictable data ownership. A naive
parallel conversion of hydrology flow would race because source cells add water
to neighbouring destination accumulators.

## Decision

Heritage owns one process-wide `Core/Jobs/JobSystem` from `EngineRuntimeState`.
The initial API is a synchronous bounded `parallelFor`; the calling thread
participates and explicit call return is the phase barrier. Workers persist for
the process lifetime. Same-system nested calls execute inline.

Automatic worker count is based on reported logical processors and deliberately
leaves scheduling headroom rather than creating one saturated worker for every
hardware thread. Subsystems do not assume a fixed core count.

Parallel authoritative phases use deterministic ownership patterns. Reductions
write per-range scratch and merge in stable range order. Hydrology's shared
neighbour-write flow phase uses a 27-colour `(x,z,layer) mod 3` partition: one
colour executes at a time, while all source cells within that colour have
non-overlapping one-cell neighbourhoods and can run without atomics or locks.

The JobSystem is engine infrastructure, not Racing United gameplay code.
Subsystem-specific threading remains disallowed unless a later ADR establishes a
clear reason (for example a blocking I/O service with different lifetime needs).

## Consequences

### Positive

- One reusable worker pool scales across water, vehicles and future systems.
- No worker creation/destruction in simulation hot loops.
- Main thread contributes useful work instead of immediately blocking.
- Low-core systems naturally retain a serial/small-pool path.
- Deterministic per-range reductions avoid mutex-heavy hot loops.
- F8 can expose actual worker/range activity for live verification.

### Costs / constraints

- Not every loop is safe to parallelize; shared writes need explicit ownership.
- Synchronous batches still have scheduling/barrier overhead, so small workloads
  should stay inline through suitable grain sizes.
- Floating-point operations that are order-dependent must preserve a defined
  merge order or provide equivalence evidence.
- More systems are not migrated merely because the JobSystem exists; profiling
  decides the next migration.

## Validation

JOB01 adds a native regression for exact-once range execution, deterministic
range reduction, nested-call safety and worker-count/stat sanity. Surface
hydrology is compared against the serial path in portable testing and remains
covered by pooling, tire-clearing and large wet-workload regressions. Windows
MSVC plus live F8 profiling remains the final platform/performance gate.
