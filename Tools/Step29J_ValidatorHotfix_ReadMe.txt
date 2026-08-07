Heritage Engine Step 29J validator hotfix

Reason:
Step 29J split the old UI/Vehicle/VisualPanel.lua into a coordinator plus
UI/Vehicle/Visual/BodyPanel.lua and UI/Vehicle/Visual/WheelsPanel.lua.
The safety validator still searched the coordinator for the body slider text,
so it produced a false FAIL even though the feature exists in BodyPanel.lua.

Install:
1. Extract this ZIP over the RacingUnited project root.
2. Replace Tools/ValidateProject.ps1.
3. Run Tools/BuildAndRunStep29J.cmd again.

No engine, physics, Lua gameplay, vehicle model, PlayerCar.obj, or settings files
are changed by this hotfix.
