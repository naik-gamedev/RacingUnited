# CLOUDURP15BE – Lower Blue Horizon Reference

This pass keeps the longer daytime sky gradient, but lowers the blue-horizon band so it sits noticeably closer to the terrain line.

## Numeric reference
- Previous lower-band threshold: `0.640`
- New lower-band threshold: `0.360`
- Absolute change: `-0.280` normalized sky height
- Relative change: about `43.75%` lower than the previous setting

This value can be used as a tuning reference for future passes. Increasing the threshold pushes the blue horizon higher; decreasing it pulls the blue horizon lower.
