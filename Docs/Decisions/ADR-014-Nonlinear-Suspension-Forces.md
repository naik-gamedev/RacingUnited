# ADR-014: Non-linear Suspension Forces and Energy Telemetry

Status: Accepted in Step 29M.

## Context

A linear spring and one bump/rebound coefficient cannot reproduce progressive
road springs, digressive racing dampers, travel stops, or the loads needed for
later thermal, wear, and failure models.

## Decision

`SuspensionModel` evaluates healthy force components separately. Spring preload
and progression, digressive bump/rebound damping, bump/droop stops, motion
ratio, and force limiting are authored in the suspension component. The model
reports every component and instantaneous damper dissipation to wheel state and
the opt-in Dynamics Lab.

The runtime uses no hidden vehicle-category tuning. Defaults may form a simple
linear model, and unsupported linkage providers remain unresolved.

## Consequences

- Suspension tuning can be measured and diagnosed rather than inferred from a
  single total wheel load.
- Damper thermal and wear work can later integrate real dissipated energy.
- Travel-limit impacts can later drive physical damage thresholds.
- This does not yet add unsprung mass, linkage kinematics, anti-roll coupling,
  temperature, wear, or damage.
