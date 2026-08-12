-- Step 29H diagnostic road-tire profiles.
-- These are intentionally generic development templates, NOT measured tire
-- data for the future Peugeot, Ducati, truck, ATV, or any production vehicle.
-- They exist to prove that each wheel can own a different native tire model.
TirePresets = TirePresets or {}

TirePresets.prototype_road_front = {
    id = "prototype_road_front",
    displayName = "Prototype Road Front",
    parameterFile = "Data/Tires/PrototypeRoadFront_MF62.tir",
    parameterProvenance = "synthetic_tire02_compatibility_seed_not_measured",
    parameterConfidence = 0.10,
    nominalLoad = 3300.0,
    peakFriction = 1.16,
    longitudinalStiffness = 88000.0,
    corneringStiffness = 85000.0,
    loadSensitivity = 0.12,
    longitudinalRelaxation = 0.33,
    lateralRelaxation = 0.41,
    wheelInertia = 1.48,
    pneumaticTrail = 0.080,
    stiffnessLoadExponent = 0.85,
    longitudinalShapeFactor = 1.65,
    lateralShapeFactor = 1.30,
    longitudinalCurvatureFactor = 0.20,
    lateralCurvatureFactor = 0.15,
    combinedSlipExponent = 2.00,
    pneumaticTrailFalloff = 0.70,
    fallbackSurface = 0
}

TirePresets.prototype_road_rear = {
    id = "prototype_road_rear",
    displayName = "Prototype Road Rear",
    parameterFile = "Data/Tires/PrototypeRoadRear_MF62.tir",
    parameterProvenance = "synthetic_tire02_compatibility_seed_not_measured",
    parameterConfidence = 0.10,
    nominalLoad = 3600.0,
    peakFriction = 1.13,
    longitudinalStiffness = 94000.0,
    corneringStiffness = 76000.0,
    loadSensitivity = 0.12,
    longitudinalRelaxation = 0.38,
    lateralRelaxation = 0.48,
    wheelInertia = 1.64,
    pneumaticTrail = 0.070,
    stiffnessLoadExponent = 0.85,
    longitudinalShapeFactor = 1.65,
    lateralShapeFactor = 1.30,
    longitudinalCurvatureFactor = 0.20,
    lateralCurvatureFactor = 0.15,
    combinedSlipExponent = 2.00,
    pneumaticTrailFalloff = 0.70,
    fallbackSurface = 0
}
