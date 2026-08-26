# OPT00A — Profiling Validation Hotfix

OPT00's code-health validation check incorrectly wrote `Test-Path $path -and ...`, causing PowerShell to interpret `-and` as a `Test-Path` parameter. OPT00A changes that expression to `(Test-Path $path) -and ...`. Runtime and profiling code are unchanged.
