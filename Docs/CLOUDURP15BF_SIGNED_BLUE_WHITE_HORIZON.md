# CLOUDURP15BF – Signed Blue/White Horizon

The lower sky gradient now uses signed view-direction Y rather than a clamped 0..1 height.

## Exact tuning reference
- `blueToWhiteStart = +0.010` — blue ends / grey-white fade begins.
- `fullyWhiteHeight = -0.500` — grey-white target is fully reached.
- transition span = `0.510`.

For future tuning: make `blueToWhiteStart` smaller to push the blue endpoint lower; make `fullyWhiteHeight` more negative to lengthen the fade farther downward.
