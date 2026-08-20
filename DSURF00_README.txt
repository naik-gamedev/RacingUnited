HERITAGE ENGINE DSURF00 - DYNAMIC SURFACE FOUNDATION

Overlay this archive on the RacingUnited project root.
Then run:
  Tools\00_BuildAndRunCurrent.cmd

Primary roadmap:
  Docs\DYNAMIC_SURFACE_ROADMAP.md

Architecture decision:
  Docs\Decisions\ADR-128-Persistent-Dynamic-Surface-State.md

DSURF00 intentionally introduces the compiled foundation without deleting the live WATER18 runtime in the same step. The accepted roadmap requires staged replacement and hard deletion of WATER14-WATER18 presentation code plus old persistent marble storage after replacement paths compile. This avoids turning the repository into a knowingly broken intermediate state.
