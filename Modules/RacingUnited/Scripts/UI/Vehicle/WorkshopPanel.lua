-- First usable front-end for the versioned topology-first vehicle contract.
local function WorkshopColumnWidth()
    return math.max(100.0, (UI.GetAvailableWidth() - 10.0) * 0.5)
end

local function WorkshopThreeColumnWidth()
    return math.max(80.0, (UI.GetAvailableWidth() - 20.0) / 3.0)
end

local function WorkshopTemplateButton(templateId, width)
    local template = VehicleDefinitionV2.templates[templateId]
    if UI.Button(template.label, width, 38.0, false) then
        SelectVehicleWorkshopTemplate(templateId)
    end
end

local function WorkshopTemplateRow(firstId, secondId)
    local width = WorkshopColumnWidth()
    WorkshopTemplateButton(firstId, width)
    UI.SameLine()
    WorkshopTemplateButton(secondId, width)
end

local function WorkshopTemplateThreeColumnRow(firstId, secondId, thirdId)
    local width = WorkshopThreeColumnWidth()
    WorkshopTemplateButton(firstId, width)
    UI.SameLine()
    WorkshopTemplateButton(secondId, width)
    UI.SameLine()
    WorkshopTemplateButton(thirdId, width)
end

local function DrawWorkshopTemplateGrid()
    local availableWidth = UI.GetAvailableWidth()
    if availableWidth >= 540.0 then
        WorkshopTemplateThreeColumnRow("road_car", "formula", "indycar")
        WorkshopTemplateThreeColumnRow("kart", "sprint_car", "atv")
        WorkshopTemplateThreeColumnRow(
            "motorcycle", "truck", "twin_engine")
        WorkshopTemplateButton("custom", UI.GetAvailableWidth())
        return
    end

    WorkshopTemplateRow("road_car", "formula")
    WorkshopTemplateRow("indycar", "kart")
    WorkshopTemplateRow("sprint_car", "atv")
    WorkshopTemplateRow("motorcycle", "truck")
    WorkshopTemplateRow("twin_engine", "custom")
end

local function WorkshopFieldChanged(changed)
    if changed then
        RefreshVehicleWorkshopDefinition()
        SaveVehicleWorkshopDraft()
    end
end

function DrawVehicleWorkshopPanel()
    SetPrototypeScenePreset("visual")
    UI.TextWrapped("Build vehicles from bodies, power units, transmissions and contact units. Categories are editor templates only: unusual machinery remains an arrangement of components rather than a hard-coded vehicle species.")
    UI.Spacing()

    UI.TextDisabled("STARTING TOPOLOGY")
    DrawWorkshopTemplateGrid()

    local draft = vehicleWorkshop.draft
    local changed = false
    draft.id, changed = UI.InputText("Definition ID", draft.id, 96)
    WorkshopFieldChanged(changed)
    draft.displayName, changed = UI.InputText(
        "Display name", draft.displayName, 160)
    WorkshopFieldChanged(changed)
    draft.classification, changed = UI.InputText(
        "Classification metadata", draft.classification, 64)
    WorkshopFieldChanged(changed)

    UI.Spacing()
    UI.TextDisabled("MODULE-OWNED VISUAL ASSET")
    draft.bodyAsset, changed = UI.InputText(
        "Assets-relative OBJ", draft.bodyAsset, 512)
    WorkshopFieldChanged(changed)
    local columnWidth = WorkshopColumnWidth()
    if UI.Button("SELECT OBJ FROM MODULE ASSETS", columnWidth, 38.0, false) then
        SelectVehicleWorkshopBodyAsset()
    end
    UI.SameLine()
    if UI.Button("USE PLAYER CAR SLOT", columnWidth, 38.0, false) then
        draft.bodyAsset = "Vehicles/Player/PlayerCar.obj"
        RefreshVehicleWorkshopDefinition()
        SaveVehicleWorkshopDraft()
    end
    UI.TextDisabled(Module.AssetExists(draft.bodyAsset)
        and "Asset found; authored scale remains 1 Blender unit = 1 metre"
        or "Asset missing; copy it beneath Modules/RacingUnited/Assets first")

    UI.Spacing()
    UI.TextDisabled("TOPOLOGY")
    draft.massKg, changed = UI.SliderFloat(
        "Primary body mass", draft.massKg, 1.0, 30000.0, "%.1f kg")
    WorkshopFieldChanged(changed)
    draft.maximumTorqueNm, changed = UI.SliderFloat(
        "Torque per power unit", draft.maximumTorqueNm,
        0.0, 5000.0, "%.1f Nm")
    WorkshopFieldChanged(changed)
    draft.bodyCount, changed = UI.InputInt("Rigid bodies", draft.bodyCount, 1)
    WorkshopFieldChanged(changed)
    draft.powerUnitCount, changed = UI.InputInt(
        "Power units", draft.powerUnitCount, 1)
    WorkshopFieldChanged(changed)
    draft.transmissionCount, changed = UI.InputInt(
        "Transmissions", draft.transmissionCount, 1)
    WorkshopFieldChanged(changed)
    draft.contactUnitCount, changed = UI.InputInt(
        "Wheel / track contact units", draft.contactUnitCount, 1)
    WorkshopFieldChanged(changed)
    draft.forwardGearCount, changed = UI.InputInt(
        "Forward ratios per transmission", draft.forwardGearCount, 1)
    WorkshopFieldChanged(changed)

    UI.Text("Driven topology: " .. string.upper(draft.driveLayout))
    columnWidth = WorkshopColumnWidth()
    if UI.Button("FWD", columnWidth, 38.0, false) then
        SetVehicleWorkshopDriveLayout("fwd")
    end
    UI.SameLine()
    if UI.Button("RWD", columnWidth, 38.0, false) then
        SetVehicleWorkshopDriveLayout("rwd")
    end
    if UI.Button("AWD", columnWidth, 38.0, false) then
        SetVehicleWorkshopDriveLayout("awd")
    end
    UI.SameLine()
    if UI.Button("SPLIT POWERTRAINS", columnWidth, 38.0, false) then
        SetVehicleWorkshopDriveLayout("split")
    end

    UI.Text("Power-unit placement: " .. string.upper(draft.engineLocation))
    columnWidth = WorkshopColumnWidth()
    if UI.Button("FRONT", columnWidth, 38.0, false) then
        SetVehicleWorkshopEngineLocation("front")
    end
    UI.SameLine()
    if UI.Button("MID", columnWidth, 38.0, false) then
        SetVehicleWorkshopEngineLocation("mid")
    end
    if UI.Button("REAR", columnWidth, 38.0, false) then
        SetVehicleWorkshopEngineLocation("rear")
    end
    UI.SameLine()
    if UI.Button("DISTRIBUTED", columnWidth, 38.0, false) then
        SetVehicleWorkshopEngineLocation("distributed")
    end

    draft.requiresLeanDynamics, changed = UI.Checkbox(
        "Requires lean / large-camber dynamics", draft.requiresLeanDynamics)
    WorkshopFieldChanged(changed)
    draft.requiresArticulation, changed = UI.Checkbox(
        "Requires articulated bodies", draft.requiresArticulation)
    WorkshopFieldChanged(changed)
    draft.requiresTrackContacts, changed = UI.Checkbox(
        "Requires continuous track contacts", draft.requiresTrackContacts)
    WorkshopFieldChanged(changed)

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("VALIDATION + OUTPUT")
    local report = RefreshVehicleWorkshopDefinition()
    UI.Text(report.summary)
    if report.valid and report.currentSolverReady then
        UI.TextColored(
            "VALID + LIVE PREVIEW READY",
            0.30, 0.95, 0.48, 1.0)
    elseif report.valid then
        UI.TextColored(
            "VALID DEFINITION + FUTURE NATIVE PROVIDERS REQUIRED",
            1.0, 0.72, 0.22, 1.0)
    else
        UI.TextColored(
            "DEFINITION HAS STRUCTURAL ERRORS",
            1.0, 0.32, 0.32, 1.0)
    end
    UI.Text(string.format(
        "%d errors | %d warnings", report.errorCount, report.warningCount))
    for _, issue in ipairs(report.issues) do
        if issue.severity == "error" then
            UI.TextColored("ERROR [" .. issue.code .. "] " .. issue.message,
                1.0, 0.32, 0.32, 1.0)
        else
            UI.TextWrapped("WARNING [" .. issue.code .. "] " .. issue.message)
        end
    end

    if UI.Button("VALIDATE", UI.GetAvailableWidth(), 38.0, false) then
        RefreshVehicleWorkshopDefinition()
        vehicleWorkshop.message = "Validation refreshed"
    end
    columnWidth = WorkshopColumnWidth()
    if UI.Button("APPLY LIVE PROTOTYPE PREVIEW", columnWidth, 38.0, false) then
        ApplyVehicleWorkshopPreview()
    end
    UI.SameLine()
    if UI.Button("EXPORT VERSIONED DEFINITION", columnWidth, 38.0, false) then
        ExportVehicleWorkshopDefinition()
    end
    UI.TextWrapped(vehicleWorkshop.message)
    if vehicleWorkshop.lastExportPath ~= "" then
        UI.TextDisabled("Export: " .. vehicleWorkshop.lastExportPath)
    end
    UI.TextDisabled("A valid definition may intentionally be newer than the current solver. Export preserves that topology instead of silently pretending unsupported components work.")
end
