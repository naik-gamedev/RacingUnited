# WATER10A - Flat Parcel Surface Isolation

WATER10A fixes two presentation failures exposed by live testing.

1. The old connected/adaptive explicit-water mesh is fully disabled for presentation and no longer collected/tessellated. The WATER09 virtual-pipe field and WATER06 screen-space wet film remain authoritative/visible outside the 3D parcel region.
2. GPU parcel horizontal influence radius is separated from physical vertical water thickness. Shallow surface parcels are flat ellipsoidal reconstruction kernels measured in millimetres vertically, while retaining a broad horizontal splat for inexpensive merging. Only genuinely airborne parcels may grow vertically for future splash/spray.

This is an isolation milestone: no rectangular puddle geometry is allowed to coexist with the parcel renderer.
