# ADR-004: Native Simulation, Lua Configuration and Orchestration

**Status:** Accepted

Heavy reusable deterministic simulation lives in native C++. Lua supplies data, gameplay orchestration, UI, input mapping, spawning, and module-specific rules.

Prototype code may temporarily live in Lua, but production high-rate tire, suspension, drivetrain, differential, aero, networking-critical, and force-feedback logic must migrate behind stable native APIs.
