-- Standalone semantic regression for the data-only topology contract.
-- The test runner loads Definitions/VehicleDefinitionV2.lua first.
Module = {
    AssetExists = function(_) return true end
}
PrototypeCarDefinition = {
    wheels = {
        { mount = { -0.70, 0.80, 1.20 } },
        { mount = { 0.70, 0.80, 1.20 } },
        { mount = { -0.70, 0.80, -1.20 } },
        { mount = { 0.70, 0.80, -1.20 } }
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
assert(road.transmissions[1].finalDriveRatio > 0.0,
    "runtime transmission parameters were not authored")

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

local formula = BuildVehicleDefinitionV2(CreateVehicleWorkshopDraft("formula"))
local formulaReport = ValidateVehicleDefinitionV2(formula)
assert(formulaReport.valid and not formulaReport.currentSolverReady,
    "formula pushrod suspension must remain authored without fake support")

local serialized = SerializeVehicleDefinitionV2(twin)
assert(string.find(serialized, "schemaVersion = 2", 1, true),
    "serializer omitted the schema version")
assert(string.find(serialized, "power_unit_2", 1, true),
    "serializer omitted the second power unit")

print("PASS VehicleDefinitionV2 semantic regression")
