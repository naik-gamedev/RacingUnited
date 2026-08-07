# AI and Contributor Workflow

This workflow exists to prevent context-window drift, invented function signatures, stale-file edits, and plausible but unverified fixes.

## Source of truth order

1. Current repository source and generated reports.
2. Architecture decision records and ownership documents.
3. Latest successful build/test report.
4. User observations and screenshots.
5. Conversation memory only for broad intent, never exact signatures.

## Before every code change

- Identify the exact Visual Studio project and files that compile.
- Search the current source for every function, type, and binding being changed.
- Regenerate `Build/Reports/LuaAPI.md` instead of recalling Lua names from memory.
- Read the relevant ownership and architecture documents.
- Check recent milestone notes and existing tests.
- State any assumption that cannot be verified from the repository.

## While changing code

- Keep the update narrow, reversible, and independently testable.
- Preserve public names unless an explicit migration is included.
- Use generation-checked handles across Lua and subsystem boundaries.
- Do not store unmanaged raw pointers to lifetime-owned engine objects.
- Add or update validation beside the feature.
- Keep heavy deterministic simulation in native C++ and Lua orchestration/data in named modules.
- Update documentation when a contract or ownership rule changes.

## Before claiming success

- Run static project validation.
- Build the exact target configuration.
- Verify the expected marker exists in the freshly built executable.
- Run applicable runtime smoke tests.
- Distinguish clearly between isolated tests, compilation, and full runtime testing.
- Provide the user with the exact command that rebuilds and launches the fresh executable.

## Debugging failures

Do not guess from “it broke.” Collect:

- Build identity.
- First compiler/linker error.
- Lua file and line.
- Exception code and stack trace when available.
- `UserData/last_launch.txt`.
- `UserData/Diagnostics/safety_smoke_last.txt`.
- Reproduction steps and whether the failure is deterministic.

For performance issues, profile first. Use Visual Studio Diagnostics or Tracy markers later; do not optimize by intuition alone.

## Handoff package

A future project ZIP should include at minimum:

- `Engine/`, `Modules/`, `Tools/`, and `Docs/`.
- `Build/Reports/LuaAPI.json` and `LuaAPI.md` if generated.
- `UserData/last_launch.txt` and relevant diagnostic reports.
- Git metadata when practical, or at least the commit hash recorded by build identity.

Never rely on a chat transcript as the only record of a technical decision.
