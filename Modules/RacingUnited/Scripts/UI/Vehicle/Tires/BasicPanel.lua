-- Step 29H: common tire parameters and transient relaxation tuning.
function DrawVehicleTiresBasicPanel()
    UI.TextDisabled("ALL-WHEEL MANUAL OVERRIDE")
    local changed = false

    vehicleTire.peakFriction, changed = UI.SliderFloat(
        "Nominal peak friction", vehicleTire.peakFriction, 0.20, 2.20, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.longitudinalStiffness, changed = UI.SliderFloat(
        "Longitudinal stiffness", vehicleTire.longitudinalStiffness,
        10000.0, 180000.0, "%.0f N/slip")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.corneringStiffness, changed = UI.SliderFloat(
        "Cornering stiffness", vehicleTire.corneringStiffness,
        10000.0, 180000.0, "%.0f N/rad")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.loadSensitivity, changed = UI.SliderFloat(
        "Peak-friction load sensitivity", vehicleTire.loadSensitivity,
        0.0, 0.40, "%.3f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.stiffnessLoadExponent, changed = UI.SliderFloat(
        "Stiffness load exponent", vehicleTire.stiffnessLoadExponent,
        0.40, 1.20, "%.2f")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.longitudinalRelaxation, changed = UI.SliderFloat(
        "Longitudinal relaxation length", vehicleTire.longitudinalRelaxation,
        0.05, 1.50, "%.2f m")
    if changed then ApplyVehicleTireModel() end

    vehicleTire.lateralRelaxation, changed = UI.SliderFloat(
        "Lateral relaxation length", vehicleTire.lateralRelaxation,
        0.05, 1.50, "%.2f m")
    if changed then ApplyVehicleTireModel() end

    UI.Spacing()
    UI.TextDisabled("Small-slip stiffness and peak friction are now separated from curve shape.")
    UI.TextDisabled("Vertical load changes both peak grip and stiffness instead of only scaling force limits.")
end
