# STUDIO17 — Police / Clandestine Free-Roam Gameplay

STUDIO17 adds a gameplay authority above the existing road/traffic stack without replacing any traffic system.

## Authoring
The Gameplay workspace now includes **POLICE / UNDERGROUND**. It authors global pursuit policy, patrol zones, roadblock sites, escape/cooldown zones and clandestine meets. All spatial objects can be placed on Scene_*.glb geometry and moved with the existing viewport gizmo.

Police gameplay is disabled by default so old scenes retain their behavior until explicitly enabled.

## Data ownership
`gameplay.hgame` advances to **HGAME v3**. HGAME v1/v2 remain readable. HROAD stays responsible for roads, lane graphs, traffic operations and traffic agents; HGAME consumes those systems for gameplay.

## Runtime
`Runtime/PoliceGameplay.lua` owns heat, pursuit/search/cooldown state, witnessing, speed enforcement, emergency-unit dispatch, roadblock activation and escape-zone influence. Police units reuse STUDIO14-16 Emergency traffic agents and the semantic route finder rather than implementing a separate navigation stack.

Roadblocks are injected into the existing traffic incident authority so civilian traffic sees and reacts to them. Pursuit targets are resolved against compiled traffic graph nodes, not road-spline authoring control points.

## Debugging
The Prototype Lab gains a **POLICE** tab with live pursuit telemetry and deterministic debug controls for reporting infractions, raising heat, forcing search/escape and clearing runtime state.
