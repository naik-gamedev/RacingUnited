-- Racing United - epistemic vehicle mass-property authoring.
-- Native C++ owns the estimator and rigid-body math; Lua only chooses the
-- current vehicle evidence and applies the resulting explicit properties.

vehicleMassProperties = vehicleMassProperties or {
    message = "Mass properties not estimated yet",
    estimate = nil
}

local function CurrentFrontStaticLoadFraction()
    local chassis = PrototypeCarDefinition.chassis
    local geometry = PrototypeCarDefinition.referenceGeometry
    if chassis.frontStaticLoadFraction then
        return chassis.frontStaticLoadFraction
    end
    if chassis.centerOfMassLocal and geometry.wheelbaseM > 0.0 then
        return 0.5 + chassis.centerOfMassLocal[3] / geometry.wheelbaseM
    end
    return 0.5
end

local function HasStrongerAuthoredMassProperties()
    local chassis = PrototypeCarDefinition.chassis
    local inertia = chassis.inertiaLocalKgM2
    local provenance = chassis.massPropertiesProvenance or ""
    local hasInertia = type(inertia) == "table"
        and #inertia >= 3
        and inertia[1] ~= nil and inertia[2] ~= nil and inertia[3] ~= nil
    local estimated = string.sub(provenance, 1, 10) == "estimated_"
    return hasInertia and provenance ~= "" and not estimated
end

function RefreshPrototypeMassPropertiesEstimate(forceEstimate)
    if not forceEstimate and HasStrongerAuthoredMassProperties() then
        local chassis = PrototypeCarDefinition.chassis
        vehicleMassProperties.message = string.format(
            "Preserving authored mass properties | %s %.0f%% confidence",
            chassis.massPropertiesProvenance,
            (chassis.massPropertiesConfidence or 0.0) * 100.0)
        return true
    end
    if not Vehicle or not Vehicle.EstimateMassProperties then
        vehicleMassProperties.message = "Vehicle mass-property estimator API unavailable"
        return false
    end

    local chassis = PrototypeCarDefinition.chassis
    local geometry = PrototypeCarDefinition.referenceGeometry
    local estimate, errorMessage = Vehicle.EstimateMassProperties(
        chassis.massKg,
        geometry.wheelbaseM,
        geometry.frontTrackM,
        geometry.rearTrackM,
        chassis.centerOfMassLocal[2],
        CurrentFrontStaticLoadFraction(),
        chassis.leftStaticLoadFraction or 0.50,
        "road_car")
    if not estimate then
        vehicleMassProperties.message = errorMessage or "Mass-property estimate failed"
        return false
    end

    -- Keep the existing authoring COM exactly when present; MASS01's estimator
    -- reproduces it from axle load fraction but this avoids needless float churn.
    chassis.inertiaLocalKgM2 = estimate.inertiaLocalKgM2
    chassis.frontStaticLoadFraction = estimate.frontStaticMassKg / estimate.totalMassKg
    chassis.leftStaticLoadFraction = estimate.leftStaticMassKg / estimate.totalMassKg
    chassis.massPropertiesProvenance = estimate.provenance
    chassis.massPropertiesConfidence = estimate.confidence
    vehicleMassProperties.estimate = estimate
    vehicleMassProperties.message = string.format(
        "Mass %.1f kg | inertia P/Y/R %.0f / %.0f / %.0f kg m^2 | %s %.0f%% confidence",
        estimate.totalMassKg,
        estimate.inertiaLocalKgM2[1],
        estimate.inertiaLocalKgM2[2],
        estimate.inertiaLocalKgM2[3],
        estimate.provenance,
        estimate.confidence * 100.0)
    return true
end

function RebuildEstimatedPrototypeMassProperties()
    return RefreshPrototypeMassPropertiesEstimate(true)
end

function ApplyPrototypeRigidBodyMassProperties(body)
    if body == 0 then
        return false
    end
    if not RefreshPrototypeMassPropertiesEstimate() then
        return false
    end

    local chassis = PrototypeCarDefinition.chassis
    if not Physics.SetBodyMass(body, chassis.massKg) then
        vehicleMassProperties.message = Physics.GetLastError()
        return false
    end
    if not Physics.SetBodyCenterOfMassLocal(
        body,
        chassis.centerOfMassLocal[1],
        chassis.centerOfMassLocal[2],
        chassis.centerOfMassLocal[3]) then
        vehicleMassProperties.message = Physics.GetLastError()
        return false
    end
    if not Physics.SetBodyInertiaLocal(
        body,
        chassis.inertiaLocalKgM2[1],
        chassis.inertiaLocalKgM2[2],
        chassis.inertiaLocalKgM2[3]) then
        vehicleMassProperties.message = Physics.GetLastError()
        return false
    end
    return true
end

RefreshPrototypeMassPropertiesEstimate()
