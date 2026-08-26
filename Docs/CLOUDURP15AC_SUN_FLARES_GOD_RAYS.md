# CLOUDURP15AC – Sun Flares and God Rays

## Summary
Adds lightweight procedural solar flares and sky-space god rays to the astronomical/atmospheric sky path.

## What changed
- Sun now gets a procedural multi-spike starburst flare around the solar disc;
- flare stays local to the Sun and does not require any authored lens texture;
- broad crepuscular god rays are added in sky space;
- god rays are strongest with low Sun plus partial cloud/haze conditions;
- earlier sunset solar tint tuning and Moon tuning remain intact.

## Notes
This is deliberately a cheap implementation. It is not a full screen-space lens flare stack and not true geometry-occluded volumetric shafts. Instead it gives the desired visual read at much lower cost and without disturbing the current weather/sky ownership model.
