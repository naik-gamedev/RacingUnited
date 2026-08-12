-- Chassis structural-compliance bridge.
-- The native solver owns the actual torsional mode. Racing United only supplies
-- vehicle data, provenance/confidence and creator-facing telemetry.
vehicleChassisFlexTelemetry = vehicleChassisFlexTelemetry or {}

function ApplyDefinitionChassisFlex()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    local flex = PrototypeCarDefinition and PrototypeCarDefinition.chassisFlex
    if flex == nil or flex.enabled == false then
        return Vehicle.SetChassisTorsionalCompliance(
            nativeVehicle,
            false,
            10000.0, 12000.0, 500.0,
            0.45, 1.20, -1.20, 1.0)
    end

    local ok = Vehicle.SetChassisTorsionalCompliance(
        nativeVehicle,
        true,
        flex.torsionalRigidityNmPerDegree or 10000.0,
        flex.torsionalDampingNmsPerRad or 12000.0,
        flex.effectiveTorsionalInertiaKgM2 or 500.0,
        flex.torsionAxisLocalY or 0.45,
        flex.frontReferenceLocalZ or 1.20,
        flex.rearReferenceLocalZ or -1.20,
        flex.maximumTwistDegrees or 1.0)
    if not ok then
        vehicleMessage = "VEHICLE ERROR: " .. Vehicle.GetLastError()
    end
    return ok
end

function RefreshChassisFlexTelemetry()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        vehicleChassisFlexTelemetry = {}
        return
    end
    vehicleChassisFlexTelemetry = Vehicle.GetChassisFlexState(nativeVehicle) or {}
end

function RebuildEstimatedChassisFlex()
    local reference = PrototypeCarDefinition.referenceGeometry or {}
    local chassis = PrototypeCarDefinition.chassis or {}
    local configured = PrototypeCarDefinition.chassisFlex or {}
    local estimate, errorText = Vehicle.EstimateChassisFlex(
        chassis.massKg or 1100.0,
        reference.wheelbaseM or 2.50,
        reference.frontTrackM or 1.50,
        reference.rearTrackM or 1.50,
        chassis.centerOfMassLocal and chassis.centerOfMassLocal[2] or 0.50,
        configured.modelYear or 2000,
        configured.construction or "unknown")
    if estimate == nil then
        vehicleMessage = "CHASSIS FLEX ESTIMATE ERROR: " .. tostring(errorText)
        return false
    end
    estimate.mountBody = "chassis"
    estimate.construction = configured.construction or "unknown"
    estimate.modelYear = configured.modelYear or 2000
    PrototypeCarDefinition.chassisFlex = estimate
    if nativeVehicle ~= 0 and Vehicle.Exists(nativeVehicle) then
        return ApplyDefinitionChassisFlex()
    end
    return true
end
