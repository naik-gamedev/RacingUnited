# Live tire deformation checklist

Use this after every tire-deformation build. A build is not considered successful until the live game visibly passes.

## A. Render-path proof
- [ ] Debug deformation toggle produces an unmistakable forced deformation on the actual player tire.
- [ ] The deformation disappears when the toggle is disabled.

## B. Flat road
- [ ] Stationary loaded tire has visible lower tread flattening.
- [ ] Rim/bead stays correctly positioned.
- [ ] Tire does not visually sink through road collider.

## C. Longitudinal sidewalk edge under tread center
- [ ] Center of tread indents.
- [ ] Both sides of tire remain distinct; this is not whole-ring translation.
- [ ] Neighboring tread/shoulder vertices deform smoothly.
- [ ] No spikes/explosions.

## D. Lower-front curb/wall
- [ ] Lower-front tread/shoulder visibly compresses.
- [ ] Tire does not simply clip through collider.
- [ ] Deformation follows actual collider shape/normal.

## E. Performance sanity
- [ ] No obvious frame-time explosion on ordinary flat road.
- [ ] Detailed path activates only near relevant complex geometry once correctness is established.
