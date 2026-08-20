// CLEAN03B wheel-substep phase: 01_TelemetryAndAirbornePolicy
// Define telemetry writers, airborne state advancement, missing-support classification, and contact transition policy.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    const auto writeThermalTelemetry = [&](const tires::TireThermalOutput& thermal) {
        if (!thermal.valid)
        {
            state.tireTreadTemperatureC = 20.0;
            state.tireCarcassTemperatureC = 20.0;
            state.tireGasTemperatureC = 20.0;
            state.tireRimTemperatureC = 20.0;
            state.tireInflationPressurePa = wheel.tireModel.inflationPressurePa;
            state.tireThermalFrictionScale = 1.0;
            state.tireThermalStiffnessScale = 1.0;
            state.tireSlipDissipationWatts = 0.0;
            state.tireThermalLossDissipationWatts = 0.0;
            state.tireRoadHeatFlowWatts = 0.0;
            state.tireAirHeatFlowWatts = 0.0;
            state.tireBrakeHeatInputWatts = 0.0;
            state.tireRimToCarcassHeatFlowWatts = 0.0;
            return;
        }
        state.tireTreadTemperatureC = thermal.treadTemperatureC;
        state.tireCarcassTemperatureC = thermal.carcassTemperatureC;
        state.tireGasTemperatureC = thermal.gasTemperatureC;
        state.tireRimTemperatureC = thermal.rimTemperatureC;
        state.tireInflationPressurePa = thermal.inflationPressurePa;
        state.tireThermalFrictionScale = thermal.frictionScale;
        state.tireThermalStiffnessScale = thermal.stiffnessScale;
        state.tireSlipDissipationWatts = thermal.slipDissipationWatts;
        state.tireThermalLossDissipationWatts = thermal.carcassDissipationWatts;
        state.tireRoadHeatFlowWatts = thermal.roadHeatFlowWatts;
        state.tireAirHeatFlowWatts = thermal.airHeatFlowWatts;
        state.tireBrakeHeatInputWatts = thermal.brakeHeatInputWatts;
        state.tireRimToCarcassHeatFlowWatts = thermal.rimToCarcassHeatFlowWatts;
    };
    const auto writeFailureTelemetry = [&](const tires::TireFailureOutput& failure) {
        const tires::TireFailureState& persistent = wheel.failureState;
        state.tireFailureStage = failure.valid
            ? failure.stage : tires::TireFailureStage::Healthy;
        state.tireFailureEventSerial = persistent.eventSerial;
        state.tireContainedGasMassRatio = failure.valid
            ? failure.containedGasMassRatio : VehicleScalar{1.0};
        state.tirePressurizedGasFraction = failure.valid
            ? failure.pressurizedGasFraction : VehicleScalar{1.0};
        state.tirePunctureAreaMm2 = persistent.punctureAreaM2 * VehicleScalar{1.0e6};
        state.tireEffectiveLeakAreaMm2 = failure.valid
            ? failure.effectiveLeakAreaM2 * VehicleScalar{1.0e6} : VehicleScalar{0.0};
        state.tireLeakMassFlowGramsPerSecond = failure.valid
            ? failure.leakMassFlowKgPerSecond * VehicleScalar{1000.0} : VehicleScalar{0.0};
        state.tireStructuralIntegrity = failure.valid
            ? failure.structuralIntegrity : VehicleScalar{1.0};
        state.tireTreadAttachment = failure.valid
            ? failure.treadAttachment : VehicleScalar{1.0};
        state.tireRimContactFraction = failure.valid
            ? failure.rimContactFraction : VehicleScalar{0.0};
        state.tireFailureEventElapsedSeconds = failure.valid
            ? failure.eventElapsedSeconds : VehicleScalar{0.0};
    };
    const auto writeWearTelemetry = [&](const tires::TireWearOutput& wear) {
        const VehicleScalar initialDepthMm = wheel.tireModel.wear.enabled
            ? wheel.tireModel.wear.initialTreadDepthM * VehicleScalar{1000.0}
            : VehicleScalar{7.0};
        if (!wear.valid)
        {
            state.tireTreadInsideSurfaceTemperatureC = state.tireTreadTemperatureC;
            state.tireTreadCenterSurfaceTemperatureC = state.tireTreadTemperatureC;
            state.tireTreadOutsideSurfaceTemperatureC = state.tireTreadTemperatureC;
            state.tireTreadHottestSurfaceTemperatureC = state.tireTreadTemperatureC;
            state.tireTreadInsideDepthMm = initialDepthMm;
            state.tireTreadCenterDepthMm = initialDepthMm;
            state.tireTreadOutsideDepthMm = initialDepthMm;
            state.tireTreadMinimumDepthMm = initialDepthMm;
            state.tireTreadWearFraction = 0.0;
            state.tireFlatSpotDepthMm = 0.0;
            state.tireFlatSpotSector = 0.0;
            state.tireAverageTreadRadiusLossMm = 0.0;
            state.tireContactTreadRadiusLossMm = 0.0;
            state.tireContactRadiusVariationMm = 0.0;
            state.tireSpatialFrictionScale = 1.0;
            state.tireTreadContactSector = 0.0;
            state.tireTreadHottestSector = 0.0;
            return;
        }
        state.tireTreadInsideSurfaceTemperatureC = wear.insideSurfaceTemperatureC;
        state.tireTreadCenterSurfaceTemperatureC = wear.centerSurfaceTemperatureC;
        state.tireTreadOutsideSurfaceTemperatureC = wear.outsideSurfaceTemperatureC;
        state.tireTreadHottestSurfaceTemperatureC = wear.hottestSurfaceTemperatureC;
        state.tireTreadInsideDepthMm = wear.insideAverageTreadDepthM * 1000.0;
        state.tireTreadCenterDepthMm = wear.centerAverageTreadDepthM * 1000.0;
        state.tireTreadOutsideDepthMm = wear.outsideAverageTreadDepthM * 1000.0;
        state.tireTreadMinimumDepthMm = wear.minimumTreadDepthM * 1000.0;
        state.tireTreadWearFraction = wear.wearFraction;
        state.tireFlatSpotDepthMm = wear.flatSpotDepthM * 1000.0;
        state.tireFlatSpotSector = static_cast<VehicleScalar>(wear.flatSpotSector);
        state.tireAverageTreadRadiusLossMm =
            wear.averageTreadRadiusLossM * VehicleScalar{1000.0};
        state.tireContactTreadRadiusLossMm =
            wear.contactTreadRadiusLossM * VehicleScalar{1000.0};
        state.tireContactRadiusVariationMm =
            wear.contactRadiusVariationM * VehicleScalar{1000.0};
        state.tireSpatialFrictionScale = wear.contactFrictionScale;
        state.tireTreadContactSector = static_cast<VehicleScalar>(wear.primaryContactSector);
        state.tireTreadHottestSector = static_cast<VehicleScalar>(wear.hottestSector);
    };
    const auto writeContaminationTelemetry = [&](
        const tires::TireContaminationOutput& contamination) {
        if (!contamination.valid)
        {
            state.tireContaminationFrictionScale = 1.0;
            state.tireContaminationTotal = 0.0;
            state.tireContaminationAverage = 0.0;
            state.tireOrganicContamination = 0.0;
            state.tireMineralContamination = 0.0;
            state.tireGravelFinesContamination = 0.0;
            state.tireRubberPickupContamination = 0.0;
            state.tireMudFilmContamination = 0.0;
            state.tireContaminationCleaningRate = 0.0;
            return;
        }
        state.tireContaminationFrictionScale = contamination.contactFrictionScale;
        state.tireContaminationTotal = contamination.contactTotal;
        state.tireContaminationAverage = contamination.averageTotal;
        state.tireOrganicContamination = contamination.contactOrganic;
        state.tireMineralContamination = contamination.contactMineral;
        state.tireGravelFinesContamination = contamination.contactGravelFines;
        state.tireRubberPickupContamination = contamination.contactRubberPickup;
        state.tireMudFilmContamination = contamination.contactMudFilm;
        state.tireContaminationCleaningRate = contamination.cleaningRatePerSecond;
    };
    const auto writeWetSurfaceTelemetry = [&](
        const tires::TireWetSurfaceOutput& wet) {
        if (!wet.valid)
        {
            state.tireRoadWaterDepthMm = 0.0;
            state.tireRetainedWaterDepthMm = 0.0;
            state.tireDrainageDemandRatio = 0.0;
            state.tireWaterWedgeFraction = 0.0;
            state.tireHydroplaningFraction = 0.0;
            state.tirePavementContactFraction = 1.0;
            state.tireHydrodynamicLiftN = 0.0;
            state.tireHydrodynamicDragN = 0.0;
            state.tireWetFrictionScale = 1.0;
            state.tireClassicalHydroplaningSpeedKph = 0.0;
            return;
        }
        state.tireRoadWaterDepthMm = wet.roadWaterDepthM * VehicleScalar{1000.0};
        state.tireRetainedWaterDepthMm =
            wet.contactRetainedWaterDepthM * VehicleScalar{1000.0};
        state.tireDrainageDemandRatio = wet.drainageDemandRatio;
        state.tireWaterWedgeFraction = wet.waterWedgeFraction;
        state.tireHydroplaningFraction = wet.hydroplaningFraction;
        state.tirePavementContactFraction = wet.pavementContactFraction;
        state.tireHydrodynamicLiftN = wet.hydrodynamicLiftN;
        state.tireHydrodynamicDragN = wet.hydrodynamicDragN;
        state.tireWetFrictionScale = wet.frictionScale;
        state.tireClassicalHydroplaningSpeedKph =
            wet.classicalPressureHydroplaningSpeedMps * VehicleScalar{3.6};
    };

    const auto writeWinterSurfaceTelemetry = [&](
        const tires::TireWinterSurfaceOutput& winter) {
        if (!winter.valid)
        {
            state.tireWinterSurfaceFraction = 0.0;
            state.tireSnowSurfaceFraction = 0.0;
            state.tireIceSurfaceFraction = 0.0;
            state.tireWinterFrictionScale = 1.0;
            state.tireWinterStiffnessScale = 1.0;
            state.tirePackedSnowFraction = 0.0;
            state.tireIceMeltFilmMicrometers = 0.0;
            state.tireStudFrictionContribution = 0.0;
            state.tireSnowInterlockContribution = 0.0;
            state.tireWinterSurfaceTemperatureC = -5.0;
            return;
        }
        state.tireWinterSurfaceFraction = winter.winterSurfaceFraction;
        state.tireSnowSurfaceFraction = winter.snowFraction;
        state.tireIceSurfaceFraction = winter.iceFraction;
        state.tireWinterFrictionScale = winter.frictionScale;
        state.tireWinterStiffnessScale = winter.stiffnessScale;
        state.tirePackedSnowFraction = winter.contactPackedSnowFraction;
        state.tireIceMeltFilmMicrometers = winter.iceMeltFilmDepthM * VehicleScalar{1.0e6};
        state.tireStudFrictionContribution = winter.studFrictionContribution;
        state.tireSnowInterlockContribution = winter.snowInterlockContribution;
        state.tireWinterSurfaceTemperatureC = winter.surfaceTemperatureC;
    };

    const auto writeShallowGranularTelemetry = [&](
        const tires::TireShallowGranularOutput& granular) {
        if (!granular.valid)
        {
            state.tireGranularSurfaceFraction = 0.0;
            state.tireGranularSinkageMm = 0.0;
            state.tireGranularContactPressureKPa = 0.0;
            state.tireGranularTreadEffectiveness = 0.0;
            state.tireGranularShearCapacityN = 0.0;
            state.tireGranularLongitudinalShearN = 0.0;
            state.tireGranularLateralShearN = 0.0;
            state.tireGranularBulldozingN = 0.0;
            state.tireGranularPlowingDragN = 0.0;
            state.tireGranularCompactionPowerW = 0.0;
            state.tireGranularFrictionScale = 1.0;
            return;
        }
        state.tireGranularSurfaceFraction = granular.granularSurfaceFraction;
        state.tireGranularSinkageMm = granular.sinkageM * VehicleScalar{1000.0};
        state.tireGranularContactPressureKPa =
            granular.contactPressurePa * VehicleScalar{0.001};
        state.tireGranularTreadEffectiveness = granular.treadEffectiveness;
        state.tireGranularShearCapacityN = granular.soilShearCapacityN;
        state.tireGranularLongitudinalShearN =
            granular.longitudinalShearForceN;
        state.tireGranularLateralShearN = granular.lateralShearForceN;
        state.tireGranularBulldozingN = granular.lateralBulldozingForceN;
        state.tireGranularPlowingDragN = granular.plowingDragN;
        state.tireGranularCompactionPowerW = granular.compactionPowerW;
        state.tireGranularFrictionScale = granular.frictionScale;
    };

    const auto writeDeformableTerrainTelemetry = [&](
        const tires::TireDeformableTerrainOutput& terrain,
        const heritage::physics::SurfaceFieldSample& field) {
        if (!terrain.valid)
        {
            state.tireTerrainSurfaceFraction = 0.0;
            state.tireTerrainSinkageMm = 0.0;
            state.tireTerrainRutDepthMm = 0.0;
            state.tireTerrainCompaction = 0.0;
            state.tireTerrainMoisture = 0.0;
            state.tireTerrainLooseDepthMm = 0.0;
            state.tireTerrainShearCapacityN = 0.0;
            state.tireTerrainLongitudinalShearN = 0.0;
            state.tireTerrainLateralShearN = 0.0;
            state.tireTerrainBulldozingN = 0.0;
            state.tireTerrainPlowingDragN = 0.0;
            state.tireTerrainMfFrictionScale = 1.0;
            state.tireTerrainPassCount = 0.0;
            return;
        }
        state.tireTerrainSurfaceFraction = terrain.terrainSurfaceFraction;
        state.tireTerrainSinkageMm = terrain.totalSinkageM * VehicleScalar{1000.0};
        state.tireTerrainRutDepthMm = (field.valid
            ? static_cast<VehicleScalar>(field.rutDepthM)
            : terrain.persistentRutDepthM) * VehicleScalar{1000.0};
        state.tireTerrainCompaction = field.valid
            ? static_cast<VehicleScalar>(field.compaction) : terrain.compaction;
        state.tireTerrainMoisture = field.valid
            ? static_cast<VehicleScalar>(field.moisture) : terrain.moisture;
        state.tireTerrainLooseDepthMm = (field.valid
            ? static_cast<VehicleScalar>(field.looseDepthM)
            : terrain.looseDepthM) * VehicleScalar{1000.0};
        state.tireTerrainShearCapacityN = terrain.shearCapacityN;
        state.tireTerrainLongitudinalShearN = terrain.longitudinalTerrainForceN;
        state.tireTerrainLateralShearN = terrain.lateralTerrainForceN;
        state.tireTerrainBulldozingN = terrain.lateralBulldozingForceN;
        state.tireTerrainPlowingDragN = terrain.plowingDragN;
        state.tireTerrainMfFrictionScale = terrain.mfFrictionScale;
        state.tireTerrainPassCount = field.valid
            ? static_cast<VehicleScalar>(field.passCount) : VehicleScalar{0.0};
    };

    tires::TireThermalOutput thermalBefore = tires::evaluateTireThermalState(
        wheel.tireModel.thermal, wheel.thermalState);
    writeThermalTelemetry(thermalBefore);
    tires::TireFailureInput failureReadInput;
    failureReadInput.grounded = state.grounded;
    failureReadInput.ambientPressurePa = wheel.tireModel.thermal.ambientPressurePa;
    failureReadInput.referenceGaugePressurePa =
        wheel.tireModel.thermal.referenceGaugePressurePa;
    failureReadInput.referenceTemperatureC =
        wheel.tireModel.thermal.referenceTemperatureC;
    failureReadInput.gasTemperatureC = thermalBefore.valid
        ? thermalBefore.gasTemperatureC : VehicleScalar{20.0};
    failureReadInput.inflationGaugePressurePa = thermalBefore.valid
        ? thermalBefore.inflationPressurePa : wheel.tireModel.inflationPressurePa;
    failureReadInput.identifiedReferencePressurePa =
        wheel.tireModel.referenceInflationPressurePa;
    failureReadInput.normalLoadN = state.normalForce;
    failureReadInput.nominalLoadN = wheel.tireModel.nominalLoad;
    failureReadInput.forwardSpeedMps = previousLongitudinalSpeed;
    failureReadInput.longitudinalSlipVelocityMps = 0.0;
    failureReadInput.lateralSlipVelocityMps = previousLateralSpeed;
    failureReadInput.radialDissipationWatts = state.tireRadialDissipationWatts;
    failureReadInput.carcassTemperatureC = thermalBefore.valid
        ? thermalBefore.carcassTemperatureC : VehicleScalar{20.0};
    tires::TireFailureOutput failureBefore = tires::evaluateTireFailureState(
        wheel.tireModel.failure, failureReadInput, wheel.failureState);
    writeFailureTelemetry(failureBefore);
    tires::TireWearInput wearReadInput;
    wearReadInput.grounded = false;
    wearReadInput.wheelRotationDegrees = state.wheelRotationDegrees;
    wearReadInput.nominalLoadN = wheel.tireModel.nominalLoad;
    wearReadInput.camberAngleRadians = radians(state.camberAngleDegrees);
    wearReadInput.inflationPressurePa = thermalBefore.valid
        ? thermalBefore.inflationPressurePa : wheel.tireModel.inflationPressurePa;
    wearReadInput.referencePressurePa =
        wheel.tireModel.referenceInflationPressurePa;
    wearReadInput.bulkTreadTemperatureC = thermalBefore.valid
        ? thermalBefore.treadTemperatureC : VehicleScalar{20.0};
    tires::TireWearOutput wearBefore = tires::evaluateTireWearState(
        wheel.tireModel.wear, wheel.tireModel.thermal, wearReadInput, wheel.wearState);
    writeWearTelemetry(wearBefore);
    tires::TireContaminationOutput contaminationBefore{};
    writeContaminationTelemetry(contaminationBefore);
    tires::TireWetSurfaceOutput wetBefore{};
    writeWetSurfaceTelemetry(wetBefore);
    tires::TireWinterSurfaceOutput winterBefore{};
    writeWinterSurfaceTelemetry(winterBefore);
    tires::TireShallowGranularOutput shallowGranularBefore{};
    writeShallowGranularTelemetry(shallowGranularBefore);
    tires::TireDeformableTerrainOutput deformableTerrainBefore{};
    heritage::physics::SurfaceFieldSample deformableTerrainField{};
    writeDeformableTerrainTelemetry(deformableTerrainBefore, deformableTerrainField);
    const auto advanceAirborneThermal = [&]() {
        tires::TireThermalOutput airborneThermal = thermalBefore;
        if (wheel.tireModel.thermal.enabled)
        {
            tires::TireThermalInput thermalInput;
            thermalInput.grounded = false;
            thermalInput.environmentTemperatureOverride = true;
            thermalInput.ambientTemperatureC = static_cast<VehicleScalar>(
                surfaces.environment().ambientTemperatureC);
            thermalInput.roadTemperatureC = static_cast<VehicleScalar>(
                surfaces.environment().surfaceTemperatureC);
            thermalInput.forwardSpeedMps = previousLongitudinalSpeed;
            thermalInput.ambientAirSpeedMps = static_cast<VehicleScalar>(
                surfaces.weatherOutput().windSpeedMps);
            thermalInput.brakeDissipationWatts = std::abs(
                state.appliedBrakeTorque * state.wheelAngularVelocity);
            airborneThermal = tires::advanceTireThermal(
                wheel.tireModel.thermal, thermalInput,
                static_cast<VehicleScalar>(substepDeltaTime), wheel.thermalState);
            tires::TireFailureInput failureInput = failureReadInput;
            failureInput.grounded = false;
            failureInput.normalLoadN = 0.0;
            failureInput.forwardSpeedMps = previousLongitudinalSpeed;
            failureInput.gasTemperatureC = airborneThermal.gasTemperatureC;
            failureInput.carcassTemperatureC = airborneThermal.carcassTemperatureC;
            failureInput.inflationGaugePressurePa = airborneThermal.inflationPressurePa;
            failureBefore = tires::advanceTireFailure(
                wheel.tireModel.failure, failureInput,
                static_cast<VehicleScalar>(substepDeltaTime), wheel.failureState);
            if (failureBefore.valid)
            {
                wheel.thermalState.containedGasMassRatio =
                    failureBefore.containedGasMassRatio;
                airborneThermal = tires::evaluateTireThermalState(
                    wheel.tireModel.thermal, wheel.thermalState);
            }
            writeThermalTelemetry(airborneThermal);
            writeFailureTelemetry(failureBefore);
        }
        if (wheel.tireModel.wear.enabled)
        {
            tires::TireWearInput wearInput = wearReadInput;
            wearInput.grounded = false;
            wearInput.bulkTreadTemperatureC = airborneThermal.valid
                ? airborneThermal.treadTemperatureC
                : wearReadInput.bulkTreadTemperatureC;
            wearInput.inflationPressurePa = airborneThermal.valid
                ? airborneThermal.inflationPressurePa
                : wearReadInput.inflationPressurePa;
            writeWearTelemetry(tires::advanceTireWear(
                wheel.tireModel.wear, wheel.tireModel.thermal, wearInput,
                static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
        }
        if (wheel.tireModel.wetSurface.enabled)
        {
            tires::TireWetSurfaceInput wetInput;
            wetInput.grounded = false;
            wetInput.wheelRotationDegrees = state.wheelRotationDegrees;
            wetInput.forwardSpeedMps = previousLongitudinalSpeed;
            wetInput.inflationPressurePa = airborneThermal.valid
                ? airborneThermal.inflationPressurePa
                : wheel.tireModel.inflationPressurePa;
            wetInput.referencePressurePa =
                wheel.tireModel.referenceInflationPressurePa;
            wetInput.currentAverageTreadDepthM = wearBefore.valid
                ? wearBefore.averageTreadDepthM
                : wheel.tireModel.wear.initialTreadDepthM;
            wetInput.initialTreadDepthM = wheel.tireModel.wear.initialTreadDepthM;
            wetInput.minimumTreadDepthM = wheel.tireModel.wear.minimumTreadDepthM;
            writeWetSurfaceTelemetry(tires::advanceTireWetSurface(
                wheel.tireModel.wetSurface, wheel.tireModel.wear, wetInput,
                static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
        }
        if (wheel.tireModel.winterSurface.enabled)
        {
            tires::TireWinterSurfaceInput winterInput;
            winterInput.grounded = false;
            winterInput.wheelRotationDegrees = state.wheelRotationDegrees;
            winterInput.forwardSpeedMps = previousLongitudinalSpeed;
            winterInput.inflationPressurePa = airborneThermal.valid
                ? airborneThermal.inflationPressurePa
                : wheel.tireModel.inflationPressurePa;
            winterInput.referencePressurePa =
                wheel.tireModel.referenceInflationPressurePa;
            winterInput.currentAverageTreadDepthM = wearBefore.valid
                ? wearBefore.averageTreadDepthM
                : wheel.tireModel.wear.initialTreadDepthM;
            winterInput.initialTreadDepthM = wheel.tireModel.wear.initialTreadDepthM;
            winterInput.minimumTreadDepthM = wheel.tireModel.wear.minimumTreadDepthM;
            winterInput.bulkTreadTemperatureC = airborneThermal.valid
                ? airborneThermal.treadTemperatureC : VehicleScalar{20.0};
            writeWinterSurfaceTelemetry(tires::advanceTireWinterSurface(
                wheel.tireModel.winterSurface, wheel.tireModel.wear, winterInput,
                static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
        }
    };

    const auto classifyMissingSupport = [&]() {
        if (hitGround)
            return;

        if (rayDiagnostics.staticSceneLoaded
            && !rayDiagnostics.originInsideStaticSceneHorizontalBounds)
        {
            state.contactStatus =
                WheelContactStatus::OutsideStaticSceneBounds;
        }
        else if (collisions.count()
                <= collisions.countForBody(
                    vehicle.description.chassisBody)
            && !rayDiagnostics.staticSceneLoaded)
        {
            state.contactStatus = WheelContactStatus::NoWorldGeometry;
        }
        else if (previousContactStatus
                == WheelContactStatus::SurfaceBehindRayOrigin
            || previousContactStatus
                == WheelContactStatus::BeyondSuspensionReach)
        {
            state.contactStatus = previousContactStatus;
        }
        // Only a grounded-to-airborne transition earns the extra query.
        // A hit opposite the suspension direction means the support plane
        // crossed behind the ray origin between fixed steps.
        else if (wasGrounded)
        {
            heritage::physics::RaycastHit reverseHit;
            if (collisions.raycast(
                    wheelRayOriginWorld,
                    scale(suspensionDirection, -1.0f),
                    static_cast<float>(rayDistance * 2.0),
                    filter,
                    bodies,
                    reverseHit))
            {
                state.contactStatus =
                    WheelContactStatus::SurfaceBehindRayOrigin;
                return;
            }
            heritage::physics::RaycastHit extendedHit;
            if (collisions.raycast(
                    wheelRayOriginWorld,
                    suspensionDirection,
                    static_cast<float>(
                        rayDistance
                        + std::max(
                            static_cast<VehicleScalar>(description.radius),
                            0.50)),
                    filter,
                    bodies,
                    extendedHit)
                && extendedHit.distance - description.radius
                    > maximumLength)
            {
                state.rawSupportDistance =
                    extendedHit.distance - description.radius;
                state.contactStatus =
                    WheelContactStatus::BeyondSuspensionReach;
                return;
            }
            state.contactStatus = state.rayCandidateCount == 0
                ? WheelContactStatus::NoRayCandidates
                : WheelContactStatus::RayCandidatesMissed;
        }
        else if (state.rayCandidateCount == 0)
        {
            state.contactStatus = WheelContactStatus::NoRayCandidates;
        }
        else
        {
            state.contactStatus =
                WheelContactStatus::RayCandidatesMissed;
        }
    };
    const auto finalizeContactState = [&]() {
        if (wasGrounded && !state.grounded)
            ++state.contactLossTransitionCount;
    };
    classifyMissingSupport();
