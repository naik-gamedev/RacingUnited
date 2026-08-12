-- SUS03B suspension creator controls. Authoring topology/data provenance is
-- deliberately separate from live spring/damper tuning. Estimated hardpoints
-- are usable engineering inputs, but the UI must never present them as measured.

function DrawVehicleSuspensionAuthoringPanel()
    local assembly = SuspensionAuthoringSelectedAssembly()
    local wheel = PrototypeCarDefinition.wheels[vehicleSuspension.selectedWheel]
    if wheel == nil then
        UI.TextDisabled("No selected wheel definition is available.")
        return
    end

    UI.Text("AUTHORING ASSEMBLY")
    UI.Text(string.format(
        "Corner: %s | axle: %s",
        tostring(wheel.name), tostring(wheel.axle)))

    if assembly then
        UI.Text("Kinematics: " .. tostring(assembly.kinematics))
        UI.Text("Preferred provider: " .. tostring(assembly.preferredProvider or "not specified"))
        UI.Text("Runtime provider: " .. tostring(assembly.runtimeProvider or "linear_raycast_v1"))
        UI.Text("Spring: " .. tostring(assembly.spring))
        UI.Text("Damper: " .. tostring(assembly.damper))
        UI.Text("Anti-roll group: " .. tostring(assembly.antiRollGroup))
        local authored, required = SuspensionAuthoringHardpointReadiness(assembly, wheel)
        UI.Text(string.format("Hardpoints available: %d / %d", authored, required))
        if authored == required and required > 0 then
            UI.TextDisabled("This corner has a complete hardpoint set. Source quality is shown below.")
        elseif wheel.axle == "front" then
            UI.TextDisabled("Missing MacPherson points can be filled by the deterministic assisted-authoring estimate.")
        else
            UI.TextDisabled("Missing trailing-arm points can be filled by the deterministic torsion-bar rear estimate.")
        end
    else
        UI.TextDisabled("No suspension architecture descriptor exists for this axle.")
    end

    UI.Separator()
    UI.Text("ASSISTED AUTHORING")
    UI.TextDisabled("Estimated geometry is intentionally low-confidence and is replaced automatically by measured or GLB-authored points.")
    UI.TextDisabled("Estimate profile: " .. tostring(vehicleSuspensionAuthoring.estimateProfile ~= ""
        and vehicleSuspensionAuthoring.estimateProfile or "not used"))
    UI.TextDisabled(string.format(
        "Native hardpoint kinematics active on %d wheel(s)",
        tonumber(vehicleSuspensionAuthoring.activeHardpointWheelCount) or 0))

    if UI.Button("REBUILD ESTIMATED SUSPENSION HARDPOINTS") then
        if EnsureSuspensionHardpointEstimates(true) then
            ApplySuspensionAuthoringGeometryToNativeVehicle()
            if vehicleSuspensionAuthoring.enabled then
                RefreshSuspensionAuthoringGizmos()
            end
        end
    end
    if UI.Button("IMPORT HARDPOINTS FROM CURRENT GLB") then
        ImportSuspensionHardpointsFromCurrentAsset()
    end

    UI.Separator()
    local changed = false
    vehicleSuspensionAuthoring.enabled, changed = UI.Checkbox(
        "Show suspension authoring gizmos",
        vehicleSuspensionAuthoring.enabled)
    if changed then
        RefreshSuspensionAuthoringGizmos()
    end

    local selectedOnlyChanged = false
    vehicleSuspensionAuthoring.selectedWheelOnly, selectedOnlyChanged = UI.Checkbox(
        "Selected wheel only",
        vehicleSuspensionAuthoring.selectedWheelOnly)
    if selectedOnlyChanged and vehicleSuspensionAuthoring.enabled then
        RefreshSuspensionAuthoringGizmos()
    end

    local scaleChanged = false
    vehicleSuspensionAuthoring.markerScale, scaleChanged = UI.SliderFloat(
        "Marker size",
        vehicleSuspensionAuthoring.markerScale,
        0.015, 0.12, "%.3f m")
    if scaleChanged and vehicleSuspensionAuthoring.enabled then
        RefreshSuspensionAuthoringGizmos()
    end

    if UI.Button("REFRESH GIZMOS") then
        RefreshSuspensionAuthoringGizmos()
    end
    if UI.Button("HIDE + DELETE GIZMOS") then
        vehicleSuspensionAuthoring.enabled = false
        DestroySuspensionAuthoringGizmos()
        vehicleSuspensionAuthoring.message = "Suspension authoring gizmos are hidden"
    end

    UI.Spacing()
    UI.TextDisabled("Marker legend: cyan=wheel centre, orange=bump, blue=droop, yellow=steering axis.")
    UI.TextDisabled("Hardpoints: green=measured, magenta=GLB-authored, orange=estimated, grey=legacy/unknown.")
    UI.TextDisabled(vehicleSuspensionAuthoring.message)

    UI.Separator()
    UI.Text("REQUIRED HARDPOINT IDS")
    if assembly and assembly.requiredHardpoints then
        for _, id in ipairs(assembly.requiredHardpoints) do
            local status, confidence = SuspensionAuthoringHardpointStatus(
                assembly, wheel, id)
            if status == "MISSING" then
                UI.TextDisabled(tostring(id) .. " : MISSING")
            else
                UI.TextDisabled(string.format(
                    "%s : %s | confidence %.0f%%",
                    tostring(id), tostring(status), confidence * 100.0))
            end
        end
    end
end
