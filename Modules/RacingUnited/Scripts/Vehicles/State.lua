-- Runtime state for the currently spawned prototype vehicle.
-- Configuration originates in Definitions/PrototypeCar.lua; this file holds
-- mutable handles, tuning values and telemetry for the active scene instance.
local definition = PrototypeCarDefinition

nativeVehicle = 0
nativeVehicleBody = 0
nativeVehicleCollider = 0

vehicleVisual = {
    assetPath = definition.visual.bodyAsset,
    fallbackAssetPath = definition.visual.fallbackBodyAsset,
    normalize = definition.visual.normalize,
    doubleSided = definition.visual.doubleSided,
    color = {
        definition.visual.color[1],
        definition.visual.color[2],
        definition.visual.color[3]
    },
    offset = {
        definition.visual.offset[1],
        definition.visual.offset[2],
        definition.visual.offset[3]
    },
    rotationDegrees = {
        definition.visual.rotationDegrees[1],
        definition.visual.rotationDegrees[2],
        definition.visual.rotationDegrees[3]
    },
    scale = definition.visual.scale,
    hideProxyWheels = definition.visual.hideProxyWheels,
    usingFallback = false
}
vehicleVisualMessage = "Player-car visual slot is ready"

vehicleWheelVisual = {
    enabled = definition.visual.articulatedWheels.enabled,
    normalize = definition.visual.articulatedWheels.normalize,
    doubleSided = definition.visual.articulatedWheels.doubleSided,
    color = {
        definition.visual.articulatedWheels.color[1],
        definition.visual.articulatedWheels.color[2],
        definition.visual.articulatedWheels.color[3]
    },
    radiusScale = definition.visual.articulatedWheels.radiusScale,
    widthScale = definition.visual.articulatedWheels.widthScale,
    rotationOffsetDegrees = {
        definition.visual.articulatedWheels.rotationOffsetDegrees[1],
        definition.visual.articulatedWheels.rotationOffsetDegrees[2],
        definition.visual.articulatedWheels.rotationOffsetDegrees[3]
    },
    assetPaths = {}
}
for index, wheel in ipairs(definition.wheels) do
    vehicleWheelVisual.assetPaths[index] = wheel.visualAsset
        or definition.visual.articulatedWheels.defaultAsset
end
vehicleWheelVisualMessage = "Articulated wheel slots are ready"

vehicleHighRateHertz = definition.solver.highRateHertz
vehicleMaximumDriveForce = definition.solver.maximumDriveForce
vehicleMaximumBrakeForce = definition.solver.maximumBrakeForce
vehicleMaximumSteerAngle = definition.steering.maximumAngleDegrees
vehicleAckermannPercent = definition.steering.ackermannPercent
vehicleSteeringRate = definition.steering.rateDegreesPerSecond
vehicleSteeringReturnRate = definition.steering.returnRateDegreesPerSecond
vehicleHighSpeedSteeringRateFactor = definition.steering.highSpeedRateFactor
vehicleHighSpeedReferenceMps = definition.steering.highSpeedReferenceMps
vehicleSteeringInput = 0.0
vehicleSteeringTarget = 0.0
vehicleSteeringCenter = 0.0
vehicleSteeringInner = 0.0
vehicleSteeringOuter = 0.0
vehicleDetectedWheelbase = 0.0
vehicleDetectedSteerTrack = 0.0
vehicleSteeringRateFactor = 1.0
vehicleTireFriction = definition.solver.tireFriction
vehicleLateralStiffness = definition.solver.lateralStiffness
vehicleRollingResistance = definition.solver.rollingResistance

vehicleTire = {
    nominalLoad = definition.tire.nominalLoad,
    peakFriction = definition.tire.peakFriction,
    longitudinalStiffness = definition.tire.longitudinalStiffness,
    corneringStiffness = definition.tire.corneringStiffness,
    loadSensitivity = definition.tire.loadSensitivity,
    longitudinalRelaxation = definition.tire.longitudinalRelaxation,
    lateralRelaxation = definition.tire.lateralRelaxation,
    wheelInertia = definition.tire.wheelInertia,
    pneumaticTrail = definition.tire.pneumaticTrail,
    stiffnessLoadExponent = definition.tire.stiffnessLoadExponent,
    longitudinalShapeFactor = definition.tire.longitudinalShapeFactor,
    lateralShapeFactor = definition.tire.lateralShapeFactor,
    longitudinalCurvatureFactor = definition.tire.longitudinalCurvatureFactor,
    lateralCurvatureFactor = definition.tire.lateralCurvatureFactor,
    combinedSlipExponent = definition.tire.combinedSlipExponent,
    pneumaticTrailFalloff = definition.tire.pneumaticTrailFalloff,
    fallbackSurface = definition.tire.fallbackSurface
}

vehicleDriverAids = {
    absEnabled = definition.driverAids.absEnabled,
    tractionControlEnabled = definition.driverAids.tractionControlEnabled,
    absTargetSlip = definition.driverAids.absTargetSlip,
    tractionTargetSlip = definition.driverAids.tractionTargetSlip,
    minimumSpeed = definition.driverAids.minimumSpeed,
    modulationRate = definition.driverAids.modulationRate,
    maximumHandbrakeTorque = definition.driverAids.maximumHandbrakeTorque,
    frontBrakeBias = definition.driverAids.frontBrakeBias,
    handbrakeInput = 0.0,
    absActiveWheels = 0,
    tractionActiveWheels = 0
}

vehicleSpeed = 0.0
vehicleGroundedWheels = 0
vehicleLastHighRateSteps = 0
vehicleTotalHighRateSteps = 0
vehicleIdleRpm = definition.powertrain.idleRpm
vehicleRedlineRpm = definition.powertrain.redlineRpm
vehicleMaximumEngineTorque = definition.powertrain.maximumEngineTorque
vehicleEngineBrakingTorque = definition.powertrain.engineBrakingTorque
vehicleFinalDriveRatio = definition.powertrain.finalDriveRatio
vehicleDrivetrainEfficiency = definition.powertrain.efficiency
vehicleShiftDuration = definition.powertrain.shiftDuration
vehicleClutchEngagementRate = definition.powertrain.clutchEngagementRate
vehicleReverseRatio = definition.powertrain.reverseRatio
vehicleForwardRatios = {}
for index, ratio in ipairs(definition.powertrain.forwardRatios) do
    vehicleForwardRatios[index] = ratio
end
vehicleDifferentialMode = definition.differential.mode
vehicleDifferentialBias = definition.differential.torqueBias
vehicleCurrentGear = 1
vehicleRequestedGear = 1
vehicleShifting = false
vehicleShiftTimeRemaining = 0.0
vehicleEngineRpm = vehicleIdleRpm
vehicleEngineTorque = 0.0
vehicleClutchEngagement = 0.0
vehicleClutchSlipRpm = 0.0
vehicleWheelCoupledRpm = 0.0
vehicleSelectedGearRatio = vehicleForwardRatios[1]
vehicleOutputTorque = 0.0
vehicleDrivenWheelSpeedDifferenceRpm = 0.0
vehicleForwardGearCount = #vehicleForwardRatios
vehicleMessage = "Step 29J.1 exact wheel-center presentation is ready"
vehicleWheelTelemetry = {}
vehicleWheelTireProfileNames = {}
for index, wheel in ipairs(definition.wheels) do
    vehicleWheelTireProfileNames[index] = wheel.tireProfile or "vehicle_default"
end
vehicleWheelMounts = {}
for index, wheel in ipairs(definition.wheels) do
    vehicleWheelMounts[index] = wheel.mount
end
