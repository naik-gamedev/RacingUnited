-- CLEAN08: VehicleDefinitionV2 dynamics/component validation.
-- This owns suspension/contact/anti-roll/chassis-flex/drive-graph validity while
-- the root validator owns schema/component identity and report orchestration.

local Validation = VehicleDefinitionV2ValidationInternal
local AddVehicleDefinitionIssue = Validation.AddIssue
local ValidFiniteVector3 = Validation.ValidFiniteVector3

function ValidateVehicleDefinitionV2Dynamics(report, definition, context)
    local bodies = context.bodies
    local powerUnits = context.powerUnits
    local transmissions = context.transmissions
    local suspensions = context.suspensions
    local contacts = context.contacts
    local antiRollBars = context.antiRollBars
    local chassisFlex = context.chassisFlex
    local connections = context.connections
    local bodyIds = context.bodyIds
    local transmissionIds = context.transmissionIds
    local suspensionIds = context.suspensionIds
    local contactIds = context.contactIds

    for _, suspension in ipairs(suspensions) do
        if not bodyIds[suspension.mountBody] then
            AddVehicleDefinitionIssue(
                report, "error", "suspension_body_ref",
                "Suspension '" .. tostring(suspension.id)
                    .. "' references a missing body")
        end
        if type(suspension.motionRatio) ~= "number"
            or suspension.motionRatio <= 0.0 then
            AddVehicleDefinitionIssue(
                report, "error", "suspension_motion_ratio",
                "Suspension '" .. tostring(suspension.id)
                    .. "' needs a positive motion ratio")
        end
        if not ValidFiniteVector3(suspension.steeringAxis) then
            AddVehicleDefinitionIssue(
                report, "error", "suspension_steering_axis",
                "Suspension '" .. tostring(suspension.id)
                    .. "' needs a finite non-zero steering axis")
        end
        local signedGeometryFields = {
            { "staticCamberDegrees", 45.0 },
            { "camberGainDegreesPerM", 1000.0 },
            { "camberProgressionDegreesPerM2", 10000.0 },
            { "staticToeDegrees", 45.0 },
            { "toeGainDegreesPerM", 1000.0 },
            { "toeProgressionDegreesPerM2", 10000.0 }
        }
        for _, fieldLimit in ipairs(signedGeometryFields) do
            local field = fieldLimit[1]
            local limit = fieldLimit[2]
            local value = suspension[field]
            if type(value) ~= "number" or value ~= value
                or math.abs(value) > limit then
                AddVehicleDefinitionIssue(
                    report, "error", "suspension_geometry_parameter",
                    "Suspension '" .. tostring(suspension.id)
                        .. "' has invalid " .. field)
                break
            end
        end
        local nonNegativeFields = {
            "springPreloadN", "springRateNPerM",
            "springProgressionNPerM2", "bumpDampingNsPerM",
            "bumpHighSpeedDampingNsPerM", "bumpDampingKneeVelocityMps",
            "reboundDampingNsPerM", "reboundHighSpeedDampingNsPerM",
            "reboundDampingKneeVelocityMps", "bumpStopEngagementM",
            "bumpStopRateNPerM", "bumpStopProgressionNPerM2",
            "droopStopEngagementM", "droopStopRateNPerM"
        }
        for _, field in ipairs(nonNegativeFields) do
            local value = suspension[field]
            if type(value) ~= "number" or value < 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "suspension_parameter",
                    "Suspension '" .. tostring(suspension.id)
                        .. "' has invalid " .. field)
                break
            end
        end
    end
    for _, contact in ipairs(contacts) do
        if not bodyIds[contact.mountBody] then
            AddVehicleDefinitionIssue(
                report, "error", "contact_body_ref",
                "Contact unit '" .. tostring(contact.id) .. "' references a missing body")
        end
        if not suspensionIds[contact.suspension] then
            AddVehicleDefinitionIssue(
                report, "error", "contact_suspension_ref",
                "Contact unit '" .. tostring(contact.id)
                    .. "' references a missing suspension")
        end
        local nonNegativeContactFields = {
            "effectiveUnsprungMassKg", "tireRadialDampingNsPerM"
        }
        for _, field in ipairs(nonNegativeContactFields) do
            local value = contact[field]
            if type(value) ~= "number" or value < 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "contact_vertical_parameter",
                    "Contact unit '" .. tostring(contact.id)
                        .. "' has invalid " .. field)
                break
            end
        end
        local positiveContactFields = {
            "tireRadialStiffnessNPerM", "maximumTireDeflectionM",
            "maximumTireNormalForceN"
        }
        for _, field in ipairs(positiveContactFields) do
            local value = contact[field]
            if type(value) ~= "number" or value <= 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "contact_vertical_parameter",
                    "Contact unit '" .. tostring(contact.id)
                        .. "' has invalid " .. field)
                break
            end
        end
    end
    for _, bar in ipairs(antiRollBars) do
        if not contactIds[bar.leftContactUnit] then
            AddVehicleDefinitionIssue(
                report, "error", "anti_roll_bar_left_ref",
                "Anti-roll bar '" .. tostring(bar.id)
                    .. "' references a missing left contact unit")
        end
        if not contactIds[bar.rightContactUnit] then
            AddVehicleDefinitionIssue(
                report, "error", "anti_roll_bar_right_ref",
                "Anti-roll bar '" .. tostring(bar.id)
                    .. "' references a missing right contact unit")
        end
        if bar.leftContactUnit == bar.rightContactUnit then
            AddVehicleDefinitionIssue(
                report, "error", "anti_roll_bar_same_contact",
                "Anti-roll bar '" .. tostring(bar.id)
                    .. "' must couple two different contact units")
        end
        local nonNegativeFields = {
            "torsionalStiffnessNmPerRad", "torsionalDampingNmsPerRad",
            "maximumWheelForceN"
        }
        for _, field in ipairs(nonNegativeFields) do
            local value = bar[field]
            if type(value) ~= "number" or value < 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "anti_roll_bar_parameter",
                    "Anti-roll bar '" .. tostring(bar.id)
                        .. "' has invalid " .. field)
                break
            end
        end
        local positiveFields = {
            "leftLeverArmM", "rightLeverArmM",
            "leftLinkMotionRatio", "rightLinkMotionRatio"
        }
        for _, field in ipairs(positiveFields) do
            local value = bar[field]
            if type(value) ~= "number" or value <= 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "anti_roll_bar_parameter",
                    "Anti-roll bar '" .. tostring(bar.id)
                        .. "' has invalid " .. field)
                break
            end
        end
        local confidence = tonumber(bar.confidence) or 0.0
        if confidence < 0.0 or confidence > 1.0 then
            AddVehicleDefinitionIssue(
                report, "error", "anti_roll_bar_confidence",
                "Anti-roll bar '" .. tostring(bar.id)
                    .. "' confidence must be in 0..1")
        end
    end

    if chassisFlex.enabled == true then
        if chassisFlex.provider ~= "chassis_torsional_mode_v1" then
            AddVehicleDefinitionIssue(
                report, "error", "chassis_flex_provider",
                "Enabled chassis flex requires chassis_torsional_mode_v1")
        end
        if not bodyIds[chassisFlex.mountBody] then
            AddVehicleDefinitionIssue(
                report, "error", "chassis_flex_body_ref",
                "Chassis flex references a missing mount body")
        end
        local positiveFlexFields = {
            "torsionalRigidityNmPerDegree",
            "effectiveTorsionalInertiaKgM2",
            "maximumTwistDegrees"
        }
        for _, field in ipairs(positiveFlexFields) do
            local value = chassisFlex[field]
            if type(value) ~= "number" or value <= 0.0 or value ~= value then
                AddVehicleDefinitionIssue(
                    report, "error", "chassis_flex_parameter",
                    "Chassis flex has invalid " .. field)
                break
            end
        end
        local damping = chassisFlex.torsionalDampingNmsPerRad
        if type(damping) ~= "number" or damping < 0.0 or damping ~= damping then
            AddVehicleDefinitionIssue(
                report, "error", "chassis_flex_parameter",
                "Chassis flex has invalid torsionalDampingNmsPerRad")
        end
        local frontZ = chassisFlex.frontReferenceLocalZ
        local rearZ = chassisFlex.rearReferenceLocalZ
        if type(frontZ) ~= "number" or type(rearZ) ~= "number"
            or frontZ ~= frontZ or rearZ ~= rearZ
            or frontZ - rearZ < 0.10 then
            AddVehicleDefinitionIssue(
                report, "error", "chassis_flex_reference_span",
                "Chassis flex needs frontReferenceLocalZ ahead of rearReferenceLocalZ")
        end
        local confidence = tonumber(chassisFlex.confidence) or 0.0
        if confidence < 0.0 or confidence > 1.0 then
            AddVehicleDefinitionIssue(
                report, "error", "chassis_flex_confidence",
                "Chassis-flex confidence must be in 0..1")
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

end
