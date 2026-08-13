-- Step 29H: focused live tire telemetry for the front-left wheel.
function DrawVehicleTiresLivePanel()
    if vehicleWheelTelemetry[1] == nil then
        UI.TextDisabled("No tire telemetry is available yet.")
        return
    end

    local wheel = vehicleWheelTelemetry[1]
    UI.TextDisabled("FRONT-LEFT LIVE TIRE | profile: " .. (vehicleWheelTireProfileNames[1] or "unknown"))
    UI.Text(string.format(
        "Grounded: %s | normal load: %.0f N | surface: %s",
        tostring(wheel.grounded), wheel.normalForce, wheel.surfaceName))
    UI.Text(string.format(
        "Raw slip ratio / angle: %.3f / %.2f deg",
        wheel.slipRatio, wheel.slipAngleDegrees))
    UI.Text(string.format(
        "Relaxed slip ratio / angle: %.3f / %.2f deg",
        wheel.relaxedSlipRatio, wheel.relaxedSlipAngleDegrees))
    UI.Text(string.format(
        "Pure Fx / Fy: %.0f / %.0f N",
        wheel.pureLongitudinalForce, wheel.pureLateralForce))
    UI.Text(string.format(
        "Combined Fx / Fy: %.0f / %.0f N",
        wheel.longitudinalForce, wheel.lateralForce))
    UI.Text(string.format(
        "Combined scale / grip used: %.3f / %.1f%%",
        wheel.combinedSlipScale, wheel.gripUtilization * 100.0))
    UI.Text(string.format(
        "Effective friction: %.3f | pneumatic trail: %.4f m",
        wheel.effectiveFriction, wheel.pneumaticTrail))
    UI.Text(string.format("Aligning torque: %.1f Nm", wheel.aligningTorque))
    UI.Text(string.format(
        "Turn slip: %.4f 1/m  normalized %.4f",
        wheel.turnSlipPerM or 0.0, wheel.normalizedTurnSlip or 0.0))
    UI.Text(string.format(
        "Contact twist: %.3f deg  parking Mz %.1f Nm  turn Mz %.1f Nm",
        wheel.contactPatchTwistDegrees or 0.0,
        wheel.parkingTurnMoment or 0.0, wheel.turnSlipMoment or 0.0))
    UI.Text(string.format(
        "Turn reductions Fx/Fy/Ky/trail: %.3f / %.3f / %.3f / %.3f",
        wheel.turnSlipLongitudinalReduction or 1.0,
        wheel.turnSlipLateralReduction or 1.0,
        wheel.turnSlipCorneringReduction or 1.0,
        wheel.turnSlipTrailReduction or 1.0))
    UI.Text(string.format(
        "Radius free / loaded / effective: %.2f / %.2f / %.2f mm",
        (wheel.tireFreeRollingRadius or 0.0) * 1000.0,
        (wheel.tireLoadedRadius or 0.0) * 1000.0,
        (wheel.tireEffectiveRollingRadius or 0.0) * 1000.0))
    UI.Text(string.format(
        "Finite footprint: %.1f x %.1f mm | %.1f cm^2",
        (wheel.tireContactPatchLength or 0.0) * 1000.0,
        (wheel.tireContactPatchWidth or 0.0) * 1000.0,
        (wheel.tireContactPatchArea or 0.0) * 10000.0))
    UI.Text(string.format(
        "Road envelope: %+6.2f mm | pitch %+5.2f deg | cross %+5.2f deg",
        (wheel.tireEnvelopeRoadOffset or 0.0) * 1000.0,
        wheel.tireEnvelopeSlopeDegrees or 0.0,
        wheel.tireEnvelopeCrossSlopeDegrees or 0.0))
    UI.Text(string.format(
        "2D footprint: %.0f/%.0f samples | support %.0f%% | refined %s",
        wheel.tireEnvelopeValidSamples or 0.0,
        wheel.tireFootprintTotalSamples or 0.0,
        (wheel.tireFootprintSupportedFraction or 0.0) * 100.0,
        tostring(wheel.tireFootprintRefined or false)))
    UI.Text(string.format(
        "Footprint roughness: %.2f mm | surface grip %.3f | spread %.3f",
        (wheel.tireFootprintRoughnessRange or 0.0) * 1000.0,
        wheel.tireFootprintSurfaceFriction or 1.0,
        wheel.tireFootprintSurfaceSpread or 0.0))
    UI.Text(string.format(
        "Rigid ring radial: %+6.2f mm @ %+6.3f m/s",
        (wheel.tireRingRadialOffset or 0.0) * 1000.0,
        wheel.tireRingRadialVelocity or 0.0))
    UI.Text(string.format(
        "Ring X/Y: %+6.2f / %+6.2f mm | Vx/Vy %+6.3f / %+6.3f m/s",
        (wheel.tireRingLongitudinalOffset or 0.0) * 1000.0,
        (wheel.tireRingLateralOffset or 0.0) * 1000.0,
        wheel.tireRingLongitudinalVelocity or 0.0,
        wheel.tireRingLateralVelocity or 0.0))
    UI.Text(string.format(
        "Ring yaw: %+6.3f deg @ %+7.2f deg/s | wind-up %+6.3f deg @ %+7.2f deg/s",
        wheel.tireRingYawDegrees or 0.0,
        wheel.tireRingYawRateDegreesPerSecond or 0.0,
        wheel.tireRingWindupDegrees or 0.0,
        wheel.tireRingWindupRateDegreesPerSecond or 0.0))
    UI.Text(string.format(
        "Thermal tread / carcass / gas: %.1f / %.1f / %.1f C",
        wheel.tireTreadTemperatureC or 20.0,
        wheel.tireCarcassTemperatureC or 20.0,
        wheel.tireGasTemperatureC or 20.0))
    UI.Text(string.format(
        "Pressure: %.1f PSI | thermal grip / stiffness: %.3f / %.3f",
        (wheel.tireInflationPressurePa or 220000.0) / 6894.757293168,
        wheel.tireThermalFrictionScale or 1.0,
        wheel.tireThermalStiffnessScale or 1.0))
    UI.Text(string.format(
        "Heat slip / carcass-loss: %.0f / %.0f W | road / air flow: %+.0f / %+.0f W",
        wheel.tireSlipDissipationWatts or 0.0,
        wheel.tireThermalLossDissipationWatts or 0.0,
        wheel.tireRoadHeatFlowWatts or 0.0,
        wheel.tireAirHeatFlowWatts or 0.0))
    UI.Text(string.format(
        "Tread surface I / C / O: %.1f / %.1f / %.1f C | hottest %.1f C",
        wheel.tireTreadInsideSurfaceTemperatureC or 20.0,
        wheel.tireTreadCenterSurfaceTemperatureC or 20.0,
        wheel.tireTreadOutsideSurfaceTemperatureC or 20.0,
        wheel.tireTreadHottestSurfaceTemperatureC or 20.0))
    UI.Text(string.format(
        "Tread depth I / C / O: %.2f / %.2f / %.2f mm | min %.2f mm",
        wheel.tireTreadInsideDepthMm or 7.0,
        wheel.tireTreadCenterDepthMm or 7.0,
        wheel.tireTreadOutsideDepthMm or 7.0,
        wheel.tireTreadMinimumDepthMm or 7.0))
    UI.Text(string.format(
        "Wear %.2f%% | flat spot %.3f mm | spatial grip %.3f | sector %.0f / hot %.0f",
        (wheel.tireTreadWearFraction or 0.0) * 100.0,
        wheel.tireFlatSpotDepthMm or 0.0,
        wheel.tireSpatialFrictionScale or 1.0,
        wheel.tireTreadContactSector or 0.0,
        wheel.tireTreadHottestSector or 0.0))
    UI.Text(string.format(
        "Tread radius loss avg / contact / variation: %.3f / %.3f / %+.3f mm",
        wheel.tireAverageTreadRadiusLossMm or 0.0,
        wheel.tireContactTreadRadiusLossMm or 0.0,
        wheel.tireContactRadiusVariationMm or 0.0))
    UI.Text(string.format(
        "Contamination contact / avg: %.1f%% / %.1f%% | grip %.3f | clean %.2f 1/s",
        (wheel.tireContaminationTotal or 0.0) * 100.0,
        (wheel.tireContaminationAverage or 0.0) * 100.0,
        wheel.tireContaminationFrictionScale or 1.0,
        wheel.tireContaminationCleaningRate or 0.0))
    UI.Text(string.format(
        "Pickup organic / dirt / gravel / rubber / mud: %.0f / %.0f / %.0f / %.0f / %.0f%%",
        (wheel.tireOrganicContamination or 0.0) * 100.0,
        (wheel.tireMineralContamination or 0.0) * 100.0,
        (wheel.tireGravelFinesContamination or 0.0) * 100.0,
        (wheel.tireRubberPickupContamination or 0.0) * 100.0,
        (wheel.tireMudFilmContamination or 0.0) * 100.0))
    UI.Text(string.format(
        "Track rubber bonded / loose / maturity: %.1f / %.1f / %.1f%% | grip %.3f",
        (wheel.tireTrackDepositedRubber or 0.0) * 100.0,
        (wheel.tireTrackLooseRubber or 0.0) * 100.0,
        (wheel.tireTrackMarbleMaturity or 0.0) * 100.0,
        wheel.tireTrackRubberFrictionScale or 1.0))

    UI.Text(string.format(
        "Water road / retained: %.2f / %.2f mm | drainage %.2f | wedge %.0f%%",
        wheel.tireRoadWaterDepthMm or 0.0,
        wheel.tireRetainedWaterDepthMm or 0.0,
        wheel.tireDrainageDemandRatio or 0.0,
        (wheel.tireWaterWedgeFraction or 0.0) * 100.0))
    UI.Text(string.format(
        "Hydro %.1f%% | pavement %.1f%% | wet grip %.3f | classic onset %.0f km/h",
        (wheel.tireHydroplaningFraction or 0.0) * 100.0,
        (wheel.tirePavementContactFraction or 1.0) * 100.0,
        wheel.tireWetFrictionScale or 1.0,
        wheel.tireClassicalHydroplaningSpeedKph or 0.0))
    UI.Text(string.format(
        "Hydrodynamic lift / water drag: %.0f / %.0f N",
        wheel.tireHydrodynamicLiftN or 0.0,
        wheel.tireHydrodynamicDragN or 0.0))

    UI.Text(string.format(
        "Winter footprint snow / ice: %.1f%% / %.1f%% | grip %.3f | stiff %.3f",
        (wheel.tireSnowSurfaceFraction or 0.0) * 100.0,
        (wheel.tireIceSurfaceFraction or 0.0) * 100.0,
        wheel.tireWinterFrictionScale or 1.0,
        wheel.tireWinterStiffnessScale or 1.0))
    UI.Text(string.format(
        "Packed snow %.1f%% | ice melt film %.1f um | surface %.1f C",
        (wheel.tirePackedSnowFraction or 0.0) * 100.0,
        wheel.tireIceMeltFilmMicrometers or 0.0,
        wheel.tireWinterSurfaceTemperatureC or -5.0))
    UI.Text(string.format(
        "Winter mechanical contribution studs / snow interlock: %.3f / %.3f",
        wheel.tireStudFrictionContribution or 0.0,
        wheel.tireSnowInterlockContribution or 0.0))

    UI.Text(string.format(
        "Granular footprint %.1f%% | sinkage %.1f mm | pressure %.0f kPa | tread eff %.3f",
        (wheel.tireGranularSurfaceFraction or 0.0) * 100.0,
        wheel.tireGranularSinkageMm or 0.0,
        wheel.tireGranularContactPressureKPa or 0.0,
        wheel.tireGranularTreadEffectiveness or 0.0))
    UI.Text(string.format(
        "Granular shear cap %.0f N | Fx %.0f N | Fy shear / bulldoze %.0f / %.0f N",
        wheel.tireGranularShearCapacityN or 0.0,
        wheel.tireGranularLongitudinalShearN or 0.0,
        wheel.tireGranularLateralShearN or 0.0,
        wheel.tireGranularBulldozingN or 0.0))
    UI.Text(string.format(
        "Granular plow %.0f N | compaction %.0f W | MF base grip %.3f",
        wheel.tireGranularPlowingDragN or 0.0,
        wheel.tireGranularCompactionPowerW or 0.0,
        wheel.tireGranularFrictionScale or 1.0))

    UI.Text(string.format(
        "Terrain footprint %.1f%% | sinkage %.1f mm | persistent rut %.1f mm",
        (wheel.tireTerrainSurfaceFraction or 0.0) * 100.0,
        wheel.tireTerrainSinkageMm or 0.0,
        wheel.tireTerrainRutDepthMm or 0.0))
    UI.Text(string.format(
        "Terrain compaction %.1f%% | moisture %.1f%% | loose depth %.0f mm | passes %.0f",
        (wheel.tireTerrainCompaction or 0.0) * 100.0,
        (wheel.tireTerrainMoisture or 0.0) * 100.0,
        wheel.tireTerrainLooseDepthMm or 0.0,
        wheel.tireTerrainPassCount or 0.0))
    UI.Text(string.format(
        "Terrain shear cap %.0f N | Fx %.0f N | Fy shear / bulldoze %.0f / %.0f N",
        wheel.tireTerrainShearCapacityN or 0.0,
        wheel.tireTerrainLongitudinalShearN or 0.0,
        wheel.tireTerrainLateralShearN or 0.0,
        wheel.tireTerrainBulldozingN or 0.0))
    UI.Text(string.format(
        "Terrain plow %.0f N | residual MF grip %.3f",
        wheel.tireTerrainPlowingDragN or 0.0,
        wheel.tireTerrainMfFrictionScale or 1.0))

    UI.Spacing()
    UI.TextDisabled("Pure forces show what each slip direction requested before combined-slip sharing.")
    UI.TextDisabled("Combined scale falls below 1.0 when acceleration/braking and cornering compete for grip.")
end
