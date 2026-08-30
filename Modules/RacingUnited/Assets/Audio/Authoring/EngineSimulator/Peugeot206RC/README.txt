HERITAGE ENGINE SOUND CAPTURE LABORATORY - PEUGEOT 206 RC
==========================================================

Engine Simulator source model:
  Peugeot_206_RC_EW10J4S_FINAL_STOCK.mr

Target application:
  Engine-Simulator/engine-sim-community-edition
  Community Edition direct Load Engine workflow.

Final Heritage calibration reference:
  173 hp @ 6790 rpm
  148 lb-ft @ 4798 rpm

Workflow:
  1. Load the .mr in Engine Simulator Community Edition.
  2. In Racing United open Vehicle -> LAB -> AUDIO.
  3. CAPTURE tab: record a calibration source with WASAPI loopback.
  4. SHAPE / FILTER tab: non-destructively audition RAW, ENGINE BAY,
     REAR / EXHAUST and DRIVER CABIN perspectives.
  5. Save a .hacoustic profile when the source character is convincing.
  6. Return to CAPTURE and follow the 53-cell RPM x throttle bank.

Heritage stores captures/profile data beneath:
  UserData/Modules/RacingUnited/EngineSoundLab/

Raw captures are never overwritten by the shaping stage.
