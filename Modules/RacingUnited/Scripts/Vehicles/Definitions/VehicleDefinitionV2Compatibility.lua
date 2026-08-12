-- CLEAN08: current-native-solver compatibility/readiness validation.
-- A definition can be structurally valid while requiring topology/mechanisms that
-- the current preview solver does not implement yet; keep those concepts distinct.

local Validation = VehicleDefinitionV2ValidationInternal
local AddVehicleDefinitionIssue = Validation.AddIssue

function ValidateVehicleDefinitionV2Compatibility(report, definition, context)
    local bodies = context.bodies
    local powerUnits = context.powerUnits
    local transmissions = context.transmissions
    local suspensions = context.suspensions
    local contacts = context.contacts
    local chassisFlex = context.chassisFlex

    local requirements = definition.requirements or {}
    local currentReasons = {}
    if #bodies ~= 1 then table.insert(currentReasons, "one rigid body") end
    if #powerUnits ~= 1 then table.insert(currentReasons, "one power unit") end
    if #transmissions ~= 1 then table.insert(currentReasons, "one transmission") end
    if #contacts ~= 4 then table.insert(currentReasons, "four wheel contacts") end
    if definition.topologyIntent
        and definition.topologyIntent.driveLayout == "split" then
        table.insert(currentReasons, "independent drivetrain routing")
    end
    if #transmissions == 1
        and (#(transmissions[1].forwardRatios or {}) < 1
            or #(transmissions[1].forwardRatios or {}) > 16) then
        table.insert(currentReasons, "1 to 16 native forward ratios")
    end
    local currentSuspensionProviders = {
        linear_raycast_v1 = true,
        macpherson_strut_v1 = true,
        trailing_arm_torsion_bar_v1 = true
    }
    local suspensionReasons = {}
    for _, suspension in ipairs(suspensions) do
        if not currentSuspensionProviders[suspension.provider]
            and not suspensionReasons[suspension.provider] then
            suspensionReasons[suspension.provider] = true
            table.insert(currentReasons,
                "suspension provider '" .. tostring(suspension.provider) .. "'")
        end
    end
    if chassisFlex.enabled == true
        and chassisFlex.provider ~= "chassis_torsional_mode_v1" then
        table.insert(currentReasons,
            "chassis-flex provider '" .. tostring(chassisFlex.provider) .. "'")
    end
    if requirements.leanDynamics then
        table.insert(currentReasons, "motorcycle lean/camber dynamics")
    end
    if requirements.articulation then
        table.insert(currentReasons, "articulated body constraints")
    end
    if requirements.trackContacts then
        table.insert(currentReasons, "continuous track contacts")
    end
    if #currentReasons > 0 then
        report.currentSolverReady = false
        AddVehicleDefinitionIssue(
            report, "warning", "future_solver_components",
            "Definition is valid but preview awaits: " .. table.concat(currentReasons, ", "))
    end

    local bodyAsset = definition.presentation
        and definition.presentation.bodyAsset or ""
    if bodyAsset == "" then
        AddVehicleDefinitionIssue(
            report, "warning", "missing_visual", "No presentation body asset is selected")
    elseif not Module.AssetExists(bodyAsset) then
        AddVehicleDefinitionIssue(
            report, "warning", "missing_asset",
            "Asset is not present inside this module: " .. bodyAsset)
    end
end
