Racing United / Heritage Engine — SUSP24 to SUSP26 FINAL SUSPENSION CLOSURE

This replaces the misleading header-only notion of SUSP15-SUSP23 completion.

WHAT IS HERE
- one production corner/vehicle/axle suspension force coordinator
- real provider callback registry
- dynamic 6DOF compliance + feedback into next kinematic solve
- detailed Damper V3
- dynamic air + hydropneumatic springs
- physical active actuator force-speed-power limits
- semi-active damper control, ride-height control, active anti-roll
- leaf friction, inerters, third/heave and hydraulic interconnection
- complete damage transitions including leak/seize/break/detach + bent geometry feedback
- full 3D alternative motorcycle front
- deterministic versioned runtime state serialization
- runtime validation/duplicate-authority contract
- portable certification + ASan/UBSan clean reference run

PORTABLE CERTIFICATION RESULT
See Tests/CERTIFICATION_OUTPUT.txt and Tests/SANITIZER_OUTPUT.txt.

INSTALL
From PowerShell, run:
  Tools\Apply_SUSP24_to_SUSP26_Core.ps1 -RepoRoot C:\Users\Naik\Documents\Codex\RacingUnited

IMPORTANT
The installer deliberately exits non-zero if the real checkout still lacks VehicleSystem/Lua/Studio/native-regression wiring. That is not a core compile failure; it prevents another disconnected header package from being mislabeled "suspension complete".

The attachment runtime did not expose the current merged RacingUnited source tree, so this package cannot truthfully make line-accurate edits to files it cannot read. Use Build\Reports\SUSP24_26_WiringAudit.txt to identify the remaining live anchors in that checkout.
