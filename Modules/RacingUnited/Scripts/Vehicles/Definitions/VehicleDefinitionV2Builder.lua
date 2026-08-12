-- VehicleDefinitionV2 workshop draft construction and topology builder.
-- Loaded after VehicleDefinitionV2.lua; keeps builder-specific helpers local.

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

local function WorkshopSuspensionHardpoints(axle, cornerName)
    local architectures = PrototypeCarDefinition
        and PrototypeCarDefinition.suspensionArchitecture or {}
    local assembly = architectures[axle]
    local result = {}
    if assembly == nil then return result end
    local hardpoints = assembly.hardpointsByCorner
        and assembly.hardpointsByCorner[cornerName] or {}

    -- Preserve stable authoring order. A point may be an older raw vec3 or a
    -- SUS03A record carrying epistemic provenance/confidence. Estimated points
    -- are valid engineering inputs, but remain visibly distinguishable from
    -- measured or GLB-authored coordinates. Per-corner data is never mirrored
    -- implicitly by the definition compiler.
    for _, id in ipairs(assembly.requiredHardpoints or {}) do
        local source = hardpoints[id]
        local position = source
        local provenance = ""
        local confidence = 0.0
        if type(source) == "table" and type(source.position) == "table" then
            position = source.position
            provenance = tostring(source.provenance or "")
            confidence = tonumber(source.confidence) or 0.0
        end
        if type(position) == "table" and #position >= 3 then
            result[#result + 1] = {
                id = id,
                position = { position[1], position[2], position[3] },
                provenance = provenance,
                confidence = confidence
            }
        end
    end
    return result
end

local function WorkshopSuspensionProvider(axle, fallback)
    local architectures = PrototypeCarDefinition
        and PrototypeCarDefinition.suspensionArchitecture or {}
    local assembly = architectures[axle]
    if assembly and type(assembly.runtimeProvider) == "string"
        and assembly.runtimeProvider ~= "" then
        return assembly.runtimeProvider
    end
    return fallback or "linear_raycast_v1"
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
    local prototypePowertrain = PrototypeCarDefinition
        and PrototypeCarDefinition.powertrain or {}
    local prototypeWheelPhysics = PrototypeCarDefinition
        and PrototypeCarDefinition.wheelPhysics or {}
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
        suspensions = {},
        contactUnits = {},
        antiRollBars = {},
        chassisFlex = { enabled = false },
        driveConnections = {}
    }

    -- Closed-unibody road-car drafts inherit the current prototype's explicit
    -- low-confidence structural estimate. Other templates remain rigid until
    -- their own construction-aware estimate or authored value is supplied.
    if draft.templateId == "road_car"
        and PrototypeCarDefinition and PrototypeCarDefinition.chassisFlex then
        definition.chassisFlex = CopyVehicleDefinitionValue(
            PrototypeCarDefinition.chassisFlex)
    end

    local bodyCount = math.max(0, math.floor(draft.bodyCount or 0))
    for index = 1, bodyCount do
        definition.bodies[index] = {
            id = index == 1 and "chassis" or ("body_" .. tostring(index)),
            massKg = index == 1 and draft.massKg or math.max(1.0, draft.massKg * 0.10),
            role = index == 1 and "primary" or "attached"
        }
        if index == 1 and PrototypeCarDefinition and PrototypeCarDefinition.chassis then
            local chassis = PrototypeCarDefinition.chassis
            definition.bodies[index].centerOfMassLocal =
                CopyVehicleDefinitionValue(chassis.centerOfMassLocal)
            definition.bodies[index].inertiaLocalKgM2 =
                CopyVehicleDefinitionValue(chassis.inertiaLocalKgM2)
            definition.bodies[index].frontStaticLoadFraction =
                chassis.frontStaticLoadFraction or 0.50
            definition.bodies[index].leftStaticLoadFraction =
                chassis.leftStaticLoadFraction or 0.50
            definition.bodies[index].massPropertiesProvenance =
                chassis.massPropertiesProvenance or ""
            definition.bodies[index].massPropertiesConfidence =
                chassis.massPropertiesConfidence or 0.0
        end
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
            redlineRpm = 7000.0,
            engineBrakingTorqueNm =
                prototypePowertrain.engineBrakingTorque or 70.0
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
            reverseRatio = prototypePowertrain.reverseRatio or -3.20,
            forwardRatios = WorkshopGearRatios(
                math.max(0, math.floor(draft.forwardGearCount or 0))),
            finalDriveRatio = prototypePowertrain.finalDriveRatio or 3.90,
            efficiency = prototypePowertrain.efficiency or 0.88,
            shiftDurationSeconds = prototypePowertrain.shiftDuration or 0.22,
            clutchEngagementRate =
                prototypePowertrain.clutchEngagementRate or 5.0
        }
    end

    local contactCount = math.max(0, math.floor(draft.contactUnitCount or 0))
    for index = 1, contactCount do
        local axle = WorkshopContactAxle(index, contactCount)
        local source = PrototypeCarDefinition
            and PrototypeCarDefinition.wheels[index] or nil
        local serviceBrakeFactor = contactCount > 0 and 1.0 / contactCount or 0.0
        if contactCount == 4 then
            serviceBrakeFactor = axle == "front" and 0.31 or 0.19
        end
        local suspensionId = "suspension_" .. tostring(index)
        definition.suspensions[index] = {
            id = suspensionId,
            provider = WorkshopSuspensionProvider(
                axle, draft.suspensionProvider),
            mountBody = "chassis",
            hardpoints = WorkshopSuspensionHardpoints(
                axle, source and source.name or "contact_" .. tostring(index)),
            restLengthM = source and source.restLengthM
                or prototypeWheelPhysics.restLengthM or 0.50,
            maximumCompressionM = source and source.maximumCompressionM
                or prototypeWheelPhysics.maximumCompressionM or 0.18,
            maximumDroopM = source and source.maximumDroopM
                or prototypeWheelPhysics.maximumDroopM or 0.15,
            springPreloadN = 0.0,
            springRateNPerM = source and source.springRateNPerM
                or prototypeWheelPhysics.springRateNPerM or 35000.0,
            springProgressionNPerM2 = 15000.0,
            bumpDampingNsPerM = source and source.bumpDampingNsPerM
                or prototypeWheelPhysics.bumpDampingNsPerM or 3200.0,
            bumpHighSpeedDampingNsPerM = 1800.0,
            bumpDampingKneeVelocityMps = 0.25,
            reboundDampingNsPerM = source and source.reboundDampingNsPerM
                or prototypeWheelPhysics.reboundDampingNsPerM or 4200.0,
            reboundHighSpeedDampingNsPerM = 2600.0,
            reboundDampingKneeVelocityMps = 0.30,
            bumpStopEngagementM = (source and source.maximumCompressionM
                or prototypeWheelPhysics.maximumCompressionM or 0.18) * 0.75,
            bumpStopRateNPerM = 120000.0,
            bumpStopProgressionNPerM2 = 1000000.0,
            droopStopEngagementM = (source and source.maximumDroopM
                or prototypeWheelPhysics.maximumDroopM or 0.15) * 0.85,
            droopStopRateNPerM = 35000.0,
            steeringAxis = source and source.steeringAxis
                or prototypeWheelPhysics.steeringAxis or { 0.0, 1.0, 0.0 },
            staticCamberDegrees = source and source.staticCamberDegrees
                or prototypeWheelPhysics.staticCamberDegrees or 0.0,
            camberGainDegreesPerM = source and source.camberGainDegreesPerM
                or prototypeWheelPhysics.camberGainDegreesPerM or 0.0,
            camberProgressionDegreesPerM2 = source
                and source.camberProgressionDegreesPerM2
                or prototypeWheelPhysics.camberProgressionDegreesPerM2 or 0.0,
            staticToeDegrees = source and source.staticToeDegrees
                or prototypeWheelPhysics.staticToeDegrees or 0.0,
            toeGainDegreesPerM = source and source.toeGainDegreesPerM
                or prototypeWheelPhysics.toeGainDegreesPerM or 0.0,
            toeProgressionDegreesPerM2 = source
                and source.toeProgressionDegreesPerM2
                or prototypeWheelPhysics.toeProgressionDegreesPerM2 or 0.0,
            motionRatio = 1.0,
            maximumForceN = 250000.0
        }
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
            suspension = suspensionId,
            tireProvider = draft.requiresLeanDynamics
                and "mf62_motorcycle" or "mf62_road",
            tireParameterFile = source and source.tireParameterFile or nil,
            tireParameterProvenance = source and source.tireParameterProvenance or nil,
            tireParameterConfidence = source and source.tireParameterConfidence or 0.0,
            suspensionDirection = { 0.0, -1.0, 0.0 },
            radiusM = source and source.radiusM
                or prototypeWheelPhysics.radiusM or 0.35,
            effectiveUnsprungMassKg = source
                and source.effectiveUnsprungMassKg
                or prototypeWheelPhysics.effectiveUnsprungMassKg or 0.0,
            tireRadialStiffnessNPerM = source
                and source.tireRadialStiffnessNPerM
                or prototypeWheelPhysics.tireRadialStiffnessNPerM or 220000.0,
            tireRadialDampingNsPerM = source
                and source.tireRadialDampingNsPerM
                or prototypeWheelPhysics.tireRadialDampingNsPerM or 1800.0,
            maximumTireDeflectionM = source
                and source.maximumTireDeflectionM
                or prototypeWheelPhysics.maximumTireDeflectionM or 0.08,
            maximumTireNormalForceN = source
                and source.maximumTireNormalForceN
                or prototypeWheelPhysics.maximumTireNormalForceN or 250000.0,
            serviceBrakeFactor = serviceBrakeFactor,
            parkingBrakeFactor = axle == "rear"
                and (contactCount == 2 and 1.0 or 0.5) or 0.0
        }
    end

    -- Anti-roll bars are topology components, not hidden properties of a
    -- suspension provider. The 206-oriented four-wheel template carries the
    -- same reusable front/rear pair into the native V2 definition.
    if contactCount == 4 and PrototypeCarDefinition
        and PrototypeCarDefinition.antiRollBars then
        local wheelIndices = {}
        for index, wheel in ipairs(PrototypeCarDefinition.wheels or {}) do
            wheelIndices[wheel.name] = index
        end
        for _, group in ipairs({ "front", "rear" }) do
            local bar = PrototypeCarDefinition.antiRollBars[group]
            local leftIndex = bar and wheelIndices[bar.leftWheel] or nil
            local rightIndex = bar and wheelIndices[bar.rightWheel] or nil
            if leftIndex and rightIndex
                and definition.contactUnits[leftIndex]
                and definition.contactUnits[rightIndex] then
                definition.antiRollBars[#definition.antiRollBars + 1] = {
                    id = group .. "_anti_roll_bar",
                    leftContactUnit = definition.contactUnits[leftIndex].id,
                    rightContactUnit = definition.contactUnits[rightIndex].id,
                    enabled = bar.enabled ~= false,
                    torsionalStiffnessNmPerRad =
                        bar.torsionalStiffnessNmPerRad,
                    torsionalDampingNmsPerRad =
                        bar.torsionalDampingNmsPerRad,
                    leftLeverArmM = bar.leftLeverArmM,
                    rightLeverArmM = bar.rightLeverArmM,
                    leftLinkMotionRatio = bar.leftLinkMotionRatio,
                    rightLinkMotionRatio = bar.rightLinkMotionRatio,
                    maximumWheelForceN = bar.maximumWheelForceN,
                    provenance = bar.provenance,
                    confidence = bar.confidence
                }
            end
        end
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
