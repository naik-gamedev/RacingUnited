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

    local controlled, throttle, brake, steering, handbrake =
        VehicleDynamicsLabFixedInputs(fixedDeltaTime)
    if not controlled then
        throttle = Input.Value("Throttle")
        brake = Input.Value("Brake")
        steering = ReadVehicleSteeringInput()
        handbrake = Input.Value("Handbrake")
    end
    Vehicle.SetInputs(
        nativeVehicle, throttle, brake, steering, handbrake)
    RefreshVehicleTelemetry()

    -- Keep the small prototype laboratory tightly bounded, but authored worlds
    -- are allowed to contain real hills, valleys and kilometres of free-roam
    -- space. The only automatic authored-world rescue is now a genuine deep-fall
    -- failsafe: 500 metres below the configured player spawn height.
    if nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody) then
        local x, y, z = Physics.GetBodyPosition(nativeVehicleBody)
        if playerWorld.loaded then
            local spawnY = playerWorld.spawnPosition[2] or 0.0
            if y < spawnY - 500.0 then
                ResetVehicleAtPlayerWorldSpawn(
                    "Safety-reset after falling 500 m below Player World spawn")
                vehicleMessage =
                    "Safety-reset the vehicle after a 500 m world fall"
            end
        else
            local horizontalLimit = 80.0
            if y < -20.0
                or math.abs(x) > horizontalLimit
                or math.abs(z) > horizontalLimit then
                ResetNativeVehicle()
                vehicleMessage =
                    "Safety-reset the vehicle after it left the prototype test world"
            end
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
