-- Step 29Q: live suspension, upright and terrain-contact diagnostics.
local function DrawSuspensionWheelSelector()
    local changed = false
    local requested = vehicleSuspension.selectedWheel
    requested, changed = UI.InputInt("Wheel (1-based)", requested, 1)
    if changed then
        local wheelCount = nativeVehicle ~= 0
            and Vehicle.GetWheelCount(nativeVehicle) or 1
        requested = math.max(1, math.min(wheelCount, requested))
        ReadVehicleSuspensionModel(requested)
        if vehicleSuspensionAuthoring.enabled then
            RefreshSuspensionAuthoringGizmos()
        end
    end
    UI.TextDisabled("Provider: " .. tostring(vehicleSuspension.provider))
end

local function ApplySelectedSuspensionWhenChanged(changed)
    if changed and ApplyVehicleSuspensionModel(
            vehicleSuspension.selectedWheel) then
        vehicleMessage = "Updated suspension on wheel "
            .. tostring(vehicleSuspension.selectedWheel)
    end
end

local function DrawSuspensionSpringControls()
    local changed, fieldChanged = false, false
    vehicleSuspension.springPreloadN, fieldChanged = UI.SliderFloat(
        "Spring preload", vehicleSuspension.springPreloadN,
        0.0, 10000.0, "%.0f N")
    changed = changed or fieldChanged
    vehicleSuspension.springRateNPerM, fieldChanged = UI.SliderFloat(
        "Linear spring rate", vehicleSuspension.springRateNPerM,
        1000.0, 200000.0, "%.0f N/m")
    changed = changed or fieldChanged
    vehicleSuspension.springProgressionNPerM2, fieldChanged = UI.SliderFloat(
        "Spring progression", vehicleSuspension.springProgressionNPerM2,
        0.0, 2000000.0, "%.0f N/m^2")
    changed = changed or fieldChanged
    vehicleSuspension.motionRatio, fieldChanged = UI.SliderFloat(
        "Motion ratio", vehicleSuspension.motionRatio,
        0.10, 3.00, "%.3f")
    changed = changed or fieldChanged
    vehicleSuspension.maximumForceN, fieldChanged = UI.SliderFloat(
        "Safety force limit", vehicleSuspension.maximumForceN,
        10000.0, 1000000.0, "%.0f N")
    changed = changed or fieldChanged
    ApplySelectedSuspensionWhenChanged(changed)
    UI.TextDisabled("Motion ratio maps wheel travel and speed into spring/damper shaft motion.")
end

local function DrawSuspensionDamperControls()
    local changed, fieldChanged = false, false
    vehicleSuspension.bumpDampingNsPerM, fieldChanged = UI.SliderFloat(
        "Low-speed bump", vehicleSuspension.bumpDampingNsPerM,
        0.0, 20000.0, "%.0f N s/m")
    changed = changed or fieldChanged
    vehicleSuspension.bumpHighSpeedDampingNsPerM, fieldChanged = UI.SliderFloat(
        "High-speed bump", vehicleSuspension.bumpHighSpeedDampingNsPerM,
        0.0, 20000.0, "%.0f N s/m")
    changed = changed or fieldChanged
    vehicleSuspension.bumpDampingKneeVelocityMps, fieldChanged = UI.SliderFloat(
        "Bump knee velocity", vehicleSuspension.bumpDampingKneeVelocityMps,
        0.01, 5.0, "%.3f m/s")
    changed = changed or fieldChanged
    vehicleSuspension.reboundDampingNsPerM, fieldChanged = UI.SliderFloat(
        "Low-speed rebound", vehicleSuspension.reboundDampingNsPerM,
        0.0, 20000.0, "%.0f N s/m")
    changed = changed or fieldChanged
    vehicleSuspension.reboundHighSpeedDampingNsPerM, fieldChanged = UI.SliderFloat(
        "High-speed rebound", vehicleSuspension.reboundHighSpeedDampingNsPerM,
        0.0, 20000.0, "%.0f N s/m")
    changed = changed or fieldChanged
    vehicleSuspension.reboundDampingKneeVelocityMps, fieldChanged = UI.SliderFloat(
        "Rebound knee velocity",
        vehicleSuspension.reboundDampingKneeVelocityMps,
        0.01, 5.0, "%.3f m/s")
    changed = changed or fieldChanged
    ApplySelectedSuspensionWhenChanged(changed)
    UI.TextDisabled("Separate slopes reproduce digressive shim-stack behavior without a vendor SDK.")
end

local function DrawSuspensionTravelControls()
    local changed, fieldChanged = false, false
    vehicleSuspension.bumpStopEngagementM, fieldChanged = UI.SliderFloat(
        "Bump-stop engagement", vehicleSuspension.bumpStopEngagementM,
        0.0, 0.50, "%.4f m")
    changed = changed or fieldChanged
    vehicleSuspension.bumpStopRateNPerM, fieldChanged = UI.SliderFloat(
        "Bump-stop rate", vehicleSuspension.bumpStopRateNPerM,
        0.0, 1000000.0, "%.0f N/m")
    changed = changed or fieldChanged
    vehicleSuspension.bumpStopProgressionNPerM2, fieldChanged = UI.SliderFloat(
        "Bump-stop progression", vehicleSuspension.bumpStopProgressionNPerM2,
        0.0, 10000000.0, "%.0f N/m^2")
    changed = changed or fieldChanged
    vehicleSuspension.droopStopEngagementM, fieldChanged = UI.SliderFloat(
        "Droop-stop engagement", vehicleSuspension.droopStopEngagementM,
        0.0, 0.50, "%.4f m")
    changed = changed or fieldChanged
    vehicleSuspension.droopStopRateNPerM, fieldChanged = UI.SliderFloat(
        "Droop-stop rate", vehicleSuspension.droopStopRateNPerM,
        0.0, 1000000.0, "%.0f N/m")
    changed = changed or fieldChanged
    ApplySelectedSuspensionWhenChanged(changed)
    UI.TextDisabled("Travel stops engage progressively before the raycast wheel reaches hard travel limits.")
end

local function DrawSuspensionUnsprungControls()
    local changed, fieldChanged = false, false
    vehicleSuspension.effectiveUnsprungMassKg, fieldChanged = UI.SliderFloat(
        "Effective unsprung mass",
        vehicleSuspension.effectiveUnsprungMassKg,
        0.0, 1000.0, "%.2f kg")
    changed = changed or fieldChanged
    vehicleSuspension.tireRadialStiffnessNPerM, fieldChanged = UI.SliderFloat(
        "Tire radial stiffness",
        vehicleSuspension.tireRadialStiffnessNPerM,
        10000.0, 2000000.0, "%.0f N/m")
    changed = changed or fieldChanged
    vehicleSuspension.tireRadialDampingNsPerM, fieldChanged = UI.SliderFloat(
        "Tire radial damping",
        vehicleSuspension.tireRadialDampingNsPerM,
        0.0, 20000.0, "%.0f N s/m")
    changed = changed or fieldChanged
    vehicleSuspension.maximumTireDeflectionM, fieldChanged = UI.SliderFloat(
        "Maximum tire deflection",
        vehicleSuspension.maximumTireDeflectionM,
        0.005, 0.30, "%.4f m")
    changed = changed or fieldChanged
    vehicleSuspension.maximumTireNormalForceN, fieldChanged = UI.SliderFloat(
        "Maximum tire normal force",
        vehicleSuspension.maximumTireNormalForceN,
        10000.0, 1000000.0, "%.0f N")
    changed = changed or fieldChanged
    ApplySelectedSuspensionWhenChanged(changed)
    UI.TextDisabled("Zero mass restores the legacy massless raycast wheel for compatibility or low-cost simulation.")
end

local function DrawSuspensionGeometryControls()
    local changed, fieldChanged = false, false
    vehicleSuspension.steeringAxisX, fieldChanged = UI.SliderFloat(
        "Steering axis X", vehicleSuspension.steeringAxisX,
        -1.0, 1.0, "%.4f")
    changed = changed or fieldChanged
    vehicleSuspension.steeringAxisY, fieldChanged = UI.SliderFloat(
        "Steering axis Y", vehicleSuspension.steeringAxisY,
        -1.0, 1.0, "%.4f")
    changed = changed or fieldChanged
    vehicleSuspension.steeringAxisZ, fieldChanged = UI.SliderFloat(
        "Steering axis Z", vehicleSuspension.steeringAxisZ,
        -1.0, 1.0, "%.4f")
    changed = changed or fieldChanged
    vehicleSuspension.staticCamberDegrees, fieldChanged = UI.SliderFloat(
        "Static local camber", vehicleSuspension.staticCamberDegrees,
        -15.0, 15.0, "%.3f deg")
    changed = changed or fieldChanged
    vehicleSuspension.camberGainDegreesPerM, fieldChanged = UI.SliderFloat(
        "Camber gain", vehicleSuspension.camberGainDegreesPerM,
        -100.0, 100.0, "%.3f deg/m")
    changed = changed or fieldChanged
    vehicleSuspension.camberProgressionDegreesPerM2, fieldChanged =
        UI.SliderFloat(
            "Camber progression",
            vehicleSuspension.camberProgressionDegreesPerM2,
            -1000.0, 1000.0, "%.3f deg/m^2")
    changed = changed or fieldChanged
    vehicleSuspension.staticToeDegrees, fieldChanged = UI.SliderFloat(
        "Static local toe", vehicleSuspension.staticToeDegrees,
        -10.0, 10.0, "%.3f deg")
    changed = changed or fieldChanged
    vehicleSuspension.toeGainDegreesPerM, fieldChanged = UI.SliderFloat(
        "Toe gain / bump steer", vehicleSuspension.toeGainDegreesPerM,
        -100.0, 100.0, "%.3f deg/m")
    changed = changed or fieldChanged
    vehicleSuspension.toeProgressionDegreesPerM2, fieldChanged =
        UI.SliderFloat(
            "Toe progression",
            vehicleSuspension.toeProgressionDegreesPerM2,
            -1000.0, 1000.0, "%.3f deg/m^2")
    changed = changed or fieldChanged
    ApplySelectedSuspensionWhenChanged(changed)
    UI.TextDisabled("The linear provider uses signed local curves; hardpoint providers derive alignment and motion ratio from linkage geometry.")
end

local function DrawSuspensionLiveTelemetry()
    local wheel = vehicleWheelTelemetry[vehicleSuspension.selectedWheel]
    if wheel == nil then
        UI.TextDisabled("No live wheel telemetry is available yet.")
        return
    end
    UI.Text(string.format("Compression / velocity: %.4f m / %.4f m/s",
        wheel.compression, wheel.compressionVelocity))
    UI.Text(string.format("Spring / damper: %.0f N / %.0f N",
        wheel.suspensionSpringForce, wheel.suspensionDampingForce))
    UI.Text(string.format("Bump stop / droop stop: %.0f N / %.0f N",
        wheel.suspensionBumpStopForce, wheel.suspensionDroopStopForce))
    UI.Text(string.format("Unclamped / contact load: %.0f N / %.0f N",
        wheel.suspensionUnclampedForce, wheel.normalForce))
    UI.Text(string.format("Contact: %s | losses: %d",
        wheel.contactStatus, wheel.contactLossTransitions))
    UI.Text(string.format("Ray candidates / exact / triangles: %d / %d / %d",
        wheel.rayCandidates, wheel.rayExactTests,
        wheel.staticTriangleCandidates))
    if wheel.suspensionBottomed then
        UI.Text(string.format("Bottom-out penetration: %.4f m",
            wheel.bottomOutPenetration))
    end
    UI.TextDisabled(string.format(
        "Static scene: %s | inside XZ: %s | ray overlaps: %s | static hit: %s",
        tostring(wheel.staticSceneLoaded),
        tostring(wheel.originInsideStaticSceneBounds),
        tostring(wheel.rayBoundsOverlapStaticScene),
        tostring(wheel.selectedHitWasStaticTriangle)))
    UI.Text(string.format("Damper heat generation: %.1f W",
        wheel.damperDissipationWatts))
    UI.Text(string.format("Unsprung velocity: %.4f m/s",
        wheel.unsprungVelocity))
    UI.Text(string.format("Tire deflection / velocity: %.4f m / %.4f m/s",
        wheel.tireDeflection, wheel.tireDeflectionVelocity))
    UI.Text(string.format("Tire radial heat generation: %.1f W",
        wheel.tireRadialDissipationWatts))
    UI.Text(string.format("Camber / toe (workshop): %.3f deg / %.3f deg",
        wheel.workshopCamberDegrees or wheel.camberAngleDegrees,
        wheel.workshopToeDegrees or wheel.toeAngleDegrees))
    UI.TextDisabled(string.format(
        "Native mirrored local rotations: %.3f / %.3f deg",
        wheel.camberAngleDegrees, wheel.toeAngleDegrees))
    UI.Text(string.format("Native upright XYZ: %.3f / %.3f / %.3f deg",
        wheel.uprightRotationX, wheel.uprightRotationY,
        wheel.uprightRotationZ))
    UI.TextDisabled("Damper watts will later feed oil/gas temperature, fade, cavitation and wear.")
end

local function DrawStaticRideHeightCalibration()
    if vehicleRideHeight == nil or not vehicleRideHeight.valid then
        UI.TextDisabled(vehicleRideHeight ~= nil
            and vehicleRideHeight.message
            or "Static ride height has not been calibrated.")
        return
    end

    UI.Text(vehicleRideHeight.message)
    UI.TextDisabled(
        "Spring/torsion preload sets static height; dampers carry no load at rest.")
    for _, corner in ipairs(vehicleRideHeight.corners) do
        UI.Text(string.format(
            "%s | load %.0f N | preload %.0f N | tire %.1f mm | travel %.1f mm",
            tostring(corner.wheel_name),
            corner.supported_load_n,
            corner.spring_preload_n,
            corner.static_tire_deflection_m * 1000.0,
            corner.target_compression_m * 1000.0))
        UI.TextDisabled("  " .. tostring(corner.provider)
            .. " | " .. tostring(corner.diagnostic))
    end

    local geometry = vehicleRideHeight.assetGeometry
    if geometry ~= nil and geometry.valid then
        UI.Separator()
        UI.Text(string.format(
            "Authored chassis minima: front %.1f mm | rear %.1f mm",
            geometry.front_clearance_m * 1000.0,
            geometry.rear_clearance_m * 1000.0))
        UI.TextDisabled(string.format(
            "GLB road plane Y %.4f m | %d body nodes / %d tire nodes",
            geometry.ground_plane_y,
            geometry.body_node_count,
            geometry.tire_node_count))
    else
        UI.TextDisabled(
            "Load the Peugeot GLB to inspect front/rear underside clearances.")
    end
end

function DrawVehicleSuspensionPanel()
    SetPrototypeScenePreset("vehicle")
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        UI.TextDisabled("Create the player vehicle before tuning suspension.")
        return
    end

    DrawSuspensionWheelSelector()
    if UI.Button("READ NATIVE WHEEL") then
        ReadVehicleSuspensionModel(vehicleSuspension.selectedWheel)
        vehicleMessage = "Read native suspension wheel "
            .. tostring(vehicleSuspension.selectedWheel)
    end
    if UI.Button("COPY SELECTED TUNE TO ALL WHEELS") then
        ApplyVehicleSuspensionModelToAllWheels()
    end
    if UI.Button("RESTORE PROTOTYPE SUSPENSION") then
        RestoreVehicleSuspensionDefinition()
    end
    UI.Separator()

    if UI.BeginTabBar("VehicleSuspensionTabs") then
        if UI.BeginTabItem("SPRINGS") then
            DrawSuspensionSpringControls()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("DAMPERS") then
            DrawSuspensionDamperControls()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("TRAVEL") then
            DrawSuspensionTravelControls()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("UNSPRUNG") then
            DrawSuspensionUnsprungControls()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("GEOMETRY") then
            DrawSuspensionGeometryControls()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("AUTHORING") then
            DrawVehicleSuspensionAuthoringPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("LIVE") then
            DrawSuspensionLiveTelemetry()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("RIDE HEIGHT") then
            DrawStaticRideHeightCalibration()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
    UI.Spacing()
    UI.TextDisabled(vehicleMessage)
end
