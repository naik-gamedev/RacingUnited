# ADR-055: Reusable Tire Parts and Bounded Performance-Bias Authoring

## Status
Accepted as roadmap architecture.

## Context
Heritage needs many tire products across cars, motorcycles, trucks, karts, ATVs and possible future
three-wheel vehicles. Requiring creators to hand-author hundreds of low-level tire-model coefficients for
every part is impractical, while reducing tires to direct surface-grip multipliers would undermine the
physical tire model. Tires are also reusable parts that may be fitted to more than one vehicle, and even
nominal road vehicles can encounter snow, ice, mud, sand or gravel.

## Decision
Represent a tire as a reusable part definition with identity, dimensions, engineering metadata,
provenance/confidence and optional measured/fitted tire-model data. A Vehicle Workshop references tire
parts per axle/wheel rather than duplicating their engineering definition.

When complete measured/fitted data is unavailable, the Tire Part Lab generates a physically reasonable
baseline from dimensions, operating load/pressure, construction and tread metadata. Tire size does not
directly multiply friction; it influences the physical mechanisms that determine contact behavior.

Expose simple creator-facing biases around that baseline for **Dry**, **Wet**, **Snow/Ice**, **Mud**,
**Sand**, **Gravel** and **Wear/Endurance**. These controls are available to every tire family rather than
being gated behind an "off-road" tire class. They are bounded/versioned residual-calibration inputs that
adjust coherent underlying model parameters; they are not post-solver final-force multipliers. Advanced
authoring can separate snow from ice and expose detailed studs/sipes, mud self-cleaning, sand flotation,
gravel/hardpack, pressure/temperature and other family-specific parameters where justified.

Manufacturer/model names never infer performance automatically. Differences between named products come
from authored/measured/fitted/calibrated data.

Track rubbering and loose tire marbles remain a specialized rubber subsystem. Shared spatial state may
store deposited and loose-rubber concentration, while marble visualization/migration and optional sparse
high-detail debris have dedicated ownership.

## Consequences
- Creators can add plausible tires quickly without pretending generated defaults are measured data.
- Serious native tire physics remains authoritative.
- The same tire part can be reused or swapped across compatible vehicles.
- Simple dry/wet/winter/mud/sand/gravel/endurance controls remain approachable while advanced
  calibration is still possible.
- Brand stereotypes are not encoded into physics.
- Tire marbles can be visually special without requiring thousands of persistent rigid bodies.
