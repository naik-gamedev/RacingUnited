# Heritage Engine Multicore and CPU-Scaling Roadmap

## Purpose

Heritage must scale from low-end 2-core/4-thread machines through the intended
8-core desktop/console-class target and onward to larger CPUs without changing
the authoritative simulation rules. Multithreading is not a licence to run
unnecessary work: sleeping, multi-rate updates, spatial interest management,
data locality and GPU presentation remain equally important.

JOB01 establishes the shared engine worker pool and moves surface hydrology onto
it as the first deterministic production workload. JOB02 then attacks the next
live bottleneck exposed by that migration: water presentation was rebuilding,
aggregating and uploading tens of thousands of records at render rate. It
deliberately does not scatter subsystem-owned `std::thread` instances through
the engine.

## JOB01 baseline

`Core/Jobs/JobSystem` is a process-wide bounded worker pool owned by
`EngineRuntimeState`. Its first public primitive is a synchronous `parallelFor`:

- persistent workers are created once rather than per update;
- the calling/main thread participates in each batch;
- work is divided into bounded contiguous ranges and dynamically claimed;
- range indices are stable so each worker can write private reduction data and
  the caller can merge results in deterministic range order;
- nested use on the same JobSystem executes inline, preventing worker-pool
  starvation and recursive waits;
- worker exceptions are returned to the calling thread;
- idle waiting uses condition variables rather than spin loops.

The default worker count is hardware-aware. It intentionally does not occupy
every logical processor with worker threads because the caller itself performs
work and the renderer/driver/audio/OS still need scheduling headroom. The exact
count is a starting policy and must remain profile-driven rather than a promise
that one topology is optimal for every CPU.

### JOB01A live concurrency correction

The first JOB01 live rain test exposed an intermittent `ParallelBatch` lifetime
race that small regressions had not triggered reliably. The waiting caller could
observe the final participant count reaching zero and destroy the stack-owned
condition variable while the last worker was still executing `notify_one()`.
JOB01A makes the decrement/notification a lifetime handshake protected by the
batch completion mutex. ThreadSanitizer reproduced the original race and no
longer reports it with the corrected path. This fix is part of the job-system
baseline before any additional subsystem is migrated.

## First production migration: hydrology

The 30 Hz surface-water solver is the first parallel workload because its large
cell arrays are naturally batchable and were a measured CPU concern. JOB01
parallelizes precipitation/losses, depth application and statistics directly.
The flow phase requires special handling because one source cell contributes to
neighbour cells.

Hydrology therefore partitions sources into 27 spatial colours using `(x mod 3,
z mod 3, layer mod 3)`. Cells of one colour have non-overlapping one-cell
neighbourhoods, so those sources can update the shared depth-delta field in
parallel without atomics or mutexes. Colours execute in a fixed order and
per-range runoff reductions are merged in stable order. This preserves the
existing reduced-order water model instead of replacing it with a nondeterministic
parallel approximation.

## Scaling policy

The long-term CPU strategy is hierarchical:

1. **Do not schedule work that can sleep.** Dry water cells, settled fields,
   inactive effects and distant secondary state should cost close to zero.
2. **Run each mechanism only as fast as its physics requires.** High-rate tire
   contact may remain hundreds of Hz or higher where justified; ordinary
   downhill water can stay around 30 Hz; slow drainage/weather fields can be
   lower; presentation is interpolated at render rate.
3. **Batch by contiguous data.** Hot tire, water, surface and traffic loops
   should move toward cache-friendly arrays/batches before merely adding more
   threads.
4. **Use the shared worker pool.** New heavy subsystems submit bounded jobs;
   they do not permanently claim a CPU core or create private pools by default.
5. **Synchronize at explicit phase boundaries.** Parallelism inside a physics
   phase is preferred over hidden cross-phase races.
6. **Keep per-job writes private whenever possible.** Merge deterministic
   reductions after a barrier instead of putting mutexes in hot loops.
7. **Move presentation work to the GPU.** CPU authority should not imply CPU
   simulation of every visible ripple, raindrop, marble or deformation vertex.

## Planned migration order

The order below is intentionally profile-gated. A subsystem moves only when its
measurements justify the complexity.

### JOB02 — water presentation cache, instancing and profiling

The first live JOB01 capture proved worker execution was active, but also exposed
water presentation inside render submission as the dominant CPU path when a large
wet field was visible. JOB02 therefore optimizes the renderer before migrating more
physics work:

- PERF12 retains cadence-aligned persistent presentation caches, while splitting the 50-100 m 6 Hz region into two phased radial slices and 100-200 m 2 Hz into four to distribute collection/upload spikes.
  A 30 Hz near refresh still does not force the 20/6/2 Hz regions to be reuploaded;
- cached water positions use an FP64 presentation origin with FP32 local records,
  so camera motion only updates one origin uniform until a ring leaves its safety margin;
- each presentation slice owns persistent VBO storage and only that slice is orphaned/uploaded when due;
- water quads use instanced triangle strips generated from `gl_VertexID`; the old
  geometry-shader expansion stage remains removed for older/iGPU efficiency;
- water uniform locations are cached at initialization rather than queried from
  the OpenGL driver by name every rendered view;
- adaptive planar presentation now covers the entire 0–200 m explicit-water field.
  The 0.5 m authoritative cells are only the fallback/source tessellation; compatible
  regions may merge through 1/2/4/8/16 m patches, capped at 8 m inside 50 m and
  16 m beyond. Puddle edges, curbs, material boundaries and non-planar support stay fine;
- presentation slices cross-fade per fragment; the final water geometry fades completely
  by 200 m. There is no special 100 m fine/coarse switch;
- F8 reports total/fine/coarse record counts plus 1/2/4/8/16 m patch populations,
  largest patch, refreshes, collect/pack-upload/draw CPU time and hydrology timing.

World-anchored ripple animation, physical flow direction, environment reflection,
water depth and tire interaction remain intact. JOB02 is a presentation optimization,
not a lower-fidelity water physics mode.

### JOB03 — distance-adaptive multi-source hydrology cadence — IMPLEMENTED CANDIDATE

The authoritative water field no longer performs expensive weather/flow source
work for every baked cell at 30 Hz. Spatial chunks are classified from the
**minimum distance to any simulation-interest source**. Multiple local players
therefore create the union of their own local high-rate regions; their positions
are never averaged into a point between them.

Current cadence ladder:

- 0–25 m: **30 Hz**;
- 25–50 m: **20 Hz**;
- 50–100 m: **6 Hz**;
- 100–200 m: **2 Hz**;
- beyond 200 m: **0.5 Hz background persistence** for now.

The slow bands are deterministically phase-staggered per spatial chunk, so a
large 2 Hz region does not all wake on the same render frame. Each chunk still
uses its actual elapsed physical time when it runs; the phase offset changes
*scheduling only* and does not invent or discard rainfall/drainage time. Incoming
water from a faster neighbouring chunk is applied immediately even if the target
chunk's own source solve is currently at a slower cadence.

JOB03 also limits the conservative depth-apply pass to cadence-touched source
cells and their immediate neighbours rather than scanning the complete baked
field for pending flow. The 30 Hz base clock remains the scheduler's maximum
cadence and all existing 27-colour lock-free flow safety remains intact.

Future work can add true sleep/dirty wake-up for dry or fully settled background
chunks once lazy catch-up rules preserve rain, drainage and runoff correctly.

### JOB04 — vehicle and tire parallel phases

- Batch independent vehicles/wheels within explicit fixed-step phases.
- Keep inter-vehicle collision/contact resolution behind clear barriers.
- Cache persistent tire/road contact candidates before increasing thread count.
- Avoid per-substep heap allocation and introduce per-worker scratch storage
  where profiling proves allocation/cache pressure.

### JOB05 — data-oriented/SIMD hot loops

- Evaluate structure-of-arrays layouts for large wheel/tire/thermal batches.
- Vectorize numerically independent work where results can remain equivalent.
- Measure cache misses and memory bandwidth before and after layout changes.

### JOB06 — wider world systems

- Migrate suitable traffic, vegetation, rubber/marble, weather and streaming
  jobs to the same pool.
- Use temporal staggering so low-frequency background systems do not all wake on
  one frame.
- Keep networking/server simulation deterministic and authoritative.

## Hardware targets

The primary full-detail development target is an 8-core CPU, but no subsystem
may assume eight cores exist. A 2-core/4-thread machine should automatically use
a small pool and more aggressive sleeping/LOD; a 16- or 32-thread machine should
be able to consume additional work without changing gameplay outcomes.

The low-end goal is not to lower local driving physics into an arcade model.
Scale presentation, distant secondary simulation, inactive-region cadence and
background work first.

## Performance rules for future AIs/contributors

- Profile before and after every scheduling migration.
- A lower total CPU time with worse frame-time spikes is not automatically a win.
- Never parallelize a loop with shared writes until ownership/race behavior is
  proven explicitly.
- Do not make floating-point result order nondeterministic merely to gain a
  small benchmark improvement in authoritative simulation.
- Do not spawn one OS thread per car, tire, water chunk or subsystem.
- Do not busy-wait workers while the main thread sleeps.
- Keep a serial/single-worker path naturally valid for debugging and low-core
  hardware.
- Windows/MSVC live profiling is the final performance gate; portable synthetic
  benchmarks are evidence, not a shipping-performance claim.
