-- Step 29H: advanced generalized sine/arctangent road-tire curve controls.
function DrawVehicleTiresCurvePanel()
    UI.TextDisabled("ALL-WHEEL MANUAL OVERRIDE")
    local changed = false

    vehicleTire.longitudinalShapeFactor, changed = UI.SliderFloat(
        "Longitudinal shape factor", vehicleTire.longitudinalShapeFactor,
        1.00, 1.95, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.longitudinalCurvatureFactor, changed = UI.SliderFloat(
        "Longitudinal curvature", vehicleTire.longitudinalCurvatureFactor,
        -0.80, 0.95, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.lateralShapeFactor, changed = UI.SliderFloat(
        "Lateral shape factor", vehicleTire.lateralShapeFactor,
        1.00, 1.95, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.lateralCurvatureFactor, changed = UI.SliderFloat(
        "Lateral curvature", vehicleTire.lateralCurvatureFactor,
        -0.80, 0.95, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.combinedSlipExponent, changed = UI.SliderFloat(
        "Combined-slip exponent", vehicleTire.combinedSlipExponent,
        1.20, 4.00, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.pneumaticTrail, changed = UI.SliderFloat(
        "Base pneumatic trail", vehicleTire.pneumaticTrail,
        0.0, 0.20, "%.3f m")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.pneumaticTrailFalloff, changed = UI.SliderFloat(
        "Pneumatic-trail falloff", vehicleTire.pneumaticTrailFalloff,
        0.0, 3.00, "%.2f")
    if changed then ApplyVehicleTireModel() end

    UI.Spacing()
    UI.TextDisabled("These parameters shape progressive breakaway and post-peak recovery.")
    UI.TextDisabled("They are native tire data, not a global handling multiplier.")
end
