-- Creates, resets and destroys the Step 29H prototype vehicle.
function DestroyVehicleDemo()
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.Destroy(nativeVehicle)
    end
    nativeVehicle = 0
    if nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody) then
        Physics.DestroyBody(nativeVehicleBody)
    end
    nativeVehicleBody = 0
    nativeVehicleCollider = 0
    vehicleWheelTelemetry = {}
end

function ResetNativeVehicleAt(x, y, z, message)
    if nativeVehicleBody == 0 or not Physics.BodyExists(nativeVehicleBody) then
        return false
    end
    Physics.SetBodyPosition(nativeVehicleBody, x, y, z)
    Physics.SetBodyRotation(nativeVehicleBody, 0.0, 0.0, 0.0)
    Physics.SetBodyLinearVelocity(nativeVehicleBody, 0.0, 0.0, 0.0)
    Physics.SetBodyAngularVelocity(nativeVehicleBody, 0.0, 0.0, 0.0)
    Physics.ClearBodyForces(nativeVehicleBody)
    Physics.WakeBody(nativeVehicleBody)
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        Vehicle.SetInputs(nativeVehicle, 0.0, 0.0, 0.0, 0.0)
        Vehicle.SetGear(nativeVehicle, 0)
        Vehicle.SetGear(nativeVehicle, 1)
    end
    vehicleMessage = message or "Reset the native vehicle"
    return true
end

function ResetNativeVehicle()
    return ResetNativeVehicleAt(
        PrototypeCarDefinition.resetPosition[1],
        PrototypeCarDefinition.resetPosition[2],
        PrototypeCarDefinition.resetPosition[3],
        "Reset the native vehicle on dry asphalt")
end

function ResetNativeVehicleSplitGrip()
    return ResetNativeVehicleAt(
        0.0,
        PrototypeCarDefinition.resetPosition[2],
        12.0,
        "Split grip: left tires on dry asphalt, right tires on ice")
end

function ResetNativeVehicleSurfaceRunway()
    return ResetNativeVehicleAt(
        0.0,
        PrototypeCarDefinition.resetPosition[2],
        18.5,
        "Surface runway: drive forward through wet asphalt, gravel, dirt, grass, snow and ice")
end

function AddNativeVehicleWheel(
    wheel,
    serviceBrakeFactor,
    handbrakeFactor)
    local physics = PrototypeCarDefinition.wheelPhysics
    return Vehicle.AddWheel(
        nativeVehicle,
        wheel.mount[1], wheel.mount[2], wheel.mount[3],
        0.0, -1.0, 0.0,
        wheel.radiusM or physics.radiusM,
        wheel.restLengthM or physics.restLengthM,
        wheel.maximumCompressionM or physics.maximumCompressionM,
        wheel.maximumDroopM or physics.maximumDroopM,
        wheel.springRateNPerM or physics.springRateNPerM,
        wheel.bumpDampingNsPerM or physics.bumpDampingNsPerM,
        wheel.reboundDampingNsPerM or physics.reboundDampingNsPerM,
        wheel.driveFactor,
        wheel.steerFactor,
        serviceBrakeFactor,
        handbrakeFactor)
end

function CreateNativeVehicleDemo(compiledSourceDefinition)
    DestroyVehicleDemo()
    if playerEntity == 0 or not Entity.Exists(playerEntity) then
        vehicleMessage = "VEHICLE ERROR: Player Vehicle Root is missing"
        return false
    end

    local existingBody = Physics.FindBodyByEntity(playerEntity)
    if existingBody ~= 0 and Physics.BodyExists(existingBody) then
        Physics.DestroyBody(existingBody)
    end

    nativeVehicleBody = Physics.CreateBody(
        playerEntity, "dynamic", PrototypeCarDefinition.chassis.massKg)
    if nativeVehicleBody == 0 then
        vehicleMessage = "VEHICLE ERROR: " .. Physics.GetLastError()
        return false
    end
    Physics.SetBodyGravityFactor(nativeVehicleBody, 1.0)
    Physics.SetBodyLinearDamping(
        nativeVehicleBody, PrototypeCarDefinition.chassis.linearDamping)
    Physics.SetBodyAngularDamping(
        nativeVehicleBody, PrototypeCarDefinition.chassis.angularDamping)
    Physics.SetBodyAllowSleep(nativeVehicleBody, true)

    nativeVehicleCollider = Physics.CreateBoxCollider(
        nativeVehicleBody,
        PrototypeCarDefinition.chassis.halfExtents[1],
        PrototypeCarDefinition.chassis.halfExtents[2],
        PrototypeCarDefinition.chassis.halfExtents[3],
        PrototypeCarDefinition.chassis.colliderOffset[1],
        PrototypeCarDefinition.chassis.colliderOffset[2],
        PrototypeCarDefinition.chassis.colliderOffset[3],
        PrototypeCarDefinition.chassis.friction,
        PrototypeCarDefinition.chassis.restitution,
        false)
    if nativeVehicleCollider == 0 then
        vehicleMessage = "VEHICLE ERROR: " .. Physics.GetLastError()
        return false
    end

    local nativeProvider = "handwritten_prototype"
    local nativeLoadMessage = ""
    if compiledSourceDefinition then
        nativeVehicle, nativeProvider, nativeLoadMessage =
            Vehicle.CreateFromDefinitionV2(
                compiledSourceDefinition,
                nativeVehicleBody,
                vehicleHighRateHertz,
                vehicleMaximumDriveForce,
                vehicleMaximumBrakeForce,
                vehicleMaximumSteerAngle,
                vehicleTireFriction,
                vehicleLateralStiffness,
                vehicleRollingResistance)
    else
        nativeVehicle = Vehicle.Create(
            nativeVehicleBody,
            vehicleHighRateHertz,
            vehicleMaximumDriveForce,
            vehicleMaximumBrakeForce,
            vehicleMaximumSteerAngle,
            vehicleTireFriction,
            vehicleLateralStiffness,
            vehicleRollingResistance)
    end
    if nativeVehicle == 0 then
        vehicleMessage = "VEHICLE ERROR: "
            .. (nativeLoadMessage ~= "" and nativeLoadMessage
                or Vehicle.GetLastError())
        return false
    end

    if not Vehicle.SetSteeringGeometry(
        nativeVehicle,
        vehicleAckermannPercent,
        vehicleSteeringRate,
        vehicleSteeringReturnRate,
        vehicleHighSpeedSteeringRateFactor,
        vehicleHighSpeedReferenceMps) then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    if not Vehicle.SetTireModel(
        nativeVehicle,
        vehicleTire.nominalLoad,
        vehicleTire.peakFriction,
        vehicleTire.longitudinalStiffness,
        vehicleTire.corneringStiffness,
        vehicleTire.loadSensitivity,
        vehicleTire.longitudinalRelaxation,
        vehicleTire.lateralRelaxation,
        vehicleTire.wheelInertia,
        vehicleTire.pneumaticTrail,
        vehicleTire.stiffnessLoadExponent,
        vehicleTire.longitudinalShapeFactor,
        vehicleTire.lateralShapeFactor,
        vehicleTire.longitudinalCurvatureFactor,
        vehicleTire.lateralCurvatureFactor,
        vehicleTire.combinedSlipExponent,
        vehicleTire.pneumaticTrailFalloff) then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    if not Vehicle.SetSurfacePreset(nativeVehicle, vehicleTire.fallbackSurface) then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    if not compiledSourceDefinition then
        if not Vehicle.SetPowertrain(
            nativeVehicle,
            vehicleIdleRpm,
            vehicleRedlineRpm,
            vehicleMaximumEngineTorque,
            vehicleEngineBrakingTorque,
            vehicleFinalDriveRatio,
            vehicleDrivetrainEfficiency,
            vehicleShiftDuration,
            vehicleClutchEngagementRate) then
            vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
            return false
        end

        if not Vehicle.SetGearRatios(
            nativeVehicle,
            vehicleReverseRatio,
            table.unpack(vehicleForwardRatios)) then
            vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
            return false
        end
    end

    if not Vehicle.SetDifferential(
        nativeVehicle,
        vehicleDifferentialMode,
        vehicleDifferentialBias) then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    if not Vehicle.SetDriverAids(
        nativeVehicle,
        vehicleDriverAids.absEnabled,
        vehicleDriverAids.tractionControlEnabled,
        vehicleDriverAids.absTargetSlip,
        vehicleDriverAids.tractionTargetSlip,
        vehicleDriverAids.minimumSpeed,
        vehicleDriverAids.modulationRate,
        vehicleDriverAids.maximumHandbrakeTorque) then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
        return false
    end

    if not compiledSourceDefinition then
        local frontBrakePerWheel = vehicleDriverAids.frontBrakeBias * 0.5
        local rearBrakePerWheel = (1.0 - vehicleDriverAids.frontBrakeBias) * 0.5
        for _, wheel in ipairs(PrototypeCarDefinition.wheels) do
            local serviceBrakeFactor = wheel.axle == "front"
                and frontBrakePerWheel or rearBrakePerWheel
            local handbrakeFactor = wheel.axle == "rear" and 0.5 or 0.0
            if not AddNativeVehicleWheel(
                wheel,
                serviceBrakeFactor,
                handbrakeFactor) then
                vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
                return false
            end
        end
    end

    if not ApplyDefinitionTireProfiles() then
        return false
    end

    ResetNativeVehicle()
    vehicleMessage = compiledSourceDefinition
        and ("Step 29L native definition loaded: " .. nativeProvider)
        or "Step 29J.1 online: Peugeot-reference wheel centers + articulated wheel presentation"
    return true
end
