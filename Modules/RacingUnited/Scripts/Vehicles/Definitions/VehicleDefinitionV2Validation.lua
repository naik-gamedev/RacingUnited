-- VehicleDefinitionV2 structural/content validation.
-- Validation is separate from schema/templates and construction so each can grow independently.

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

local function FiniteVector3(value)
    if type(value) ~= "table" then return false end
    for index = 1, 3 do
        local component = value[index]
        if type(component) ~= "number" or component ~= component
            or component <= -math.huge or component >= math.huge then
            return false
        end
    end
    return true
end

local function ValidFiniteVector3(value)
    if not FiniteVector3(value) then return false end
    local lengthSquared = 0.0
    for index = 1, 3 do
        local component = value[index]
        if type(component) ~= "number" or component ~= component
            or component <= -math.huge or component >= math.huge then
            return false
        end
        lengthSquared = lengthSquared + component * component
    end
    return lengthSquared > 0.000001
end


VehicleDefinitionV2ValidationInternal = VehicleDefinitionV2ValidationInternal or {}
VehicleDefinitionV2ValidationInternal.AddIssue = AddVehicleDefinitionIssue
VehicleDefinitionV2ValidationInternal.FiniteVector3 = FiniteVector3
VehicleDefinitionV2ValidationInternal.ValidFiniteVector3 = ValidFiniteVector3

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
    local suspensions = definition.suspensions or {}
    local contacts = definition.contactUnits or {}
    local antiRollBars = definition.antiRollBars or {}
    local chassisFlex = definition.chassisFlex or { enabled = false }
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
    if #suspensions < 1 or #suspensions > 32 then
        AddVehicleDefinitionIssue(
            report, "error", "suspension_count",
            "A ground vehicle requires 1 to 32 suspension components")
    end
    if #contacts < 1 or #contacts > 32 then
        AddVehicleDefinitionIssue(
            report, "error", "contact_count",
            "A ground vehicle requires 1 to 32 contact units")
    end
    if #antiRollBars > 16 then
        AddVehicleDefinitionIssue(
            report, "error", "anti_roll_bar_count",
            "A vehicle supports at most 16 anti-roll bars")
    end

    local bodyIds = ValidateUniqueComponentIds(report, bodies, "Body")
    local powerIds = ValidateUniqueComponentIds(report, powerUnits, "Power unit")
    local transmissionIds = ValidateUniqueComponentIds(
        report, transmissions, "Transmission")
    local suspensionIds = ValidateUniqueComponentIds(
        report, suspensions, "Suspension")
    local contactIds = ValidateUniqueComponentIds(report, contacts, "Contact unit")
    ValidateUniqueComponentIds(report, antiRollBars, "Anti-roll bar")
    for _, body in ipairs(bodies) do
        if type(body.massKg) ~= "number" or body.massKg <= 0.0 then
            AddVehicleDefinitionIssue(
                report, "error", "body_mass",
                "Body '" .. tostring(body.id) .. "' needs positive mass")
        end
        if body.centerOfMassLocal ~= nil and not FiniteVector3(body.centerOfMassLocal) then
            AddVehicleDefinitionIssue(
                report, "error", "body_center_of_mass",
                "Body '" .. tostring(body.id) .. "' has invalid centerOfMassLocal")
        end
        if body.inertiaLocalKgM2 ~= nil then
            local inertia = body.inertiaLocalKgM2
            if not FiniteVector3(inertia)
                or inertia[1] <= 0.0 or inertia[2] <= 0.0 or inertia[3] <= 0.0 then
                AddVehicleDefinitionIssue(
                    report, "error", "body_inertia",
                    "Body '" .. tostring(body.id) .. "' needs positive inertiaLocalKgM2")
            end
        end
        if body.frontStaticLoadFraction ~= nil
            and (type(body.frontStaticLoadFraction) ~= "number"
                or body.frontStaticLoadFraction <= 0.0
                or body.frontStaticLoadFraction >= 1.0) then
            AddVehicleDefinitionIssue(
                report, "error", "body_front_static_load",
                "Body '" .. tostring(body.id) .. "' front static-load fraction must be between 0 and 1")
        end
        if body.leftStaticLoadFraction ~= nil
            and (type(body.leftStaticLoadFraction) ~= "number"
                or body.leftStaticLoadFraction <= 0.0
                or body.leftStaticLoadFraction >= 1.0) then
            AddVehicleDefinitionIssue(
                report, "error", "body_left_static_load",
                "Body '" .. tostring(body.id) .. "' left static-load fraction must be between 0 and 1")
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
    local validationContext = {
        bodies = bodies,
        powerUnits = powerUnits,
        transmissions = transmissions,
        suspensions = suspensions,
        contacts = contacts,
        antiRollBars = antiRollBars,
        chassisFlex = chassisFlex,
        connections = connections,
        bodyIds = bodyIds,
        powerIds = powerIds,
        transmissionIds = transmissionIds,
        suspensionIds = suspensionIds,
        contactIds = contactIds
    }

    ValidateVehicleDefinitionV2Dynamics(report, definition, validationContext)
    ValidateVehicleDefinitionV2Compatibility(report, definition, validationContext)

    report.valid = report.errorCount == 0
    report.currentSolverReady = report.valid and report.currentSolverReady
    report.summary = string.format(
        "schema v2 | %d bodies | %d power units | %d transmissions | %d suspensions | %d contacts | chassis flex %s",
        #bodies, #powerUnits, #transmissions, #suspensions, #contacts,
        chassisFlex.enabled == true and "enabled" or "rigid")
    return report
end
