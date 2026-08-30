# STUDIO24 — Incident Replay & Steward Ghost Review

STUDIO24 builds directly on STUDIO23 solver-contact evidence. It does not introduce another collision detector or another steward authority. Instead, every accepted STUDIO23 physical incident can now seal a short, bounded replay clip around the actual contact so the seconds before and after the incident can be reviewed spatially.

## Why incident-window replay first

A full-race replay recorder for 100–150 cars, especially across endurance events, should eventually use a compact native/binary stream rather than hundreds of thousands of long-lived Lua tables. STUDIO24 therefore starts with the highest-value replay product: a rolling pre-impact buffer plus a short post-impact tail only when a real incident occurs.

This keeps memory bounded while establishing the APIs, authoring policy, timeline controls and ghost playback needed by a later full broadcast/replay system.

## HGAME v10 replay policy

`MotorsportReplayConfiguration` is separate from Racing AI because replay is race-control/presentation infrastructure that can consume player, AI and eventually network participants.

Authorable fields:

- enable/disable incident replay capture;
- replay sample rate (default 12 Hz);
- pre-impact rolling history (default 8 s);
- post-impact tail (default 5 s);
- maximum retained incident clips (default 12);
- maximum recorded AI competitors per frame (default 32);
- player capture enable;
- control telemetry capture enable;
- non-physical ghost review enable;
- maximum simultaneously displayed review ghosts (default 16).

HGAME v1–v9 remain readable and receive these defaults.

## Runtime recorder

`Runtime/RacingReplay.lua` owns a bounded rolling frame buffer while an event is active. Samples contain floating-origin-safe global pose plus compact race state for the player and the configured number of AI competitors.

`RacingMotorsport.GetReplaySnapshot()` is representation-independent:

- native Heritage Vehicle competitors are sampled from their real rigid body;
- kinematic debug competitors are sampled from their body;
- logical/off-budget competitors are sampled from their route representation;
- all positions are converted into global coordinates before entering replay memory.

The configured AI participant cap is applied after the player. Because MotorsportWeekend physically promotes the front of its grid budget first, the cars capable of generating STUDIO23 body-contact incidents are naturally prioritized by the default replay budget.

## Incident sealing

`RacingAIRacecraft.ReportPhysicalContact()` remains the steward/evidence owner. After an accepted evidence record is created it calls `RacingReplay.MarkIncident(record)`.

The replay system then:

1. references the current rolling pre-impact history;
2. creates a clip/bookmark linked to the STUDIO23 incident id and verdict;
3. continues sampling through the authored post-impact interval;
4. seals the clip;
5. removes the oldest clips when the authored retention limit is exceeded.

Sustained manifold spam remains suppressed by STUDIO23 before it reaches replay creation, so one prolonged rub does not create dozens of clips.

## Review timeline

The RACE debug panel now provides:

- previous/next incident replay selection;
- play/pause;
- ±0.1 s and ±1.0 s stepping;
- direct jump to the physical impact time;
- timeline slider scrubbing;
- clip id / incident id / classification / verdict;
- time relative to impact;
- recorded phase and race-control flag;
- compact participant state at the selected replay time.

## Non-physical ghost playback

Review mode creates tagged `ReplayGhost` debug entities only. They have no rigid body and cannot affect the live race, contact solver, timing or AI.

Ghost transforms are reconstructed by:

- interpolating between replay samples;
- shortest-path interpolation of heading;
- converting stored global positions back through `Physics.GlobalToLocal` each frame.

That last step means an incident remains reviewable even if the floating origin has moved since the contact occurred.

Ghosts are destroyed when review is disabled or the prototype scene is left.

## Lifecycle

- starting a new event clears the previous in-memory replay session and arms a fresh rolling buffer;
- finishing/aborting an event stops capture but retains completed clips for post-event review while the prototype remains loaded;
- changing/leaving the prototype scene clears replay memory and destroys ghost entities.

## Compatibility and authority

- HGAME v10 is additive; v1–v9 still load;
- generated `StudioGameplay` schema is v15;
- STUDIO23 still decides which physical contact becomes an incident;
- RacingEvents remains the only penalty/timing authority;
- replay ghosts are presentation-only and never own physics;
- logical competitors remain valid replay sources even when they have no physical body.

## Next replay / race-control depth

The next layer can safely build on this foundation: native compact full-session replay streams, camera/broadcast cuts, tire/wheel contact sub-classification, incident-camera auto framing, telemetry graphs, flag/lap/pit timeline bookmarks, export/import of replay files, and network/server authoritative replay capture.
