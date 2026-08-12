-- Step 29H: per-wheel tire assignment and native-profile verification.
function DrawVehicleTiresProfilesPanel()
    UI.TextDisabled("PER-WHEEL TIRE PROFILES")
    UI.Text("Each wheel now owns an independent native TireModelDescription.")
    UI.TextDisabled("These two profiles are diagnostic templates, not production tire data.")
    UI.Spacing()

    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        UI.TextDisabled("No native vehicle is available.")
        return
    end

    local wheelNames = { "FL", "FR", "RL", "RR" }
    for index = 1, math.min(Vehicle.GetWheelCount(nativeVehicle), 4) do
        local nominalLoad, peakFriction, longitudinalStiffness, corneringStiffness =
            Vehicle.GetWheelTireModel(nativeVehicle, index)
        local profileName = vehicleWheelTireProfileNames[index] or "unknown"
        UI.Text(string.format(
            "%s: %s | load %.0f N | mu %.2f | Kx %.0f | Ky %.0f",
            wheelNames[index] or ("W" .. tostring(index)),
            profileName,
            nominalLoad or 0.0,
            peakFriction or 0.0,
            longitudinalStiffness or 0.0,
            corneringStiffness or 0.0))
        local imported, fitType, parameterSource, provenance, confidence, mapped, unsupported, measuredSide =
            Vehicle.GetWheelTireParameterInfo(nativeVehicle, index)
        if imported then
            UI.TextDisabled(string.format(
                "    MF%d .tir | confidence %.2f | mapped %d | unsupported %d | measured %s",
                fitType or 0, confidence or 0.0, mapped or 0, unsupported or 0,
                (measuredSide ~= nil and measuredSide ~= "") and measuredSide or "unspecified"))
            UI.TextDisabled("    provenance: " .. tostring(provenance or "unspecified"))
            UI.TextDisabled("    source: " .. tostring(parameterSource or ""))
        end
    end

    UI.Spacing()
    if UI.Button("RESTORE DEFINITION FRONT / REAR PROFILES") then
        ApplyDefinitionTireProfiles()
    end
    if UI.Button("APPLY FRONT TEMPLATE TO ALL WHEELS") then
        ApplyTireProfileToAllWheels("prototype_road_front")
    end
    if UI.Button("APPLY REAR TEMPLATE TO ALL WHEELS") then
        ApplyTireProfileToAllWheels("prototype_road_rear")
    end

    UI.Spacing()
    UI.TextDisabled("The BASIC/CURVE sliders intentionally override every wheel together.")
    UI.TextDisabled("Use RESTORE DEFINITION PROFILES to return to the front/rear assignment.")
end
