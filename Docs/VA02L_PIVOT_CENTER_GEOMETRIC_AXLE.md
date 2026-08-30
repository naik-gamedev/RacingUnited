# VA02L — Pivot-centred geometric wheel spin

VA02K correctly stopped trusting the authored Pivot **basis** for wheel-spin direction, but it also used the measured tire AABB centre as the centre of the spin line. That can make the tire itself look stable while a rim whose true hub centre differs by even a small radial amount visibly orbits/precesses around the tire-derived centre.

VA02L splits the two authorities:

- `WH_*_Pivot` authored origin = mechanical hub / spin-line centre.
- measured `WH_*_Tire` geometry axis = spin-line direction.

The runtime logs `tire_vs_pivot_radial_mm` once per wheel so mirrored-copy centre discrepancies are visible without changing simulation physics. Suspension/upright motion still transports the line rigidly and wheel spin remains a mesh-global axis-angle transform.
