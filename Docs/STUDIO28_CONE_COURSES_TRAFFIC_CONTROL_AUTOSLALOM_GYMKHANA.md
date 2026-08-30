# STUDIO28 — Cone Courses, Traffic Control, Autoslalom & Gymkhana

STUDIO28 turns traffic cones into a shared gameplay primitive instead of a decorative prop. The same authored cone can participate in ordinary free-roam traffic management or belong to a temporary motorsport event overlay. Visible/physical cones are intentionally separate from invisible course-rule elements, so knocking a cone away never destroys the timing or course authority.

## Cone asset contract

The default module-relative visual path is:

`Assets/Props/TrafficCone.glb`

A creator can model a cone in Blender and export a normal GLB with its origin at the **centre of the cone base**. Heritage Studio can override the asset path and visual scale per cone. If the visual asset is missing, the runtime uses a debug primitive so course logic can still be tested.

The runtime physics approximation is deliberately cheap: a low broad box for the weighted base plus a narrower upper box. Cones are light dynamic bodies with sleeping, damping, authored mass/friction/restitution and reset-to-authored-transform support. This is not a soft-body cone simulation; it is intended to preserve believable tumbling/sliding while remaining viable for large event layouts.

## Heritage Studio workflow

Open **RACE → CONE COURSES**.

### Persistent free-roam traffic cones

Use `Event = Persistent / Free Roam (0)` and choose a traffic meaning:

- **Guide / Discourage** — road remains legal but routing cost increases; an optional lower speed can be applied.
- **Slow** — increases route cost and imposes an authored temporary speed limit.
- **Close Lane** — blocks the targeted road/link/lane for ordinary traffic.
- **Close Road** — blocks the targeted road/link for ordinary traffic.

A cone can target an exact generated/manual traffic link or a road and optional lane index. Emergency traffic is allowed to ignore a cone closure so police/emergency routing does not deadlock.

### Autoslalom / gymkhana event overlay

1. In GAMEPLAY create an **Autoslalom** or **Gymkhana** event.
2. In RACE → CONE COURSES place one or more `Start` cones and assign them to that event. A pair of Start cones naturally defines the middle of the start line.
3. Place ordinary/slalom/gate/turnaround/stop-box cones as desired. These are the visible and physical objects.
4. Add invisible ordered **course elements**. They are the actual rule/timing authority.
5. Optionally assign left/right visual cone IDs to a gate and use **FIT GATE TO REFERENCED CONES**.
6. End the sequence with a **Finish** course element.
7. Use **QUICK COURSE BUILDERS** when useful: Start/Gate/Finish pairs, alternating slalom, Stop Box, 180° turnarounds and 360° circles can be generated from the selected cone/course element or current view target.
8. Publish gameplay and launch the event from the Racing United race panel.

Event-scoped cones can be hidden outside their event, spawned when the event arms, and restored to their authored locations at the next start. Persistent free-roam cones remain present independently.

### Cone lines and traffic tapers

Select any physical cone and use **DUPLICATE SELECTED CONE LINE / TAPER**. The generated cones copy the selected cone's complete visual, rigid-body, penalty and traffic-control semantics. With lateral shift `0` this creates a straight line; a non-zero total lateral shift creates a diagonal taper suitable for lane closures and roadworks. The tool also works with `Event = Persistent / Free Roam (0)`.

## Course elements

STUDIO28 implements deterministic ordered course elements:

- **Gate** — cross the plane within the authored width.
- **Slalom Left** — cross on the required left side of the cone/centre reference.
- **Slalom Right** — cross on the required right side.
- **Turnaround Left** — cross into the element on the required side and return back past its line.
- **Turnaround Right** — right-side equivalent.
- **Stop Box** — remain inside the authored rectangle at/below the configured speed for the dwell time.
- **Finish** — completes the course.
- **360 Circle Left / Right** — accumulates signed orbit angle around a reference cone/centre and requires approximately one full revolution in the authored direction; leaving the maneuver by a large margin before completion resets its progress.

Course elements stay in their authored world-space positions even if visible cones are knocked away. Crossing a later element while an earlier one is still expected proves the earlier element was skipped. The expected gate can either issue an authored time penalty or DNF the run.

## Cone-strike rules

Each event cone can use one of four penalty triggers:

- **None**
- **Contact** — qualifying player/cone solver contact immediately counts.
- **Displaced** — contact arms the cone, but the penalty is issued only if its base moves beyond the authored tolerance.
- **Knocked Down** — contact arms the cone; penalty is issued once its tilt exceeds the threshold.

STUDIO23's resolved body-contact evidence is reused, including the player body ID and normal impulse. This prevents tiny/noisy contacts below the global impulse threshold from becoming penalties. One cone produces at most one cone penalty per run until reset.

When enabled, a penalized cone strike creates a STUDIO24 replay incident bookmark at the cone's FP64 world position, allowing the existing steward replay/broadcast camera stack to inspect the hit.

## Timing and results

Autoslalom/gymkhana events use the existing event lifecycle for staging, countdown, false-start handling, flags, penalties and results, but skip circuit-only formation/pit/track-limit checkpoint logic. The cone-course authority reports element progress, expected element, cone hits and course penalties to the Race panel.

A clean completion records the **adjusted time** (elapsed time + penalties) as the event personal best when automatic PB saving is enabled. DNF runs do not overwrite the PB. Each completed course element also records an adjusted split. When a new overall PB is established, that run's gate splits become one coherent reference set; later runs show a live signed delta at every element instead of comparing against unrelated per-gate records.

## Persistence and compatibility

- **HRACE v7** adds `CONE_CONFIG`, `CONE` and `CONE_GATE` records.
- **HGAME v12** appends `Autoslalom` and `Gymkhana` event types without renumbering historical event values.
- Generated **StudioGameplay schema v18** publishes cone/course/traffic data to Racing United.
- HRACE v1–v6 remain readable and receive default cone-course configuration with no authored cones.
- HGAME v1–v11 retain their historical event enum range and remain readable.

## Design boundary

Traffic cones influence the existing lane-graph/router at the road/link/lane level; STUDIO28 does not attempt expensive local obstacle path planning around arbitrary cone geometry. The physical traffic proxy can still collide with a cone, while route-level closure/slow/guide semantics provide deterministic free-roam traffic behavior.
