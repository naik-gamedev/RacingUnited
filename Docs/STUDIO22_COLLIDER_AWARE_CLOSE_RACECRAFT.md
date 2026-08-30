# STUDIO22 — Collider-Aware Close Racecraft & Stewarding

STUDIO22 deepens the STUDIO20/21 Racing AI stack without replacing any existing race, event, timing, vehicle or authoring authority.

## Collision geometry is the chassis authority

`Physics.GetBodyCollisionBounds(body)` exposes the aggregate body-local bounds of the body's **solid** collision shapes. Trigger/sensor volumes are deliberately excluded so detection volumes cannot inflate the AI footprint. Compound collision bodies are supported by unioning every attached solid collider.

Racing AI consumes those bounds as its primary physical footprint. Logical width/length values remain only as a fallback for competitors that do not currently own a physical body or when collider-bound authority is explicitly disabled.

This API boundary intentionally does not assume a single box. As Heritage physics gains additional convex/mesh collider types, their bounds can feed the same body-level query without changing Racing AI racecraft.

## HGAME v8 racecraft policy

The Gameplay Competition/Racing AI authoring surface adds persistent control over:

- collider-bound footprint authority;
- collision-envelope safety margin;
- swept-envelope prediction horizon;
- side-by-side overlap tolerance;
- divebomb commitment distance and closing-speed threshold;
- switchback/crossover timing;
- legal defensive moves per straight;
- blocking time penalties;
- unsafe pit-release lookahead and penalties;
- multiclass pass-planning horizon;
- tire optimal temperature window;
- fuel density/mass awareness;
- predictive collision avoidance, stewarding, tire-thermal and component-damage strategy switches.

HGAME v1–v7 remain loadable. Missing STUDIO22 fields receive safe defaults.

## Close-quarters racecraft authority

`Runtime/RacingAIRacecraft.lua` sits between the high-level STUDIO20 decision proposal and the STUDIO21 physical vehicle controller. It uses route progress, lateral offset, velocity and collider-derived footprint to predict future overlap rather than waiting for physical contact.

Implemented behavior includes:

- predictive swept-envelope collision avoidance;
- side-by-side spatial margins based on both cars' physical footprint;
- braking-battle/divebomb commit or abort judgement;
- switchback/crossover response when a defender commits to the first line;
- multiclass pass planning before bumper-distance interaction;
- defensive-move counting with steward penalties;
- unsafe pit-release detection and participant penalties.

Steward penalties are real timing data through `RacingEvents.AddParticipantPenalty`; they affect participant elapsed time/classification rather than existing only as debug messages.

## Physical feedback into strategy

The STUDIO21 native Racing AI vehicle controller now feeds additional physical state back into the intelligence layer:

- body collision footprint;
- tire tread/surface temperature and thermal grip factor;
- fuel mass and updated vehicle body mass;
- aero, suspension and powertrain condition;
- collision/mechanical damage.

Racing AI can use tire-temperature state, fuel mass and component condition for pace and pit-strategy decisions. Pit service partially repairs component/mechanical health through the same physical backend.

## Debugging

The Racing United RACE prototype panel exposes:

- physical footprint source, width, length and solid-collider count;
- predicted swept-collision state and longitudinal/lateral margin;
- divebomb state, defensive moves and switchback state;
- steward penalty and unsafe-release state;
- tire temperature and thermal grip;
- vehicle/fuel mass;
- aero/suspension/powertrain condition.

The purpose is that future asset-backed race cars can be diagnosed from live physical evidence instead of guessing why an AI car chose a maneuver.

## Regression expectations

STUDIO22 must preserve all STUDIO21 behavior. In particular:

- no existing project files are deleted;
- full-physics Racing AI remains opt-in;
- logical/kinematic competitors remain valid fallback tiers;
- STUDIO21 physical pit-lane traversal remains intact;
- HGAME v7 remains loadable;
- trigger volumes never affect chassis footprint;
- blocking/unsafe-release rules produce participant timing penalties.
