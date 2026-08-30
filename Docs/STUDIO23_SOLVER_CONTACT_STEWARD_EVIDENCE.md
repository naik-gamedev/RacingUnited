# STUDIO23 — Solver-Contact Steward Evidence

STUDIO23 continues directly from STUDIO22. It does not replace collider-authoritative footprint prediction, the STUDIO20 intelligence layer, the STUDIO21 native Heritage Vehicle controller, or RacingEvents timing/steward penalties.

The milestone closes the gap between **predicted contact risk** and **what physically happened**. Heritage's collision solver is now exposed as read-only evidence so race control can review actual resolved body contacts rather than inferring incidents from distance or AI intent.

## Physics contact evidence API

`Physics.GetBodyContact(body, index)` returns one contact from the most recently completed fixed physics step. Indexing is one-based and follows the body's contact list.

The result is a Lua table containing:

- requested/self body handle;
- other body handle (0 when the other side is immutable/static world geometry);
- self and other collider handles;
- contact point in the current local physics frame;
- contact normal oriented **from the requested body toward the other side**;
- penetration depth in metres;
- accumulated normal impulse in N·s;
- accumulated tangent impulse in N·s;
- trigger state;
- warm-start state.

The normal is automatically flipped when the requested body is contact B, so callers never need to know the solver's internal A/B ordering.

`Physics.GetBodyContactCount(body)` remains the cheap count query. Callers that need evidence can enumerate `1..count`; callers that only need contact/no-contact do not pay for Lua table construction.

## Physical Racing AI evidence capture

The native Racing AI vehicle controller samples the body's real contacts and keeps the strongest solid contact for the step. For that contact it derives:

- solver normal impulse;
- tangential impulse;
- relative closing speed projected onto the contact normal;
- front / rear / left / right contact zone from chassis yaw;
- contact penetration;
- local and floating-origin-safe global contact position;
- the other rigid-body handle.

Trigger/sensor contacts are ignored for incident review.

The physical evidence is returned alongside existing tire, grip, body-mass, component-health and collider-footprint feedback. It is therefore visible without creating another physics owner.

## Incident classification

`RacingAIRacecraft.ReportPhysicalContact` receives the real solver evidence after MotorsportWeekend resolves the contacted body back to another race entrant when possible.

Current classifications are intentionally conservative:

- **Rear-end contact** — front contact while the other competitor is longitudinally ahead, or the reciprocal rear-contact case;
- **Side-to-side contact** — left/right chassis contact; the larger recent authored lateral-line movement is used as a limited squeeze/change-of-line clue;
- **Front / crossing contact** — ambiguous front/crossing geometry;
- **Barrier / static-world contact** — no race competitor owns the other body.

Ambiguous incidents remain **Racing incident** rather than manufacturing blame.

## Steward policy

HGAME v9 adds authorable thresholds for:

- contact-evidence capture enable;
- incident stewarding enable;
- minimum normal impulse;
- minimum closing speed;
- severe-contact normal impulse;
- severe-contact closing speed;
- avoidable-contact time penalty;
- severe-contact time penalty;
- duplicate-contact evidence cooldown;
- retained incident evidence count.

A real contact becomes reviewable when either the minimum impulse or minimum closing-speed threshold is met. A contact is severe when either severe threshold is met.

When a conservative fault assignment exists, the existing `RacingEvents.AddParticipantPenalty` timing authority is used. The new system does not maintain a parallel penalty clock.

## Duplicate manifold suppression

One physical collision can produce persistent contacts across many physics steps and can be observed from both bodies. STUDIO23 therefore applies an authored evidence cooldown to both involved competitors after accepting an incident.

This prevents a single sustained door rub or bumper contact from becoming dozens of independent penalties while preserving later, separate impacts.

## Retained steward evidence

`RacingAIRacecraft.GetIncidentLog(maxCount)` returns newest-first retained incident records. Each record contains:

- incident id;
- classification and verdict;
- involved entrant ids/names;
- fault entrant when one was assigned;
- contact zone;
- normal/tangent impulse;
- relative closing speed;
- penetration;
- penalty seconds;
- global contact position.

The log is reset when a new motorsport event starts. It is bounded by the HGAME authoring policy so it cannot grow without limit.

This is the evidence structure needed for a future replay timeline, steward UI and post-race incident review without changing how contacts are generated today.

## Heritage Studio / runtime diagnostics

The Gameplay inspector gains a **STUDIO23 SOLVER-CONTACT INCIDENT EVIDENCE / STEWARD REVIEW** section with the thresholds and retention controls above.

The Racing United RACE debug panel now shows:

- strongest solver contact for the selected AI car;
- chassis contact zone;
- normal impulse and closing speed;
- penetration and contact normal;
- latest steward verdict / fault / penalty;
- a short recent steward-evidence list.

## Compatibility

- HGAME v1–v8 remain loadable.
- HGAME v9 adds one additive `MOTORSPORT_AI_INCIDENTS` record.
- STUDIO22 predictive close-racecraft remains active and independent.
- Logical/kinematic competitors remain valid; they simply have no solver evidence until represented by real physics bodies.
- Trigger volumes never become steward evidence.
- Existing participant timing and penalty authority remains unchanged.

## Next racecraft depth

The next useful layer should build on this evidence rather than invent a second incident model: multi-point contact manifolds and wheel-to-wheel classification, curved swept hull prediction, actual aero wake/drafting forces, race-start clutch/launch optimization, tire pressure/carcass strategy, pit-lane queue arbitration for team cars, and replay-timeline persistence of these steward records.
