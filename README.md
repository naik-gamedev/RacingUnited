![Racing United Banner](RacingUnited_GitHub.png)

Racing United is a racing simulator, a celebration of automotive history and culture. The game is designed to embrace both organized motorsport and clandestine racing culture as they exist around the world. Modify and tune vehicles for a wide range of disciplines or joyrides.  

This project is open-source and crowdsourced, built in collaboration with the community. Tutorials and guides will be readily available and regularly updated, constantly refining our craft to exceed the quality and pace of our previous workflow.

[Join our Discord community](https://discord.gg/rTfC2Ev)

![HeritageEngine](HeritageEngine.jpg)

## Current development platform

Heritage Engine currently targets Windows 10/11 x64 and is built with Visual
Studio using the Windows SDK and the v145 C++ platform toolset. GLFW, GLAD and
Dear ImGui are included in the repository.

Open `Engine/HeritageEngine/HeritageEngine.slnx`, select an x64 configuration,
and build the solution. The solution contains Heritage Engine, the Racing
United launcher, and a headless physics regression executable.

Run `Tools\RunPhysicsTests.cmd` to build and execute deterministic vehicle
tests without opening the game. They cover quiet flat-ground rest, parked-body
sleep/wake behavior, 1000 Hz suspension timing, handbrake holding on a 5-degree
slope, legitimate downhill motion when the car is not braked, nonlinear
suspension forces, scalar unsprung-mass wheel hop and vehicle-definition
compilation. Step 29Q also covers static triangle seams, reversed winding,
steep slopes, tiny gaps, bottom-out, support beyond droop, high-speed surface
crossing, scene bounds and airborne landing diagnostics.

Linux is not a supported build target during the current prototype phase. It
may return after the Windows engine, module API and racing simulation have
stabilized.
