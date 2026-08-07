-- Versioned, topology-first vehicle authoring contract.
-- Classification selects an editor template only. Simulation support is
-- determined by the components and their connections, never by `if car` or
-- `if motorcycle` branches in the generic vehicle loader.
VehicleDefinitionV2 = {
    schemaVersion = 2,
    templateOrder = {
        "road_car", "formula", "indycar", "kart", "sprint_car",
        "atv", "motorcycle", "truck", "twin_engine", "custom"
    },
    templates = {
        road_car = {
            label = "ROAD CAR", classification = "car", massKg = 1100.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 6,
            driveLayout = "fwd", engineLocation = "front",
            maximumTorqueNm = 250.0
        },
        formula = {
            label = "FORMULA", classification = "formula", massKg = 795.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 8,
            driveLayout = "rwd", engineLocation = "mid",
            maximumTorqueNm = 500.0
        },
        indycar = {
            label = "INDYCAR", classification = "indycar", massKg = 770.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 6,
            driveLayout = "rwd", engineLocation = "mid",
            maximumTorqueNm = 530.0
        },
        kart = {
            label = "GO-KART", classification = "kart", massKg = 165.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 1,
            driveLayout = "rwd", engineLocation = "rear",
            maximumTorqueNm = 22.0
        },
        sprint_car = {
            label = "SPRINT CAR", classification = "sprint_car", massKg = 625.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 1,
            driveLayout = "rwd", engineLocation = "front",
            maximumTorqueNm = 900.0
        },
        atv = {
            label = "ATV", classification = "atv", massKg = 330.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 5,
            driveLayout = "awd", engineLocation = "mid",
            maximumTorqueNm = 65.0
        },
        motorcycle = {
            label = "MOTORCYCLE", classification = "motorcycle", massKg = 205.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 2, forwardGearCount = 6,
            driveLayout = "rwd", engineLocation = "mid",
            maximumTorqueNm = 120.0, requiresLeanDynamics = true
        },
        truck = {
            label = "TRUCK", classification = "truck", massKg = 7500.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 6, forwardGearCount = 12,
            driveLayout = "rwd", engineLocation = "front",
            maximumTorqueNm = 2200.0
        },
        twin_engine = {
            label = "TWIN ENGINE", classification = "car", massKg = 735.0,
            bodyCount = 1, powerUnitCount = 2, transmissionCount = 2,
            contactUnitCount = 4, forwardGearCount = 4,
            driveLayout = "split", engineLocation = "distributed",
            maximumTorqueNm = 54.0
        },
        custom = {
            label = "CUSTOM", classification = "custom", massKg = 1000.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 5,
            driveLayout = "rwd", engineLocation = "front",
            maximumTorqueNm = 200.0
        }
    }
}

local function CopyVehicleDefinitionValue(value)
    if type(value) ~= "table" then return value end
    local copy = {}
    for key, child in pairs(value) do
        copy[key] = CopyVehicleDefinitionValue(child)
    end
    return copy
end

function CreateVehicleWorkshopDraft(templateId)
    local template = VehicleDefinitionV2.templates[templateId]
        or VehicleDefinitionV2.templates.custom
    local draft = CopyVehicleDefinitionValue(template)
    draft.templateId = VehicleDefinitionV2.templates[templateId]
        and templateId or "custom"
    draft.id = "workshop_vehicle"
    draft.displayName = template.label .. " WORKSHOP DRAFT"
    draft.bodyAsset = "Vehicles/Player/PlayerCar.obj"
    draft.requiresArticulation = false
    draft.requiresTrackContacts = false
    return draft
end

local function WorkshopGearRatios(count)
    local defaults = { 3.40, 2.10, 1.45, 1.12, 0.89, 0.74, 0.62, 0.53 }
    local ratios = {}
    for index = 1, count do
        if defaults[index] then
            ratios[index] = defaults[index]
        else
            ratios[index] = math.max(0.20, ratios[index - 1] * 0.86)
        end
    end
    return ratios
end

local function WorkshopContactPosition(index, count)
    if count == 2 then
        return { 0.0, 0.72, index == 1 and 1.20 or -1.20 }
    end
    if count == 3 then
        if index == 1 then return { 0.0, 0.72, 1.20 } end
        return { index == 2 and -0.72 or 0.72, 0.72, -1.20 }
    end
    if count == 4 then
        local source = PrototypeCarDefinition and PrototypeCarDefinition.wheels[index]
        if source then return CopyVehicleDefinitionValue(source.mount) end
    end

    local axleCount = math.ceil(count / 2)
    local axle = math.ceil(index / 2)
    local longitudinal = axleCount <= 1 and 0.0
        or 1.40 - (axle - 1) * (2.80 / (axleCount - 1))
    return { index % 2 == 1 and -0.78 or 0.78, 0.82, longitudinal }
end

local function WorkshopContactAxle(index, count)
    if count == 2 then return index == 1 and "front" or "rear" end
    local axle = math.ceil(index / 2)
    local axleCount = math.ceil(count / 2)
    if axle == 1 then return "front" end
    if axle == axleCount then return "rear" end
    return "middle_" .. tostring(axle - 1)
end

local function WorkshopContactDriven(layout, axle)
    if layout == "awd" or layout == "split" then return true end
    if layout == "fwd" then return axle == "front" end
    if layout == "rwd" then return axle == "rear" end
    return false
end

function BuildVehicleDefinitionV2(draft)
    local definition = {
        schemaVersion = VehicleDefinitionV2.schemaVersion,
        id = draft.id,
        displayName = draft.displayName,
        classification = draft.classification,
        presentation = {
            bodyAsset = draft.bodyAsset,
            coordinateConvention = "blender_x_right_y_forward_z_up",
            metresPerUnit = 1.0
        },
        requirements = {
            leanDynamics = draft.requiresLeanDynamics == true,
            articulation = draft.requiresArticulation == true,
            trackContacts = draft.requiresTrackContacts == true
        },
        topologyIntent = {
            driveLayout = draft.driveLayout,
            powerUnitPlacement = draft.engineLocation
        },
        bodies = {},
        powerUnits = {},
        transmissions = {},
        contactUnits = {},
        driveConnections = {}
    }

    local bodyCount = math.max(0, math.floor(draft.bodyCount or 0))
    for index = 1, bodyCount do
        definition.bodies[index] = {
            id = index == 1 and "chassis" or ("body_" .. tostring(index)),
            massKg = index == 1 and draft.massKg or math.max(1.0, draft.massKg * 0.10),
            role = index == 1 and "primary" or "attached"
        }
    end

    local powerCount = math.max(0, math.floor(draft.powerUnitCount or 0))
    for index = 1, powerCount do
        local location = draft.engineLocation
        if location == "distributed" then
            location = index % 2 == 1 and "front" or "rear"
        end
        definition.powerUnits[index] = {
            id = "power_unit_" .. tostring(index),
            kind = "combustion",
            mountBody = "chassis",
            location = location,
            maximumTorqueNm = draft.maximumTorqueNm,
            idleRpm = 900.0,
            redlineRpm = 7000.0
        }
    end

    local transmissionCount = math.max(
        0, math.floor(draft.transmissionCount or 0))
    for index = 1, transmissionCount do
        definition.transmissions[index] = {
            id = "transmission_" .. tostring(index),
            kind = (draft.forwardGearCount or 0) <= 1 and "direct" or "manual",
            powerUnit = powerCount > 0
                and ("power_unit_" .. tostring(math.min(index, powerCount))) or "",
            reverseRatio = -3.20,
            forwardRatios = WorkshopGearRatios(
                math.max(0, math.floor(draft.forwardGearCount or 0)))
        }
    end

    local contactCount = math.max(0, math.floor(draft.contactUnitCount or 0))
    for index = 1, contactCount do
        local axle = WorkshopContactAxle(index, contactCount)
        definition.contactUnits[index] = {
            id = "contact_" .. tostring(index),
            kind = draft.requiresTrackContacts and "track_patch" or "wheel",
            mountBody = "chassis",
            axle = axle,
            position = WorkshopContactPosition(index, contactCount),
            steering = axle == "front",
            driven = WorkshopContactDriven(draft.driveLayout, axle),
            serviceBrake = true,
            parkingBrake = axle == "rear",
            suspensionProvider = "raycast_linear",
            tireProvider = draft.requiresLeanDynamics
                and "motorcycle_profile" or "advanced_road"
        }
    end

    for transmission = 1, transmissionCount do
        local connection = {
            id = "drive_" .. tostring(transmission),
            transmission = "transmission_" .. tostring(transmission),
            contactUnits = {}
        }
        for _, contact in ipairs(definition.contactUnits) do
            local include = contact.driven
            if draft.driveLayout == "split" then
                include = transmission == 1 and contact.axle == "front"
                    or transmission == 2 and contact.axle == "rear"
            end
            if include then
                table.insert(connection.contactUnits, contact.id)
            end
        end
        table.insert(definition.driveConnections, connection)
    end
    return definition
end

local function AddVehicleDefinitionIssue(report, severity, code, message)
    table.insert(report.issues, {
        severity = severity, code = code, message = message
    })
    if severity == "error" then
        report.errorCount = report.errorCount + 1
    else
        report.warningCount = report.warningCount + 1
    end
end

local function ValidateUniqueComponentIds(report, collection, label)
    local seen = {}
    for _, component in ipairs(collection) do
        if type(component.id) ~= "string" or component.id == "" then
            AddVehicleDefinitionIssue(
                report, "error", "missing_id", label .. " has a missing ID")
        elseif seen[component.id] then
            AddVehicleDefinitionIssue(
                report, "error", "duplicate_id",
                label .. " repeats ID '" .. component.id .. "'")
        else
            seen[component.id] = true
        end
    end
    return seen
end

function ValidateVehicleDefinitionV2(definition)
    local report = {
        valid = true,
        currentSolverReady = true,
        errorCount = 0,
        warningCount = 0,
        issues = {},
        summary = ""
    }
    if type(definition) ~= "table" then
        AddVehicleDefinitionIssue(
            report, "error", "not_a_table", "Definition is not a table")
        report.valid = false
        report.currentSolverReady = false
        return report
    end
    if definition.schemaVersion ~= 2 then
        AddVehicleDefinitionIssue(
            report, "error", "schema_version", "schemaVersion must be 2")
    end
    if type(definition.id) ~= "string"
        or not string.match(definition.id, "^[a-z0-9_%-]+$") then
        AddVehicleDefinitionIssue(
            report, "error", "unsafe_id",
            "ID must use lowercase letters, numbers, underscores or hyphens")
    end
    if type(definition.displayName) ~= "string" or definition.displayName == "" then
        AddVehicleDefinitionIssue(
            report, "error", "missing_name", "Display name is required")
    end

    local bodies = definition.bodies or {}
    local powerUnits = definition.powerUnits or {}
    local transmissions = definition.transmissions or {}
    local contacts = definition.contactUnits or {}
    local connections = definition.driveConnections or {}
    if #bodies < 1 or #bodies > 16 then
        AddVehicleDefinitionIssue(
            report, "error", "body_count", "A vehicle requires 1 to 16 bodies")
    end
    if #powerUnits > 8 then
        AddVehicleDefinitionIssue(
            report, "error", "power_count", "At most 8 power units are allowed")
    end
    if #transmissions > 8 then
        AddVehicleDefinitionIssue(
            report, "error", "transmission_count",
            "At most 8 transmissions are allowed")
    end
    if #contacts < 1 or #contacts > 32 then
        AddVehicleDefinitionIssue(
            report, "error", "contact_count",
            "A ground vehicle requires 1 to 32 contact units")
    end

    local bodyIds = ValidateUniqueComponentIds(report, bodies, "Body")
    local powerIds = ValidateUniqueComponentIds(report, powerUnits, "Power unit")
    local transmissionIds = ValidateUniqueComponentIds(
        report, transmissions, "Transmission")
    local contactIds = ValidateUniqueComponentIds(report, contacts, "Contact unit")
    for _, body in ipairs(bodies) do
        if type(body.massKg) ~= "number" or body.massKg <= 0.0 then
            AddVehicleDefinitionIssue(
                report, "error", "body_mass",
                "Body '" .. tostring(body.id) .. "' needs positive mass")
        end
    end
    for _, power in ipairs(powerUnits) do
        if not bodyIds[power.mountBody] then
            AddVehicleDefinitionIssue(
                report, "error", "power_body_ref",
                "Power unit '" .. tostring(power.id) .. "' references a missing body")
        end
    end
    for _, transmission in ipairs(transmissions) do
        if not powerIds[transmission.powerUnit] then
            AddVehicleDefinitionIssue(
                report, "error", "transmission_power_ref",
                "Transmission '" .. tostring(transmission.id)
                    .. "' references a missing power unit")
        end
        if #(transmission.forwardRatios or {}) > 32 then
            AddVehicleDefinitionIssue(
                report, "error", "gear_count", "A transmission supports at most 32 forward ratios")
        end
    end
    for _, contact in ipairs(contacts) do
        if not bodyIds[contact.mountBody] then
            AddVehicleDefinitionIssue(
                report, "error", "contact_body_ref",
                "Contact unit '" .. tostring(contact.id) .. "' references a missing body")
        end
    end
    for _, connection in ipairs(connections) do
        if not transmissionIds[connection.transmission] then
            AddVehicleDefinitionIssue(
                report, "error", "drive_transmission_ref",
                "Drive connection '" .. tostring(connection.id)
                    .. "' references a missing transmission")
        end
        for _, contactId in ipairs(connection.contactUnits or {}) do
            if not contactIds[contactId] then
                AddVehicleDefinitionIssue(
                    report, "error", "drive_contact_ref",
                    "Drive connection '" .. tostring(connection.id)
                        .. "' references missing contact '" .. tostring(contactId) .. "'")
            end
        end
    end
    if #powerUnits == 0 then
        AddVehicleDefinitionIssue(
            report, "warning", "unpowered", "Definition is an unpowered vehicle or trailer")
    elseif #connections == 0 then
        AddVehicleDefinitionIssue(
            report, "warning", "undriven", "Power units exist but no drive connection reaches a contact unit")
    end
    if #powerUnits > 0 then
        AddVehicleDefinitionIssue(
            report, "warning", "placement_metadata_only",
            "Power-unit placement is retained, but the current preview does not yet derive center of mass or inertia from component locations")
    end

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

    report.valid = report.errorCount == 0
    report.currentSolverReady = report.valid and report.currentSolverReady
    report.summary = string.format(
        "schema v2 | %d bodies | %d power units | %d transmissions | %d contacts",
        #bodies, #powerUnits, #transmissions, #contacts)
    return report
end

local function SerializeVehicleDefinitionValue(value, indent)
    local valueType = type(value)
    if valueType == "string" then return string.format("%q", value) end
    if valueType == "number" or valueType == "boolean" then
        return tostring(value)
    end
    if valueType ~= "table" then return "nil" end

    local nextIndent = indent .. "    "
    local lines = { "{" }
    local numericCount = #value
    for index = 1, numericCount do
        table.insert(lines, nextIndent
            .. SerializeVehicleDefinitionValue(value[index], nextIndent) .. ",")
    end
    local keys = {}
    for key, _ in pairs(value) do
        if type(key) ~= "number" then table.insert(keys, key) end
    end
    table.sort(keys, function(left, right) return tostring(left) < tostring(right) end)
    for _, key in ipairs(keys) do
        local encodedKey = string.match(key, "^[%a_][%w_]*$")
            and key or ("[" .. string.format("%q", key) .. "]")
        table.insert(lines, nextIndent .. encodedKey .. " = "
            .. SerializeVehicleDefinitionValue(value[key], nextIndent) .. ",")
    end
    table.insert(lines, indent .. "}")
    return table.concat(lines, "\n")
end

function SerializeVehicleDefinitionV2(definition)
    return "-- Generated by Heritage Vehicle Workshop\nreturn "
        .. SerializeVehicleDefinitionValue(definition, "") .. "\n"
end
