# Heritage Engine Technical Memory

This directory is the repository-owned memory for Heritage Engine and Racing United. Chat history, an AI model's recollection, and an old screenshot are never authoritative for exact APIs or ownership rules.

Read these documents before changing an interconnected system:

1. `PROJECT_STATE.md` — current known-good milestone and immediate roadmap.
2. `AI_WORKFLOW.md` — required workflow for an AI or human contributor.
3. `ARCHITECTURE.md` — subsystem boundaries and dependency direction.
4. `MEMORY_OWNERSHIP.md` — object lifetime, handles, destruction, and Lua/C++ safety.
5. `LUA_API_RULES.md` — binding contract and generated API manifests.
6. `PHYSICS_ARCHITECTURE.md` — fixed-step and multi-rate physics rules.
7. `VEHICLE_ARCHITECTURE.md` — generic vehicle composition and Lua/native split.
8. `VEHICLE_DYNAMICS_LAB.md` — native recording, repeatable experiments, plots and CSV diagnostics.
9. `Decisions/` — architecture decision records that must not be silently reversed.

Generated reports are written to `Build/Reports/`. Runtime diagnostics are written to `UserData/Diagnostics/`.
