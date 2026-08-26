# OPT01A — Overlay-safe retirement hotfix

OPT01 exposed an important property of the ZIP-based development workflow: extracting a newer ZIP over an existing checkout overwrites changed files, but it cannot remove files that were intentionally deleted from the new archive. Therefore dead-code retirement must be expressed as a deterministic convergence step as well as by omission from the archive.

`Tools/Diagnostics/ApplyOPT01Retirement.ps1` contains the explicit, reviewed OPT01 deletion set and runs before the code-health snapshot and repository validation. This preserves the strict safety net: if retirement cannot be applied, the build stops rather than teaching the validator to accept stale files.

The second fix narrows the VehicleArchitecture ItemGroup validator to files that actually belong to that group. Chassis flex remains protected by the existing FLEX01 compile checks.
