# PERF05 — Stutter-Free Development I/O and Exact Capture

## Runtime rule
Heritage must not recursively walk Scripts/Assets or poll mesh/texture filesystem timestamps on the gameplay/render thread at a fixed cadence. Regular filesystem polling can create visible frametime spikes even when average FPS is high.

## Authoring refresh
- F5: explicit authoring refresh. Refreshes the module asset index, reloads Lua, and advances the renderer hot-reload epoch. A hitch while deliberately pressing F5 is acceptable.
- Initial module asset discovery still occurs once shortly after startup.
- Module.RefreshAssetIndex() remains available for explicit Lua-side refresh.

## Screenshot capture
- F12: Heritage-native exact-frame clipboard capture from the final OpenGL backbuffer.
- PrintScreen is intentionally left to Windows and is not trusted for exact engine-frame debugging because the compositor can provide an older OpenGL presentation on some systems.
- Workflow: focus Heritage, press F12, switch to ChatGPT/Discord/etc., Ctrl+V.

## Profiling
Use the PERF04 frametime graph, 1% low, 0.1% low, P99/P99.9 and worst-frame readings to verify that rhythmic hitches have disappeared.
