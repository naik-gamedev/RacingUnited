# ADR-036 — Human-readable TIR parameter import

**Status:** Accepted at TIRE02 (2026-08-09).

## Decision

Heritage Engine imports human-readable MF-Tyre/MF-Swift-style `.tir` parameter files
through an engine-owned clean-room data boundary. The parser maps only coefficient
families and metadata whose semantics are implemented from public documentation.
Unsupported assignments remain visible diagnostics. Obfuscated/proprietary property
data is rejected rather than decoded, copied or guessed.

A tire imported from a property file records source, provenance, confidence, FITTYP
and mapped/unsupported counts. Vehicle definitions store only safe module-relative
paths. Runtime file I/O occurs during configuration/loading, never inside the high-rate
vehicle substep.

`FITTYP=62` is the primary TIRE02 target. `FITTYP=70` may use the common MF6.2
steady-state subset while its Temperature & Velocity coefficients remain inactive and
reported as unsupported until Heritage has an independent thermal/velocity mechanism.
Valid `MC_CONTOUR_A/B` data selects the motorcycle tire branch but does not imply that
the vehicle topology itself implements motorcycle lean dynamics.

## Consequences

- Real fitted datasets can replace synthetic seed coefficients without changing solver code.
- Historical/estimated datasets can be tagged honestly instead of masquerading as measured data.
- Future turn-slip, rigid-ring, enveloping, thermal and wet-road milestones can consume
  already-preserved parameter families incrementally.
- The engine never claims compatibility with unpublished proprietary solver behavior merely
  because a property file contains similarly named sections.
