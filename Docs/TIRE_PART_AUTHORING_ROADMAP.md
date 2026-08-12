# Heritage Tire Part Authoring Roadmap

**TIRE17C implementation status (2026-08-11):** TIRE17A's reusable `TirePartDefinition`/specialty `TireFamily` baseline and TIRE17B's versioned `TirePerformanceBiasMapping` now feed a runtime `TirePartResolver`. A tire part carries engineering width/aspect/rim/load/reference-pressure data plus optional authoritative `.tir` provenance; the resolver uses explicit property data when present and otherwise generates the estimated family+bias model. `VehicleSystem` can assign the same reusable part per wheel while each fitment owns its own cold inflation pressure. Direct low-level tire-model edits clear reusable-part assignment identity rather than leaving stale metadata. Topology-aware axle/group assignment and the Parts Lab UI remain later TIRE17/TIRE18 work.

## Purpose

Tires are reusable vehicle parts, not merely values embedded inside one vehicle definition. Heritage
should eventually let a creator define a tire once, test/calibrate it in a Tire/Vehicle Parts Lab, and
reference that tire asset from cars, motorcycles, karts, trucks, ATVs, trikes and other compatible
vehicles.

The default workflow should be approachable without reducing the serious native tire model to arcade
grip multipliers. Geometry and engineering metadata establish a physically reasonable baseline; a compact
set of creator-facing surface-performance and endurance biases then nudges that baseline within bounded,
plausible ranges. Advanced users may expose measured/fitted parameters directly.

## Tire part identity

A tire part definition should carry stable identity and provenance independently of the vehicle using it:

- manufacturer / brand;
- product/model name;
- production/version year where relevant;
- tire size (section width, aspect ratio, rim diameter);
- load index / maximum rated load;
- speed rating;
- construction type and intended family (road performance, slick, wet race, winter, motorcycle,
  kart, truck/commercial, rally gravel, ATV/off-road, etc.);
- mass/inertia when measured, otherwise clearly marked estimated;
- inflation-pressure range / reference pressure;
- tread depth, void/drainage information, siping/stud information and compound metadata where known;
- provenance/confidence for every measured, manufacturer-sourced, inferred or hand-tuned property.

Brand/model names are metadata only. Heritage must not hard-code assumptions such as "Michelin is wet"
or "Pirelli is dry". If one historical tire is better in the wet than another, that comes from authored
data, measured/fitted evidence or an explicit creator calibration bias.

## Geometry-derived baseline

The creator should not have to invent a complete tire model from zero. The Tire Part Lab can generate a
starting profile from the part's known engineering data. Important inputs include tire width, aspect
ratio, rim diameter, nominal/loaded radius, pressure, load rating, mass/construction and tread metadata.

"Bigger tire = more grip" must **not** be implemented as a direct friction multiplier. Tire dimensions
change several mechanisms instead: contact-patch shape, load sensitivity, carcass stiffness, thermal
mass and heat distribution, hydroplaning/drainage behavior, rolling resistance, rotational inertia and
operating-load envelope. The serious native tire model remains responsible for turning those properties
into forces.

When measured `.tir`/fitted data exists, it overrides generated estimates. The generated baseline is a
creator convenience and fallback, not a claim of manufacturer-accurate performance.

## Simple creator controls

The normal Vehicle Parts/Tire Lab should expose a compact set of **surface-performance biases** around the
generated or measured-average profile. These controls are available to every tire part; they are not
restricted to an "off-road tire" class because road cars, motorcycles, trucks, karts, ATVs and future
trikes may all leave pavement or encounter winter/loose surfaces.

1. **Dry Performance Bias** — nudges the dry compound/grip envelope and related calibrated parameters.
2. **Wet Performance Bias** — nudges wet-compound effectiveness, drainage/water-film tolerance and
   hydroplaning-related calibrated parameters.
3. **Snow / Ice Performance Bias** — nudges cold-compound effectiveness plus snow/ice-compatible tread,
   edge/sipe and mechanical-keying behavior. The simple UI may combine snow and ice, while Advanced
   authoring can separate compact/deep snow, slush, bare ice and stud behavior.
4. **Mud Performance Bias** — nudges tread bite, void/self-cleaning behavior and soft-ground shear/sinkage
   compatibility; it must not bypass the deformable-terrain model.
5. **Sand Performance Bias** — nudges flotation, sinkage/shear compatibility and loose-sand traction
   behavior appropriate to the tire construction and pressure envelope.
6. **Gravel Performance Bias** — nudges loose-gravel/hardpack mechanical keying, tread penetration and
   shear behavior without becoming a generic final-force multiplier.
7. **Wear / Endurance Bias** — nudges abrasion rate, heat endurance and life while preserving the
   underlying thermal/wear model.

The UI can present these as sliders centered on `Average / Neutral` (for example a normalized -1..+1
authoring value), grouped under **Surface Performance** with Wear/Endurance beside them. The exact mapping
is engine-owned, bounded and versioned. Each slider adjusts a small coherent parameter set before/during
the physical model; none may simply multiply final tire force after the solver runs.

These biases are deliberately **residual calibration controls**, not substitutes for engineering inputs.
Tread depth/void ratio, block geometry, edge density, siping, studs, construction, pressure, dimensions,
load and compound metadata establish the baseline first. For example, a smooth road-performance tire may
start with weak mud/sand/gravel behavior even with all sliders at Neutral, while a rally-gravel or
mud-terrain tire starts from a different physical baseline. The creator then uses the sliders only to
nudge the expected real-world character when complete measured/fitted data is unavailable.

A future Advanced panel may expose temperature window, cold grip, rolling resistance, sidewall/stiffness,
pressure sensitivity, detailed snow-versus-ice response, stud count/protrusion, mud self-cleaning, sand
flotation and gravel/hardpack response individually.

## Factory/default tire pressures and test acceleration

The tire **part** owns its valid inflation-pressure range and any reference/test pressure used to generate or fit tire-model parameters. The **vehicle fitment** owns the factory/default **cold inflation pressure**, because the same tire can require a different pressure on another vehicle, axle or load condition. Racing United vehicle GLB/custom-property metadata should therefore be able to author stock cold pressure per wheel group/axle (for example front/rear on the Peugeot 206 RC) without assuming four wheels.

The Vehicle/Tire Lab should generate pressure controls dynamically from the vehicle topology: `All -> axle/group -> individual wheel`. Cars, motorcycles, trikes, dual-wheel trucks and multi-axle vehicles therefore use the same authoring contract. The UI may display bar or psi, but storage should use one canonical physical unit. Live hot pressure remains simulated from cold pressure + tire thermal state rather than being authored as a fixed value.

For development/calibration only, the Lab should also expose non-persistent accelerators such as tire-wear speed and rubber-deposition/marble-generation multipliers plus reset-tire-state and reset-track-rubber controls. These are test tools only; normal gameplay remains at the physically calibrated 1x rates.

## Vehicle Parts Lab workflow

Preferred future editor/tooling direction:

```text
HeritageEditor / Vehicle Parts
    Tires
        Identity & dimensions
        Engineering metadata / provenance
        Surface Performance
            Dry Performance Bias
            Wet Performance Bias
            Snow / Ice Performance Bias
            Mud Performance Bias
            Sand Performance Bias
            Gravel Performance Bias
        Wear / Endurance Bias
        Advanced tire-model data
        Test / calibration views
```

A tire definition can then be assigned per axle or per wheel in the Vehicle Workshop. This supports:

- factory tire fitments;
- alternative road tires;
- race slick/wet sets;
- asymmetric or staggered fitments;
- motorcycle front/rear-specific tire constructions;
- truck axle-specific tires;
- replacement/modding parts without duplicating tire data inside every vehicle.

The tool should eventually provide repeatable dry/wet braking and cornering plus snow/ice, mud, sand,
gravel/hardpack, temperature, wear and hydroplaning calibration views so the simple biases are observable
rather than mysterious. Surface tests should use the same authoritative surface/terrain systems as the game.

## Tire marbles and track rubber

Tire marbles are a specialized tire/rubber feature. The authoritative simulation should distinguish:

- **deposited/rubbered-in track state** — spatial world state used by every vehicle;
- **loose-rubber concentration** — efficient world state affecting pickup/grip;
- **visible marble clusters** — dedicated presentation/instancing/particle ownership;
- **optional sparse high-detail debris** — only where actual physical pieces provide measurable value.

This keeps marbles visually and behaviorally special without requiring every rubber chunk to exist as a
persistent rigid body. Tire pickup/cleaning remains tire-owned state; deposited/loose rubber remains
shared track state.

## Relationship to tire-family simulation

TIRE17 should use tire-part definitions and creator biases as authoring inputs to reusable native
mechanisms. A road tire, wet race tire, motorcycle tire and truck tire may require different parameter
ranges or genuinely different model branches, but the editor should not create a separate physics engine
for every brand/product.

Measured and fitted data is always preferred. Estimated or slider-calibrated data remains explicitly
identified as such.
