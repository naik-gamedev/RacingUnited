-- Scene lifecycle, fixed-step input, presentation and shift commands for the
-- current player vehicle. General module lifecycle remains in Runtime/Lifecycle.lua.
function ClearVehicleRuntimeHandles()
    nativeVehicle = 0
    nativeVehicleBody = 0
    nativeVehicleCollider = 0
    vehicleWheelTelemetry = {}
end

function VehicleOnPrototypeEnter()
    CreateNativeVehicleDemo()
    VehicleVisualOnPrototypeEnter()
end

function VehicleOnPrototypeExit()
    DestroyVehicleDemo()
end

function VehicleFixedUpdate(fixedDeltaTime)
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return
    end

    Vehicle.SetInputs(
        nativeVehicle,
        Input.Value("Throttle"),
        Input.Value("Brake"),
        ReadVehicleSteeringInput(),
        Input.Value("Handbrake"))
    RefreshVehicleTelemetry()

    if nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody) then
        local x, y, z = Physics.GetBodyPosition(nativeVehicleBody)
        local horizontalLimit = playerWorld.loaded and 1000.0 or 80.0
        if y < -20.0 or math.abs(x) > horizontalLimit or math.abs(z) > horizontalLimit then
            if playerWorld.loaded then
                ResetVehicleAtPlayerWorldSpawn(
                    "Safety-reset vehicle at Player Scene spawn")
            else
                ResetNativeVehicle()
            end
            vehicleMessage = "Safety-reset the vehicle after it left the active test world"
        end
    end
end

function VehicleUpdate(deltaTime)
    local steering = ReadVehicleSteeringInput()
    inputDrive = Input.Value("Throttle") - Input.Value("Brake")
    inputPosition = math.max(
        0.0,
        math.min(1.0, inputPosition + steering * deltaTime * 0.65))
    visualSteering = visualSteering
        + (steering * 28.0 - visualSteering) * math.min(1.0, deltaTime * 8.0)

    UpdateVehicleWheelPresentation()

    if Scene.GetCurrent() ~= "prototype"
        or nativeVehicle == 0
        or not Vehicle.Exists(nativeVehicle) then
        return
    end

    if Input.Pressed("Shift Up") then
        ReportVehicleGearChange(
            Vehicle.ShiftUp(nativeVehicle),
            "Requested the next higher gear")
    end
    if Input.Pressed("Shift Down") then
        ReportVehicleGearChange(
            Vehicle.ShiftDown(nativeVehicle),
            "Requested the next lower gear")
    end
    if Input.Pressed("Select Reverse") then
        ReportVehicleGearChange(
            Vehicle.SetGear(nativeVehicle, -1),
            "Requested reverse gear")
    end
    if Input.Pressed("Select Neutral") then
        ReportVehicleGearChange(
            Vehicle.SetGear(nativeVehicle, 0),
            "Requested neutral")
    end
end
