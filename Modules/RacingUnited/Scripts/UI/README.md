# Racing United Lua UI layout

The prototype debug UI is intentionally split by responsibility so humans and AI
contributors do not append unrelated controls to one giant file.

- `PrototypeScreen.lua`: top-level tab coordinator only.
- `Prototype/`: entity, physics, module, scene and safety top-level panels.
- `Physics/`: fixed-world, suspension, query/CCD and body diagnostic panels.
- `VehicleDebugPanel.lua`: vehicle sub-tab coordinator only.
- `Vehicle/`: driving, surfaces, tires, drivetrain, driver aids, telemetry, the topology-first Vehicle Workshop and the dynamics laboratory.
- `SafetyNetPanel.lua`: shared safety controls used by prototype and About screens.

Rules:

1. New vehicle debug controls go into the matching `UI/Vehicle/` subsystem.
2. New native-physics diagnostics go into the matching `UI/Physics/` subsystem.
3. Do not rebuild a monolithic prototype screen.
4. Simulation logic does not belong in UI files.
5. Scene presentation presets may hide debug geometry but must not change physics.
