# ADR-060 — Final ownership pass and cleanup stop rule

**Status:** Accepted / CLEAN13 candidate  
**Date:** 2026-08-10

## Context

The CLEAN01-CLEAN12 program removed the largest vehicle, collision, renderer, input, glTF, runtime, Lua-registration, validation and surface-world dumping grounds. A final inspection identified four remaining files whose existing internal concepts already provided durable ownership boundaries, plus one inactive standalone launcher project and an overgrown project-state narrative.

Continuing to refactor merely because files are long would now create churn rather than clarity.

## Decision

CLEAN13 is the final planned architecture-only checkpoint. It:

- splits EntityRegistry implementation into registry/lifetime, hierarchy, transform, debug-component and mesh-component owners behind the existing public API;
- splits input bindings into action binding, analogue processing, capture, evaluation and parser/name owners behind the existing public API;
- splits Input Settings UI by its existing Bindings / Analogue / Profiles tabs behind one public page function;
- assigns collision broadphase/contact-candidate collection to a dedicated Broadphase implementation unit while preserving solver/narrowphase order;
- removes the historical Launcher project from the active solution while retaining its source only as labelled legacy reference; and
- keeps `PROJECT_STATE.md` concise while preserving the prior narrative verbatim under `Docs/History/`.

No physics equation, tire model, vehicle behavior, input semantics or UI feature is intentionally changed by this decision.

## Stop rule

After CLEAN13 is user-validated, architecture-only cleanup stops unless a concrete implementation blocker is discovered. New files/scaffolding are still created early when a known subsystem has a durable owner, but cleanup must not become an endless substitute for simulation/content/tooling progress.

The next development focus returns to TIRE15B/TIRE15C/TIRE16.
