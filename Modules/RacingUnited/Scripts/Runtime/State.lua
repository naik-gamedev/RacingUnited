-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
-- Step 29E.1 modular runtime state. This module has its own isolated Lua global environment.
-- Racing United - Lua-owned runtime with native steering and drivetrain systems.
-- Edit this file while the module is running. Heritage Engine hot-reloads it.

demoSpeed = Save.GetNumber("prototype.speed", 1.0)
showTechnicalText = Save.GetBool("prototype.show_technical_text", true)
testValue = Save.GetInt("prototype.test_integer", 5)
credits = Save.GetInt("career.credits", 25000)
transitionMessage = ""
safetyNetMessage = "Safety tests have not been run in this process."
safetyNetReportPath = ""
saveMessage = "Loaded persistent module state"
audioMessage = "Audio backend: " .. Audio.GetBackend()
ambienceHandle = 0
ambienceVolume = Save.GetNumber("audio.demo_ambience_volume", 0.35)
inputPosition = 0.5
inputDrive = 0.0
inputMessage = "Keyboard and gamepad bindings can work simultaneously"
playerEntity = 0
chassisEntity = 0
wheelFrontLeft = 0
wheelFrontRight = 0
wheelRearLeft = 0
wheelRearRight = 0
cameraMountEntity = 0
cabinEntity = 0
noseMarkerEntity = 0
temporaryEntity = 0
prefabCloneEntity = 0
showPrototypeControls = true
prototypeScenePreset = ""
visualSteering = 0.0
destroyedEntityHandle = 0
entityMessage = "Entity hierarchy has not been tested yet"
physicsProbeEntity = 0
physicsProbeBody = 0
physicsProbeCollider = 0
physicsFloorEntity = 0
physicsFloorBody = 0
physicsFloorCollider = 0
physicsProbeStartX = -3.2
physicsProbeStartY = 2.4
physicsProbeStartZ = 0.0
physicsProbeHalfX = 0.90
physicsProbeHalfY = 0.22
physicsProbeHalfZ = 0.55
physicsTickRate = Physics.GetTickRate()
physicsTimeScale = Physics.GetTimeScale()
physicsPaused = Physics.IsPaused()
physicsMessage = "Four native spring/damper constraints are supporting the chassis probe"
physicsRayOriginEntity = 0
physicsRayHitEntity = 0
physicsRayOriginX = -3.2
physicsRayOriginY = 7.0
physicsRayOriginZ = 0.0
physicsRayHit = false
physicsRayCollider = 0
physicsRayBody = 0
physicsRayDistance = 0.0
physicsRayPointX = 0.0
physicsRayPointY = 0.0
physicsRayPointZ = 0.0
physicsRayNormalX = 0.0
physicsRayNormalY = 1.0
physicsRayNormalZ = 0.0
physicsRayTrigger = false
physicsRayCandidateCount = 0
physicsRayExactTestCount = 0
physicsOverlapCount = 0
physicsOverlapCandidateCount = 0
physicsOverlapExactTestCount = 0
physicsRayIgnoreProbe = false
physicsCcdProjectileEntity = 0
physicsCcdProjectileBody = 0
physicsCcdProjectileCollider = 0
physicsCcdWallEntity = 0
physicsCcdWallBody = 0
physicsCcdWallCollider = 0
physicsSphereCastHitEntity = 0
physicsCcdStartX = 1.0
physicsCcdStartY = 2.4
physicsCcdStartZ = -2.2
physicsCcdWallX = 4.5
physicsCcdRadius = 0.22
physicsCcdLaunchSpeed = 600.0
physicsCcdEnabled = true
physicsCcdLaunched = false
physicsCcdOutcome = "Ready to launch"
physicsSphereCastHit = false
physicsSphereCastDistance = 0.0
physicsSphereCastPointX = 0.0
physicsSphereCastPointY = 0.0
physicsSphereCastPointZ = 0.0
physicsSphereCastNormalX = 0.0
physicsSphereCastNormalY = 0.0
physicsSphereCastNormalZ = 0.0
physicsSphereCastCandidateCount = 0
physicsSphereCastExactTestCount = 0
physicsSpringConstraints = {}
physicsSpringAnchorEntities = {}
physicsSpringRestLength = 1.55
physicsSpringStiffness = 3500.0
physicsSpringDamping = 450.0
physicsSpringMaximumForce = 25000.0
physicsSpringsEnabled = true
physicsSpringAverageLength = 0.0
physicsSpringAverageExtension = 0.0
physicsSpringTotalForce = 0.0
physicsSpringMaximumAbsForce = 0.0
physicsSpringAnchorY = 4.70


-- Vehicle state is initialized by Scripts/Vehicles/State.lua.

Input.RegisterAction("Confirm", "Key:Space", "Common")
Input.RegisterAction("Horn", "Mouse:Right", "Common")
Input.RegisterAction("Toggle 3D View", "Key:Tab", "Common")
