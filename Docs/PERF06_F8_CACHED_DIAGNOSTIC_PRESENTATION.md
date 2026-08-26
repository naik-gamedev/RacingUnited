# PERF06 - F8 cached diagnostic presentation

PERF06 keeps the performance measurements added by PERF01 through PERF04 but removes the profiler UI itself from the hot path as much as practical.

## Runtime policy

- FPS/frame time, rolling lows, GPU frame/pass timers, CPU breakdown, pacing state and frame graph remain presented every rendered frame.
- All underlying engine/renderer/Dynamic Surface/cloud/shadow measurements continue to be collected at their original cadence.
- The long detailed diagnostic report is formatted at 10 Hz and cached between refreshes.
- The cached report is submitted to Dear ImGui as one large unwrapped text item. Dear ImGui's large-text path can skip lines outside the visible scrolling region instead of processing more than one hundred individual Text widgets every frame.
- No glFinish, readback, fence wait, simulation, renderer, shader or quality behavior is changed.

## Why

After PERF05 removed the repeated GL_LINK_STATUS queries, F8 itself became the dominant CPU cost in the measured scene (roughly 23 ms in the captured sample). The detailed information is valuable and is retained; only its presentation cadence is decoupled from the render-frame cadence.
