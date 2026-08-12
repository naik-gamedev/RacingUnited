-- Compact per-wheel live telemetry.
function DrawVehicleTelemetryPanel()
    SetPrototypeScenePreset("vehicle")
    UI.Text(string.format("Speed %.1f km/h | grounded %d | total 1000 Hz steps %d",
        vehicleSpeed * 3.6, vehicleGroundedWheels, vehicleTotalHighRateSteps))
    if nativeVehicleBody ~= 0 and Physics.BodyExists(nativeVehicleBody) then
        local rotationX, rotationY, rotationZ = Physics.GetBodyRotation(nativeVehicleBody)
        local comX, comY, comZ = Physics.GetBodyCenterOfMassLocal(nativeVehicleBody)
        if rotationX ~= nil then
            UI.Text(string.format("Body pitch / yaw / roll %.2f / %.2f / %.2f deg",
                rotationX, rotationY, rotationZ))
        end
        if comX ~= nil then
            UI.Text(string.format("Physical COM local %.2f / %.2f / %.2f m | %s %.0f%%",
                comX, comY, comZ,
                tostring(PrototypeCarDefinition.chassis.centerOfMassProvenance or "unknown"),
                (PrototypeCarDefinition.chassis.centerOfMassConfidence or 0.0) * 100.0))
        end
        local inertiaPitch, inertiaYaw, inertiaRoll =
            Physics.GetBodyInertiaLocal(nativeVehicleBody)
        if inertiaPitch ~= nil then
            local inertiaOverride = Physics.IsBodyInertiaLocalOverridden(nativeVehicleBody)
            UI.Text(string.format(
                "Inertia pitch / yaw / roll %.0f / %.0f / %.0f kg m^2 | explicit=%s | %s %.0f%%",
                inertiaPitch, inertiaYaw, inertiaRoll,
                tostring(inertiaOverride == true),
                tostring(PrototypeCarDefinition.chassis.massPropertiesProvenance or "unknown"),
                (PrototypeCarDefinition.chassis.massPropertiesConfidence or 0.0) * 100.0))
        end
    end
    if #vehicleWheelTelemetry >= 4 then
        local fl = vehicleWheelTelemetry[1]
        local fr = vehicleWheelTelemetry[2]
        local rl = vehicleWheelTelemetry[3]
        local rr = vehicleWheelTelemetry[4]
        local frontLoad = fl.normalForce + fr.normalForce
        local rearLoad = rl.normalForce + rr.normalForce
        local leftLoad = fl.normalForce + rl.normalForce
        local rightLoad = fr.normalForce + rr.normalForce
        local diagonalFlRr = fl.normalForce + rr.normalForce
        local diagonalFrRl = fr.normalForce + rl.normalForce
        local minimumCompression = math.min(fl.compression, fr.compression, rl.compression, rr.compression)
        local maximumCompression = math.max(fl.compression, fr.compression, rl.compression, rr.compression)
        UI.Text(string.format("Axle load F/R %.0f / %.0f N | side load L/R %.0f / %.0f N",
            frontLoad, rearLoad, leftLoad, rightLoad))
        UI.Text(string.format("Diagonal load FL+RR / FR+RL %.0f / %.0f N | corner travel spread %.1f mm",
            diagonalFlRr, diagonalFrRl,
            (maximumCompression - minimumCompression) * 1000.0))
        local frontBar = vehicleAntiRollBars and vehicleAntiRollBars.front or nil
        local rearBar = vehicleAntiRollBars and vehicleAntiRollBars.rear or nil
        if frontBar ~= nil and rearBar ~= nil then
            UI.Text(string.format("ARB torque front/rear %.1f / %.1f Nm",
                frontBar.torqueNm or 0.0, rearBar.torqueNm or 0.0))
        end
    end
    local flex = vehicleChassisFlexTelemetry or {}
    if flex.enabled then
        UI.Text(string.format(
            "Chassis torsional flex %.4f deg @ %.3f deg/s | structural torque %.0f Nm",
            flex.twistDegrees or 0.0,
            flex.twistRateDegreesPerSecond or 0.0,
            flex.driveTorqueNm or 0.0))
        UI.Text(string.format(
            "Flex sections front/rear %.4f / %.4f deg | rigidity %.0f Nm/deg | %s %.0f%%",
            flex.frontSectionTwistDegrees or 0.0,
            flex.rearSectionTwistDegrees or 0.0,
            flex.torsionalRigidityNmPerDegree or 0.0,
            tostring(PrototypeCarDefinition.chassisFlex
                and PrototypeCarDefinition.chassisFlex.provenance or "unknown"),
            ((PrototypeCarDefinition.chassisFlex
                and PrototypeCarDefinition.chassisFlex.confidence) or 0.0) * 100.0))
    end
    UI.TextDisabled("Pitch, roll and yaw are simultaneous rigid-body motion; diagonal loading is four-corner suspension response, not a fourth rotation axis.")
    UI.Spacing()

    local labels = { "FRONT LEFT", "FRONT RIGHT", "REAR LEFT", "REAR RIGHT" }
    for index, wheel in ipairs(vehicleWheelTelemetry) do
        UI.TextDisabled(labels[index] or ("WHEEL " .. tostring(index)))
        UI.Text(string.format("surface=%s wet=%.0f%% grounded=%s",
            VehicleDetectedSurfaceLabel(wheel), wheel.surfaceWetness * 100.0, tostring(wheel.grounded)))
        UI.Text(string.format("load=%.0f N | Fx/Fy=%.0f / %.0f N | steer=%.1f deg",
            wheel.normalForce, wheel.longitudinalForce, wheel.lateralForce, wheel.steerAngle))
        UI.Text(string.format("spring/damper=%.0f / %.0f N | stops +%.0f / -%.0f N | damper %.0f W",
            wheel.suspensionSpringForce, wheel.suspensionDampingForce,
            wheel.suspensionBumpStopForce, wheel.suspensionDroopStopForce,
            wheel.damperDissipationWatts))
        UI.Text(string.format("slip=%.3f / %.2f deg | grip=%.1f%% | Mz=%.1f Nm",
            wheel.relaxedSlipRatio, wheel.relaxedSlipAngleDegrees,
            wheel.gripUtilization * 100.0, wheel.aligningTorque))
        UI.Spacing()
    end
    UI.TextDisabled(vehicleMessage)
end
