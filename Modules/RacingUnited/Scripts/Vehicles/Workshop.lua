-- Persistent authoring state and the current-solver preview bridge.
local function ClampWorkshopInteger(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, math.floor(value)))
end

local function LoadVehicleWorkshopDraft()
    local templateId = Save.GetString("vehicle_workshop.template", "road_car")
    local draft = CreateVehicleWorkshopDraft(templateId)
    draft.id = Save.GetString("vehicle_workshop.id", draft.id)
    draft.displayName = Save.GetString(
        "vehicle_workshop.display_name", draft.displayName)
    draft.bodyAsset = Save.GetString(
        "vehicle_workshop.body_asset", draft.bodyAsset)
    draft.classification = Save.GetString(
        "vehicle_workshop.classification", draft.classification)
    draft.massKg = Save.GetNumber("vehicle_workshop.mass_kg", draft.massKg)
    draft.bodyCount = Save.GetInt("vehicle_workshop.body_count", draft.bodyCount)
    draft.powerUnitCount = Save.GetInt(
        "vehicle_workshop.power_unit_count", draft.powerUnitCount)
    draft.transmissionCount = Save.GetInt(
        "vehicle_workshop.transmission_count", draft.transmissionCount)
    draft.contactUnitCount = Save.GetInt(
        "vehicle_workshop.contact_count", draft.contactUnitCount)
    draft.forwardGearCount = Save.GetInt(
        "vehicle_workshop.forward_gears", draft.forwardGearCount)
    draft.driveLayout = Save.GetString(
        "vehicle_workshop.drive_layout", draft.driveLayout)
    draft.engineLocation = Save.GetString(
        "vehicle_workshop.engine_location", draft.engineLocation)
    draft.maximumTorqueNm = Save.GetNumber(
        "vehicle_workshop.maximum_torque_nm", draft.maximumTorqueNm)
    draft.requiresLeanDynamics = Save.GetBool(
        "vehicle_workshop.requires_lean", draft.requiresLeanDynamics == true)
    draft.requiresArticulation = Save.GetBool(
        "vehicle_workshop.requires_articulation", false)
    draft.requiresTrackContacts = Save.GetBool(
        "vehicle_workshop.requires_tracks", false)
    return draft
end

vehicleWorkshop = {
    draft = LoadVehicleWorkshopDraft(),
    definition = nil,
    report = nil,
    message = "Choose a topology template, validate it, then preview supported configurations",
    lastExportPath = ""
}

function SaveVehicleWorkshopDraft()
    local draft = vehicleWorkshop.draft
    Save.SetString("vehicle_workshop.template", draft.templateId)
    Save.SetString("vehicle_workshop.id", draft.id)
    Save.SetString("vehicle_workshop.display_name", draft.displayName)
    Save.SetString("vehicle_workshop.body_asset", draft.bodyAsset)
    Save.SetString("vehicle_workshop.classification", draft.classification)
    Save.SetNumber("vehicle_workshop.mass_kg", draft.massKg)
    Save.SetInt("vehicle_workshop.body_count", draft.bodyCount)
    Save.SetInt("vehicle_workshop.power_unit_count", draft.powerUnitCount)
    Save.SetInt("vehicle_workshop.transmission_count", draft.transmissionCount)
    Save.SetInt("vehicle_workshop.contact_count", draft.contactUnitCount)
    Save.SetInt("vehicle_workshop.forward_gears", draft.forwardGearCount)
    Save.SetString("vehicle_workshop.drive_layout", draft.driveLayout)
    Save.SetString("vehicle_workshop.engine_location", draft.engineLocation)
    Save.SetNumber("vehicle_workshop.maximum_torque_nm", draft.maximumTorqueNm)
    Save.SetBool("vehicle_workshop.requires_lean", draft.requiresLeanDynamics)
    Save.SetBool("vehicle_workshop.requires_articulation", draft.requiresArticulation)
    Save.SetBool("vehicle_workshop.requires_tracks", draft.requiresTrackContacts)
end

function RefreshVehicleWorkshopDefinition()
    local draft = vehicleWorkshop.draft
    draft.bodyCount = ClampWorkshopInteger(draft.bodyCount, 0, 16)
    draft.powerUnitCount = ClampWorkshopInteger(draft.powerUnitCount, 0, 8)
    draft.transmissionCount = ClampWorkshopInteger(draft.transmissionCount, 0, 8)
    draft.contactUnitCount = ClampWorkshopInteger(draft.contactUnitCount, 0, 32)
    draft.forwardGearCount = ClampWorkshopInteger(draft.forwardGearCount, 0, 32)
    draft.massKg = math.max(1.0, math.min(100000.0, draft.massKg))
    draft.maximumTorqueNm = math.max(
        0.0, math.min(100000.0, draft.maximumTorqueNm))
    vehicleWorkshop.definition = BuildVehicleDefinitionV2(draft)
    vehicleWorkshop.report = ValidateVehicleDefinitionV2(
        vehicleWorkshop.definition)
    local report = vehicleWorkshop.report
    report.nativeValid = report.valid
    report.nativeSolverReady = report.currentSolverReady
    report.nativeProvider = "lua_test_fallback"
    report.nativeSummary = report.summary
    report.nativeIssues = ""
    if Vehicle and Vehicle.CompileDefinitionV2 then
        report.nativeValid,
            report.nativeSolverReady,
            report.nativeProvider,
            report.nativeSummary,
            report.nativeIssues = Vehicle.CompileDefinitionV2(
                vehicleWorkshop.definition)
        if not report.nativeValid then
            table.insert(report.issues, {
                severity = "error",
                code = "native_compiler",
                message = report.nativeIssues ~= "" and report.nativeIssues
                    or "Native compiler rejected the definition"
            })
            report.errorCount = report.errorCount + 1
        elseif report.currentSolverReady and not report.nativeSolverReady then
            table.insert(report.issues, {
                severity = "warning",
                code = "native_provider_mismatch",
                message = report.nativeIssues ~= "" and report.nativeIssues
                    or "Native runtime provider is not available"
            })
            report.warningCount = report.warningCount + 1
        end
        report.valid = report.valid and report.nativeValid
        report.currentSolverReady = report.valid
            and report.nativeSolverReady
        report.summary = report.nativeSummary
    end
    return report
end

function SelectVehicleWorkshopTemplate(templateId)
    local previousAsset = vehicleWorkshop.draft.bodyAsset
    vehicleWorkshop.draft = CreateVehicleWorkshopDraft(templateId)
    vehicleWorkshop.draft.bodyAsset = previousAsset
    RefreshVehicleWorkshopDefinition()
    SaveVehicleWorkshopDraft()
    vehicleWorkshop.message = VehicleDefinitionV2.templates[templateId].label
        .. " topology loaded; this is an editable starting point, not hard-coded physics"
end

function SetVehicleWorkshopDriveLayout(layout)
    vehicleWorkshop.draft.driveLayout = layout
    RefreshVehicleWorkshopDefinition()
    SaveVehicleWorkshopDraft()
end

function SetVehicleWorkshopEngineLocation(location)
    vehicleWorkshop.draft.engineLocation = location
    RefreshVehicleWorkshopDefinition()
    SaveVehicleWorkshopDraft()
end

function SelectVehicleWorkshopBodyAsset()
    local selected, errorText = Module.SelectAssetFile()
    if selected == nil then
        vehicleWorkshop.message = errorText or "Asset selection cancelled"
        return
    end
    vehicleWorkshop.draft.bodyAsset = selected
    RefreshVehicleWorkshopDefinition()
    SaveVehicleWorkshopDraft()
    vehicleWorkshop.message = "Selected module asset: " .. selected
end

function ApplyVehicleWorkshopPreview()
    local report = RefreshVehicleWorkshopDefinition()
    if not report.valid then
        vehicleWorkshop.message = "Cannot preview: repair the definition errors first"
        return false
    end
    if not report.currentSolverReady then
        vehicleWorkshop.message =
            "Definition retained, but its components need a later native solver provider"
        return false
    end
    if not Module.AssetExists(vehicleWorkshop.draft.bodyAsset) then
        vehicleWorkshop.message =
            "Cannot preview: choose an OBJ already inside this module's Assets folder"
        return false
    end
    local definition = vehicleWorkshop.definition
    local transmission = definition.transmissions[1]
    PrototypeCarDefinition.schemaVersion = 2
    PrototypeCarDefinition.classification = definition.classification
    PrototypeCarDefinition.chassis.massKg = vehicleWorkshop.draft.massKg
    PrototypeCarDefinition.visual.bodyAsset = vehicleWorkshop.draft.bodyAsset
    PrototypeCarDefinition.powertrain.maximumEngineTorque =
        vehicleWorkshop.draft.maximumTorqueNm
    PrototypeCarDefinition.powertrain.forwardRatios = {}
    vehicleForwardRatios = {}
    for index, ratio in ipairs(transmission.forwardRatios) do
        PrototypeCarDefinition.powertrain.forwardRatios[index] = ratio
        vehicleForwardRatios[index] = ratio
    end
    vehicleMaximumEngineTorque = vehicleWorkshop.draft.maximumTorqueNm
    vehicleForwardGearCount = #vehicleForwardRatios
    vehicleSelectedGearRatio = vehicleForwardRatios[1]

    if not CreateNativeVehicleDemo(definition) then
        vehicleWorkshop.message = "Native preview failed: " .. vehicleMessage
        return false
    end
    vehicleVisual.assetPath = vehicleWorkshop.draft.bodyAsset
    vehicleVisual.usingFallback = false
    ApplyVehicleVisualMesh()
    vehicleWorkshop.message = "LIVE PREVIEW: "
        .. vehicleWorkshop.draft.displayName
        .. " | native provider " .. report.nativeProvider
    vehicleMessage = vehicleWorkshop.message
    SaveVehicleWorkshopDraft()
    return true
end

function ExportVehicleWorkshopDefinition()
    local report = RefreshVehicleWorkshopDefinition()
    if not report.valid then
        vehicleWorkshop.message = "Cannot export a structurally invalid definition"
        return false
    end
    local safeId = string.gsub(vehicleWorkshop.draft.id, "[^a-z0-9_%-]", "_")
    local relativePath = "VehicleWorkshop/" .. safeId .. ".vehicle.lua"
    local worked, pathOrError = Module.WriteSaveText(
        relativePath,
        SerializeVehicleDefinitionV2(vehicleWorkshop.definition))
    if worked then
        vehicleWorkshop.lastExportPath = pathOrError
        vehicleWorkshop.message = "Exported versioned vehicle definition"
        return true
    end
    vehicleWorkshop.message = "EXPORT ERROR: " .. tostring(pathOrError)
    return false
end

RefreshVehicleWorkshopDefinition()
