# ADR-002: Generation-Checked Handles Across Lifetime Boundaries

**Status:** Accepted

Entities, bodies, colliders, constraints, and vehicles cross Lua and subsystem boundaries as integer handles containing an index and generation. Raw owning pointers are not exposed.

This makes stale references fail safely after destruction or slot reuse and is mandatory for new lifetime-owned engine object types.
