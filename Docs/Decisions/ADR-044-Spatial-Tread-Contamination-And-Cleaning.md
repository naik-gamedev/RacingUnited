# ADR-044 - Spatial tread contamination, material pickup and self-cleaning

## Status

Accepted - TIRE11 candidate.

## Context

TIRE08 already gives every tire a 16 circumferential x 3 lateral tread field, while TIRE10 makes
that field geometrically relevant to rolling radius. Surface contamination must therefore remain
material-fixed and spatial: a tire that clips grass with one shoulder should not instantly become
uniformly dirty, and contamination should rotate through the contact patch and clean progressively.
A global `dirtyTirePercent` would discard the state already available and would make later water,
rubber pickup and terrain transfer much harder to compose.

The normal high-fidelity path must also preserve the project's one-MF6.2-evaluation-per-tire
performance contract. Forty-eight tread cells are state/history, not forty-eight tire-force solvers.

## Decision

Promote `Vehicles/Tires/TireSurfaceInteraction.*` into a compiled clean-room material-transfer
mechanism. Each `TireTreadCellState` owns five independent normalized channels:

- organic/grass contamination;
- mineral dirt/dust;
- gravel fines;
- rubber pickup/marbles;
- mud-film groundwork.

Wear and contamination share one exported `tireTreadContactWeights` function so circumferential and
lateral contact ownership cannot drift between mechanisms. Grass, dirt and gravel collision
materials expose bounded pickup compositions, and TIRE06 supported-sample material fractions are
blended so edge contact is visible before the centre ray crosses a boundary; surface wetness can add mud-film availability. A
separate `surfaceRubberDebrisFraction` input is reserved for dynamic-track rubber/marbles without
requiring a new collision material. Snow and ice transfer are deferred to TIRE13.

Pickup acts on the currently contacting weighted cells. On clean hard surfaces the currently
contacting material-fixed sectors self-clean progressively; forward speed, contact slip and hot
tread increase scrubbing/release. Material-specific retention controls relative persistence.

The aggregate active-contact state feeds the existing tire pipeline through three bounded outputs:

- contact friction scale before the single MF6.2 evaluation;
- rolling-resistance scale;
- tread-to-road heat-transfer scale for TIRE07.

No extra MF solve is introduced. The current `[HERITAGE_CONTAMINATION]` prototype coefficients are
explicitly synthetic development values, not measured historical Pirelli data and not proprietary
third-party tire-model coefficients.

## Consequences

- Grass/dirt/gravel excursions can continue affecting grip after the tire returns to asphalt.
- Tire rotation naturally moves contaminated sectors into and out of the footprint.
- Clean asphalt progressively restores the tread instead of resetting contamination instantly.
- Wet loose surfaces can deposit mud-film groundwork before full TIRE15 terramechanics exists.
- Dynamic rubber/marbles can later feed the same tread state independently from collision material.
- TIRE12 can add spatial water without replacing the contamination architecture.
- Large grids retain one MF6.2 force/moment evaluation per tire in the normal fidelity tier.
