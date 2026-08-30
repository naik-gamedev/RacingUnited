# STUDIO20 — Racing AI Intelligence & Strategy

STUDIO20 adds a dedicated Racing AI decision layer above STUDIO19's grid/weekend/championship authority. It does not replace venue routes, timing, traffic, police, event execution, competitors or championship data.

## Authoring

HGAME v6 adds `MotorsportAiConfiguration` plus per-entrant racecraft traits. Global controls cover decision frequency, steering/braking lookahead, opponent awareness, slipstream windows, overtaking thresholds, defending, blue-flag behavior, multi-class negotiation, wet-line activation, fuel/tire consumption, pit thresholds, strategy, and mistakes.

Entrants additionally own racecraft, awareness, defending tendency, tire management, fuel management, strategy risk, mistake frequency, reaction time, and preferred line bias. HGAME v5 loads these with safe defaults.

## Runtime authority

`Runtime/RacingAI.lua` is representation-independent. It produces a control intent and telemetry for each logical competitor. The current STUDIO19 lightweight proxy backend consumes the resulting target speed, dry/wet-line choice and lateral line offset. A future full Heritage race-car controller can consume the same `GetControlIntent()` output instead of replacing the AI intelligence.

Decisions include authored target-speed/braking lookahead, racing/wet line, slipstream, overtaking, defending, blue-flag yield, multi-class yield/pass, pit strategy, Safety Car/FCY/VSC behavior, and mistake recovery.

Fuel and tire state are simulated continuously. AI can pit for fuel, tire life, weather crossover, maximum stint, or mandatory stops. Pit service is integrated with MotorsportWeekend rather than being an editor-only setting.

## Debugging

The Racing United RACE prototype tab exposes live per-agent decision, reason, target/current speed, brake demand, lateral offset, wet-line blend, nearest opponents, slipstream/overtake/defend/yield state, fuel, estimated laps remaining, tire life/compound, pit request, and mistake recovery.

## Compatibility

- HGAME v6 writes STUDIO20 data.
- HGAME v1–v5 remain readable.
- Generated `StudioGameplay` schema is v12.
- STUDIO19 competition/weekend/championship behavior remains present.
