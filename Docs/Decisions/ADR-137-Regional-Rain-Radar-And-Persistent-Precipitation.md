# ADR-137 — World-space regional rain radar and persistent precipitation

## Decision

Heritage Engine will treat precipitation as a world-space regional field rather than a camera/global scalar only.

1. Keep a sparse coarse regional precipitation authority at 250 m cells.
2. Make regional rain deterministic and wind-advected so the radar and visible rain share one storm field.
3. Store cumulative precipitation in millimetres per regional cell.
4. Lazily catch dormant cells up from a timestamped weather-history timeline when they are first queried.
5. Expose the field through an F10 radar with current-rate and accumulated-rain views.
6. Feed camera-local rain presentation and near-field GPU hydrology from the regional rate.
7. Use accumulated regional precipitation only as a conservative promotion seed until a dedicated coarse background-runoff solver is introduced.
8. Keep expensive 512x512/10 m hydrology near actual simulation-interest sources; never simulate an artificial midpoint between distant local players.

## G8M implementation correction

The logical 441-visible-tile work cap must not imply a 441-layer permanent WaterState scratch allocation. G8M uses a 32-layer recycled scratch array and consumes up to 441 due visible tiles through in-frame sub-batches. This preserves the requested logical work cap while avoiding the G8L startup VRAM spike.
