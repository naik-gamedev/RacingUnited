# CLOUDURP15AD – Object-Occluded Screen-Space Sun Shafts

## Summary
Extends the existing solar flare / god-ray work with a real screen-space object-occluded shaft pass.

## What changed
- the cloud present pass now samples the copied scene depth buffer;
- screen-space sun shafts are radially accumulated from the Sun position in screen space;
- visible scene geometry blocks the shafts, including buildings, terrain, alpha-tested foliage and impostors, so long as they write depth;
- existing procedural solar flare remains;
- previous Sun tint / Moon tuning remains.

## Notes
This is still a screen-space solution, so it only knows about what is visible in the current frame. It is nevertheless a major step up from the earlier sky-only shafts and pairs well with dense impostor vegetation because the cost depends on screen resolution, not raw instance count.
