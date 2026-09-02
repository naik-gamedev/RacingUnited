# SUSP12 — Five-Link Independent Suspension

SUSP12 adds `multilink_v1` as a native independent-suspension provider for modern road/race cars.

Five fixed-length chassis-to-upright links constrain one rigid wheel carrier. Requested wheel travel is the sixth constraint, so wheel-centre scrub, camber and toe migration emerge from the authored geometry rather than travel curves. The fifth link is the toe/steering link. Its chassis pickup can move along an authored steering-rack axis; a fixed rack therefore yields passive bump steer while steering input produces a physical rack displacement and constrained upright steer.

The spring and damper use separate chassis/upright mounting points. Their real shaft compression and instantaneous motion ratios are differentiated from the solved rigid-upright pose and feed the existing virtual-work suspension force contract. SUSP05 remains the only bridge from solved wheel-centre motion to the 1 kHz tire-support query.

The provider requires 17 named hardpoints: four ordinary link inner/outer pairs, toe-link inner/outer, wheel center, spring upper/lower, damper upper/lower, and steering-rack axis start/end.
