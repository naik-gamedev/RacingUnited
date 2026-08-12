# Tools hygiene

Heritage Engine keeps `Tools/` intentionally small.

## Normal daily-use helpers

- `00_BuildAndRunCurrent.cmd` — validate, run the headless native physics regression suite, rebuild Release x64, and launch the exact fresh executable. A regression failure blocks launch.
- `01_LaunchCurrent.cmd` — launch the current Release executable without rebuilding.

## Reusable infrastructure

- `ValidateProject.ps1`
- `GenerateBuildIdentity.ps1`
- `GenerateLuaApiManifest.ps1`
- `SetupLua.ps1`

Step-specific build CMDs and test-checklist files are disposable delivery helpers. They are **not project dependencies** and may be deleted after a step is integrated.

`00_BuildAndRunCurrent.cmd` intentionally checks only the build/safety entry points it directly invokes. Detailed source/document/scaffold inventory belongs to `ValidateProject.ps1`; do not duplicate a growing per-milestone file list in the CMD.

`ValidateProject.ps1` no longer requires historical helper CMDs. By default it prints only failures plus one summary line; the complete detailed result remains in `Build/Reports/ValidationReport.txt`. Pass `-VerboseOutput` manually when every PASS line is desired. Project-visible vehicle scaffolds are validated as a complete declared set, so a placeholder moved to the wrong directory cannot silently survive merely because it is not compiled yet.
