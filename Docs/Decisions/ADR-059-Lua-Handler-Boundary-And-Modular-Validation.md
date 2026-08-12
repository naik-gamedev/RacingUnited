# ADR-059 — Lua handler boundary and modular validation

**Status:** CLEAN12 implementation candidate  
**Date:** 2026-08-10

## Context

CLEAN07 moved Lua registration tables out of `LuaModuleRuntime.cpp`, but the runtime header
still declared roughly 400 domain handlers and included major Audio/Input/Entity/Physics/
Graphics headers. Small binding changes therefore carried a broad declaration/dependency
surface. At the same time `Tools/ValidateProject.ps1` had grown beyond two thousand lines
and had itself become a recurring source of parser/stale-ownership failures.

## Decision

1. Keep `LuaModuleRuntime` authoritative for Lua-state lifetime, sandboxing, module lifecycle,
   ordered registration, error handling, hot reload and runtime-owned state.
2. Move Lua C-handler declarations behind four private friend catalogues: Core, Physics,
   Vehicle and Entity. These are implementation details, not public APIs.
3. Binding translation units include the concrete engine service headers they use. The
   runtime header stores service pointers through forward declarations and must not regain
   transitive subsystem implementation ownership.
4. Preserve public Lua names and registration order. The API-manifest generator resolves
   domain handler owners and remains the compatibility authority.
5. Keep one public validator entry point (`Tools/ValidateProject.ps1`) while dot-sourcing
   ordered, responsibility-owned modules below `Tools/Validation/`. Validation modules share
   the runner's `Check()`/report state; they are not separate user workflows.

## Consequences

- Ordinary runtime consumers parse a substantially smaller header.
- A domain binding file has a clearer dependency on the service it actually exposes.
- Future API growth does not require adding hundreds of declarations to the runtime class.
- The validator can keep growing in coverage without growing one fragile parser surface.
- Domain handler structs are deliberately private implementation catalogues; they must not
  become alternate service locators or contain Racing United-specific gameplay.
