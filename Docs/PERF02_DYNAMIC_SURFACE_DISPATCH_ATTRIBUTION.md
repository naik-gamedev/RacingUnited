# PERF02 — Dynamic Surface Dispatch Attribution

PERF02 is an attribution-only profiler pass for the production `DynamicSurfaceGpuRuntime`.

It exists to explain the measured ~20 ms CPU wall time inside the Dynamic Surface update without changing water behavior, physics, rendering quality, topology cadence, or GPU synchronization policy.

F8 now reports:

- top-level Dynamic Surface CPU stages: bookkeeping, tire-water readback polling, residency, lazy state provisioning, geometry binds, async GPU-timer submission, optional snow/mud dispatch, tire-event dispatch, tire-water sample dispatch, and residual time;
- near topology CPU build/raster cost and near-atlas / tile-indirection GL upload wall time;
- far 500 m topology total CPU time, candidate generation, candidate sort, missing-tile scan, tile resolve/decompression/copy, far-atlas upload, and far-tag upload;
- tire-clearing GL setup, uniforms, total/slowest `glDispatchCompute`, and barrier wall time;
- tire-water sample polling `glClientWaitSync(..., timeout=0)`, map and unmap wall time;
- tire-water sample upload/setup/dispatch/barrier/fence wall time;
- optional snow/mud dispatch/copy/barrier wall time.

All timings use CPU `steady_clock` around work that already existed. PERF02 adds no `glFinish`, no blocking query, no positive-timeout fence wait, and no full-atlas readback.

The intended next step is to capture F8 while the ~20 ms cost is present and optimize only the identified owner.
