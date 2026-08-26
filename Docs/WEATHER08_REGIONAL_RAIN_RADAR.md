# WEATHER08 — Regional Rain Radar and Persistent Precipitation History

## Goal

> **Implementation status correction (2026-08-22):** This document records the
> intended WEATHER08 architecture and an earlier prototype, not a fully
> integrated current feature. `WeatherRadarOverlay.cpp` exists, but it targets
> regional snapshot/query APIs that are not presently exposed by the compiled
> `PrecipitationField`. The complete regional authority, 24-hour evolution and
> production radar integration remain planned work. Continue from
> `WEATHER09_DYNAMIC_FORECAST_AND_RADAR_ROADMAP.md` and verify every claim
> against the current build before marking it complete.

Heritage weather must remain world-space across large free-roam maps. A location that has been outside the expensive centimetre Dynamic Surface working set for twenty minutes must still know how much rain fell there while the player was elsewhere.

## Regional authority

`PrecipitationField` now owns a coarse persistent regional rain field underneath the existing representative-raindrop system.

- regional storage cell: 250 m x 250 m
- broad storm coherence scale: about 2.4 km plus a smaller detail octave
- storm field is deterministic, world anchored, and advected by authored wind
- current regional rate is derived from the authored weather rainfall rate multiplied by the coherent storm field
- accumulated precipitation is stored per regional cell in millimetres
- untouched cells are lazily integrated from the weather-history timeline when first queried, so dormant world regions consume no frame-by-frame work
- weather-rate/wind edits are timestamped in a tiny history timeline so late-loaded regions do not assume the latest setting was active for the entire session

A 200 km^2 free-roam map would contain only about 3,200 250 m cells if fully materialized. The implementation is sparse, so it stores only cells that are actually queried by the radar or promotion path.

## F10 weather radar

F10 toggles the Heritage regional weather radar. The initial radar supports 5/10/20/40 km spans and two views:

- current regional rainfall rate (mm/h)
- accumulated regional precipitation (mm)

The radar is a view of the world precipitation authority; opening or closing it does not change weather simulation.

## Connection to rain and Dynamic Surface

Camera-local rain presentation and the near-field GPU Dynamic Surface now read the regional rain rate instead of the old single global rainfall scalar. Newly promoted 10 m WaterState tiles also receive a conservative dormant-region wetness seed derived from accumulated regional rainfall. This seed is not a replacement for future coarse background runoff; exact near-field runoff is still reconstructed by the live 512x512 solver.

## G8M startup/VRAM correction

G8L interpreted the requested 441-visible-tile update cap as a 441-layer 512x512 R32UI scratch allocation (~441 MiB). Combined with the ~3 GiB WaterState authority, compact presentation pools, shadows, rain buffers and normal rendering resources, this could push 6 GiB GPUs into allocation/driver failure during launch.

G8M keeps the logical maximum of 441 visible WaterState updates in one render frame but restores a recycled 32-layer scratch array (~32 MiB). Large due sets are consumed in 32-tile sub-batches during that same frame. The logical visible update cap therefore remains 441 without permanently reserving ~441 MiB of scratch VRAM.

`Tools/00_BuildAndRunCurrent.cmd` also launches the engine directly rather than through detached `START`, so an immediate runtime exit now leaves its process output and exit code in the build console.
