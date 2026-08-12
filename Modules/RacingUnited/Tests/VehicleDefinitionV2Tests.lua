-- Standalone semantic regression for the data-only topology contract.
-- The test runner loads the VehicleDefinitionV2 schema, builder, core/dynamics/compatibility
-- validation phases and serialization files before this test module.
Module = {
    AssetExists = function(_) return true end
}
PrototypeCarDefinition = {
    wheels = {
        { mount = { -0.70, 0.80, 1.20 } },
        { mount = { 0.70, 0.80, 1.20 } },
        { mount = { -0.70, 0.80, -1.20 } },
        { mount = { 0.70, 0.80, -1.20 } }
    },
    chassisFlex = {
        enabled = true,
        provider = "chassis_torsional_mode_v1",
        mountBody = "chassis",
        torsionalRigidityNmPerDegree = 8700.0,
        torsionalDampingNmsPerRad = 11300.0,
        effectiveTorsionalInertiaKgM2 = 525.0,
        torsionAxisLocalY = 0.364,
        frontReferenceLocalZ = 1.221,
        rearReferenceLocalZ = -1.221,
        maximumTwistDegrees = 1.25,
        provenance = "estimated_chassis_flex_closed_unibody_v1",
        confidence = 0.18
    }
}

local road = BuildVehicleDefinitionV2(CreateVehicleWorkshopDraft("road_car"))
local roadReport = ValidateVehicleDefinitionV2(road)
assert(roadReport.valid, "road-car topology must be structurally valid")
assert(roadReport.currentSolverReady, "road-car template must preview today")
assert(#road.bodies == 1 and #road.contactUnits == 4,
    "road-car topology counts changed")
assert(#road.suspensions == 4
    and road.contactUnits[1].suspension == road.suspensions[1].id,
    "contacts must reference reusable suspension components")
assert(road.contactUnits[1].radiusM > 0.0
    and road.suspensions[1].springRateNPerM > 0.0,
    "runtime wheel/suspension parameters were not authored")
assert(road.suspensions[1].provider == "linear_raycast_v1",
    "road-car draft must name the current native suspension provider")
assert(road.suspensions[1].bumpHighSpeedDampingNsPerM >= 0.0
    and road.suspensions[1].bumpStopRateNPerM > 0.0
    and road.suspensions[1].droopStopRateNPerM > 0.0,
    "non-linear damper and travel-limit parameters were not authored")
assert(#road.suspensions[1].steeringAxis == 3
    and road.suspensions[1].steeringAxis[2] == 1.0
    and type(road.suspensions[1].staticCamberDegrees) == "number"
    and type(road.suspensions[1].camberGainDegreesPerM) == "number"
    and type(road.suspensions[1].staticToeDegrees) == "number"
    and type(road.suspensions[1].toeGainDegreesPerM) == "number",
    "authoritative steering-axis and alignment curves were not authored")
assert(road.contactUnits[1].effectiveUnsprungMassKg > 0.0
    and road.contactUnits[1].tireRadialStiffnessNPerM > 0.0
    and road.contactUnits[1].tireRadialDampingNsPerM >= 0.0
    and road.contactUnits[1].maximumTireDeflectionM > 0.0
    and road.contactUnits[1].maximumTireNormalForceN > 0.0,
    "unsprung-mass and radial tire parameters were not authored")
assert(road.transmissions[1].finalDriveRatio > 0.0,
    "runtime transmission parameters were not authored")
assert(road.chassisFlex.enabled == true
    and road.chassisFlex.provider == "chassis_torsional_mode_v1"
    and road.chassisFlex.torsionalRigidityNmPerDegree == 8700.0
    and road.chassisFlex.provenance == "estimated_chassis_flex_closed_unibody_v1"
    and road.chassisFlex.confidence == 0.18,
    "road-car chassis-flex component/provenance was not preserved")

local motorcycle = BuildVehicleDefinitionV2(
    CreateVehicleWorkshopDraft("motorcycle"))
local motorcycleReport = ValidateVehicleDefinitionV2(motorcycle)
assert(motorcycleReport.valid, "motorcycle topology must remain exportable")
assert(not motorcycleReport.currentSolverReady,
    "motorcycle must not pretend current ray-wheel preview is complete")

local twin = BuildVehicleDefinitionV2(
    CreateVehicleWorkshopDraft("twin_engine"))
local twinReport = ValidateVehicleDefinitionV2(twin)
assert(twinReport.valid, "twin-engine topology must remain exportable")
assert(#twin.powerUnits == 2 and #twin.transmissions == 2,
    "twin-engine component graph was flattened")
assert(#twin.driveConnections == 2,
    "independent front/rear drive connections were lost")
assert(not twinReport.currentSolverReady,
    "multi-power-unit topology must be marked as future native work")

local broken = BuildVehicleDefinitionV2(CreateVehicleWorkshopDraft("road_car"))
broken.contactUnits[1].mountBody = "missing_body"
local brokenReport = ValidateVehicleDefinitionV2(broken)
assert(not brokenReport.valid,
    "invalid component references must fail structural validation")

local brokenSuspension = BuildVehicleDefinitionV2(
    CreateVehicleWorkshopDraft("road_car"))
brokenSuspension.contactUnits[1].suspension = "missing_suspension"
assert(not ValidateVehicleDefinitionV2(brokenSuspension).valid,
    "invalid suspension references must fail structural validation")

local brokenGeometry = BuildVehicleDefinitionV2(
    CreateVehicleWorkshopDraft("road_car"))
brokenGeometry.suspensions[1].steeringAxis = { 0.0, 0.0, 0.0 }
assert(not ValidateVehicleDefinitionV2(brokenGeometry).valid,
    "a zero steering axis must fail structural validation")

local brokenFlex = BuildVehicleDefinitionV2(
    CreateVehicleWorkshopDraft("road_car"))
brokenFlex.chassisFlex.torsionalRigidityNmPerDegree = 0.0
assert(not ValidateVehicleDefinitionV2(brokenFlex).valid,
    "invalid chassis-flex parameters must fail structural validation")

local formula = BuildVehicleDefinitionV2(CreateVehicleWorkshopDraft("formula"))
local formulaReport = ValidateVehicleDefinitionV2(formula)
assert(formulaReport.valid and not formulaReport.currentSolverReady,
    "formula pushrod suspension must remain authored without fake support")

local serialized = SerializeVehicleDefinitionV2(twin)
assert(string.find(serialized, "schemaVersion = 2", 1, true),
    "serializer omitted the schema version")
assert(string.find(serialized, "power_unit_2", 1, true),
    "serializer omitted the second power unit")
local roadSerialized = SerializeVehicleDefinitionV2(road)
assert(string.find(roadSerialized, "steeringAxis", 1, true)
    and string.find(roadSerialized, "camberGainDegreesPerM", 1, true)
    and string.find(roadSerialized, "toeGainDegreesPerM", 1, true),
    "serializer omitted suspension geometry data")

print("PASS VehicleDefinitionV2 semantic regression")
