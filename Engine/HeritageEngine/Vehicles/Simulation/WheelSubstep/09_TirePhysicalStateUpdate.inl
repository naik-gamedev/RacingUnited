// CLEAN03B wheel-substep phase: 09_TirePhysicalStateUpdate
// Advance thermal, wear, contamination, wet/winter retained state, and persistent deformable SurfaceField state.
// This file is intentionally included inside VehicleSystem::simulateWheelSubstep().
// It preserves the validated lexical scope and statement order while making phase ownership explicit.

    tires::TireThermalOutput thermalAfter = thermalBefore;
    if (wheel.tireModel.thermal.enabled)
    {
        tires::TireThermalInput thermalInput;
        thermalInput.grounded = true;
        thermalInput.environmentTemperatureOverride = true;
        thermalInput.ambientTemperatureC = static_cast<VehicleScalar>(
            surfaces.environment().ambientTemperatureC);
        thermalInput.roadTemperatureC = state.surfaceTemperatureC;
        thermalInput.forwardSpeedMps = structuralLongitudinalSpeed;
        thermalInput.longitudinalSlipVelocityMps =
            circumferentialSpeed - structuralLongitudinalSpeed;
        thermalInput.lateralSlipVelocityMps = structuralLateralSpeed;
        thermalInput.longitudinalForceN =
            longitudinalForce - rollingResistanceForce;
        thermalInput.lateralForceN = lateralForce;
        thermalInput.radialDissipationWatts = state.tireRadialDissipationWatts;
        thermalInput.rollingResistanceDissipationWatts = std::abs(
            rollingResistanceForce * longitudinalSpeed);
        thermalInput.contactPatchAreaM2 = state.tireContactPatchArea;
        thermalInput.roadHeatTransferScale =
            (contaminationBefore.valid
                ? contaminationBefore.roadHeatTransferScale : VehicleScalar{1.0})
            * (wetBefore.valid
                ? wetBefore.roadHeatTransferScale : VehicleScalar{1.0});
        thermalAfter = tires::advanceTireThermal(
            wheel.tireModel.thermal, thermalInput,
            static_cast<VehicleScalar>(substepDeltaTime), wheel.thermalState);
        writeThermalTelemetry(thermalAfter);
    }
    if (wheel.tireModel.wear.enabled)
    {
        tires::TireWearInput wearAdvanceInput = wearReadInput;
        wearAdvanceInput.grounded = true;
        wearAdvanceInput.wheelRotationDegrees = state.wheelRotationDegrees;
        wearAdvanceInput.normalLoadN = suspensionForce;
        wearAdvanceInput.nominalLoadN = nominalLoadN;
        wearAdvanceInput.camberAngleRadians = radians(state.camberAngleDegrees);
        wearAdvanceInput.inflationPressurePa = thermalAfter.valid
            ? thermalAfter.inflationPressurePa : dynamicInflationPressurePa;
        wearAdvanceInput.bulkTreadTemperatureC = thermalAfter.valid
            ? thermalAfter.treadTemperatureC : wearReadInput.bulkTreadTemperatureC;
        wearAdvanceInput.slipDissipationWatts =
            std::abs((longitudinalForce - rollingResistanceForce)
                * (circumferentialSpeed - structuralLongitudinalSpeed))
            + std::abs(lateralForce * structuralLateralSpeed);
        wearAdvanceInput.wearEnergyMultiplier = static_cast<VehicleScalar>(
            surfaces.developmentControls().tireWearRateMultiplier);
        wearAfter = tires::advanceTireWear(
            wheel.tireModel.wear, wheel.tireModel.thermal, wearAdvanceInput,
            static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState);
        writeWearTelemetry(wearAfter);
    }
    if (wheel.tireModel.contamination.enabled)
    {
        contaminationInput.bulkTreadTemperatureC = thermalAfter.valid
            ? thermalAfter.treadTemperatureC : contaminationInput.bulkTreadTemperatureC;
        writeContaminationTelemetry(tires::advanceTireContamination(
            wheel.tireModel.contamination, wheel.tireModel.wear, contaminationInput,
            static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
    }
    if (wheel.tireModel.wetSurface.enabled)
    {
        wetInput.wheelRotationDegrees = state.wheelRotationDegrees;
        wetInput.inflationPressurePa = thermalAfter.valid
            ? thermalAfter.inflationPressurePa : wetInput.inflationPressurePa;
        wetInput.bulkTreadTemperatureC = thermalAfter.valid
            ? thermalAfter.treadTemperatureC : wetInput.bulkTreadTemperatureC;
        writeWetSurfaceTelemetry(tires::advanceTireWetSurface(
            wheel.tireModel.wetSurface, wheel.tireModel.wear, wetInput,
            static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
    }

    if (wheel.tireModel.winterSurface.enabled)
    {
        winterInput.wheelRotationDegrees = state.wheelRotationDegrees;
        winterInput.inflationPressurePa = thermalAfter.valid
            ? thermalAfter.inflationPressurePa : winterInput.inflationPressurePa;
        winterInput.bulkTreadTemperatureC = thermalAfter.valid
            ? thermalAfter.treadTemperatureC : winterInput.bulkTreadTemperatureC;
        writeWinterSurfaceTelemetry(tires::advanceTireWinterSurface(
            wheel.tireModel.winterSurface, wheel.tireModel.wear, winterInput,
            static_cast<VehicleScalar>(substepDeltaTime), wheel.wearState));
    }

    VehicleScalar presentationRutDepthDeltaM = 0.0;
    VehicleScalar presentationDisplacedVolumeDeltaM3 = 0.0;

    if (deformableTerrainBefore.valid
        && deformableTerrainSurfaceMaterial(activeTerrainMaterial))
    {
        // Persist plastic deformation into shared world state. This is the
        // key distinction from TIRE14: later wheels and later vehicles can
        // query the same rut/compaction history instead of receiving a fresh
        // synthetic terrain patch every contact.
        const auto fieldUpdate = tires::tireDeformableTerrainFieldUpdate(
            wheel.tireModel.deformableTerrainSurface,
            deformableTerrainInput, deformableTerrainBefore,
            static_cast<VehicleScalar>(substepDeltaTime));
        deformableTerrainField = surfaces.applyDeformable(hit.point, fieldUpdate);
        presentationRutDepthDeltaM = static_cast<VehicleScalar>(
            std::max(fieldUpdate.rutDepthDeltaM, 0.0f));
        presentationDisplacedVolumeDeltaM3 = static_cast<VehicleScalar>(
            std::max(fieldUpdate.displacedVolumeDeltaM3, 0.0f));
        writeDeformableTerrainTelemetry(
            deformableTerrainBefore, deformableTerrainField);
    }

    // TIRE15C converts real tire usage into shared deposited/loose-rubber
    // state. The field is specialized rubber state, not deformable terrain.
    // Loose rubber is migrated laterally into marble-rich cells; subsequent
    // wheels sample those cells before their force calculation and can pick
    // rubber back up through TIRE11 contamination.
    heritage::physics::rubber::TrackRubberContactInput rubberContact;
    rubberContact.material = hit.surfaceMaterial;
    rubberContact.normal = state.contactNormal;
    rubberContact.forward = wheelForward;
    rubberContact.deltaTimeSeconds = substepDeltaTime;
    rubberContact.wetness = static_cast<float>(hitSurfaceConditions.wetness);
    rubberContact.normalLoadN = static_cast<float>(suspensionForce);
    rubberContact.nominalLoadN = static_cast<float>(nominalLoadN);
    rubberContact.tireWidthM = static_cast<float>(
        std::max(contactGeometryDescription.nominalWidthM, VehicleScalar{0.08}));
    rubberContact.forwardSpeedMps = static_cast<float>(structuralLongitudinalSpeed);
    rubberContact.longitudinalSlipSpeedMps = static_cast<float>(
        circumferentialSpeed - structuralLongitudinalSpeed);
    rubberContact.lateralSlipSpeedMps = static_cast<float>(structuralLateralSpeed);
    rubberContact.slipDissipationWatts = static_cast<float>(
        std::abs((longitudinalForce - rollingResistanceForce)
            * (circumferentialSpeed - structuralLongitudinalSpeed))
        + std::abs(lateralForce * structuralLateralSpeed));
    rubberContact.treadWearDepthDeltaM =
        wearBefore.valid && wearAfter.valid
        ? static_cast<float>(std::max(
            wearBefore.averageTreadDepthM - wearAfter.averageTreadDepthM,
            VehicleScalar{0.0}))
        : 0.0f;
    rubberContact.treadWearFraction = static_cast<float>(
        wearAfter.valid ? wearAfter.wearFraction
            : (wearBefore.valid ? wearBefore.wearFraction : VehicleScalar{0.0}));
    rubberContact.treadTemperatureC = static_cast<float>(
        thermalAfter.valid ? thermalAfter.treadTemperatureC : VehicleScalar{20.0});
    rubberContact.compoundSheddingFactor = static_cast<float>(
        wheel.tireModel.wear.rubberSheddingPropensity);
    rubberContact.generationMultiplier = static_cast<float>(
        surfaces.developmentControls().rubberGenerationMultiplier);
    rubberContact.maturationMultiplier = static_cast<float>(
        surfaces.developmentControls().marbleMaturationMultiplier);
    const heritage::physics::rubber::TrackRubberSample trackRubberAfter =
        surfaces.applyTrackRubberContact(hit.point, rubberContact);
    state.tireTrackDepositedRubber = trackRubberAfter.depositedRubber;
    state.tireTrackLooseRubber = trackRubberAfter.looseRubber;
    state.tireTrackMarbleMaturity = trackRubberAfter.marbleMaturity;
    state.tireTrackRubberFrictionScale = trackRubberBefore.contactFrictionScale;
    state.tireTrackRubberPassCount = static_cast<VehicleScalar>(trackRubberAfter.passCount);

    // TIRE15B2 presentation consumes the same authoritative contact/world
    // state after all tire/terrain state updates. It is deliberately one-way:
    // clearing or reducing presentation fidelity cannot change tire forces.
    heritage::physics::SurfacePresentationContact presentationContact;
    presentationContact.normal = state.contactNormal;
    presentationContact.forward = wheelForward;
    presentationContact.material = hit.surfaceMaterial;
    presentationContact.deltaTimeSeconds = substepDeltaTime;
    presentationContact.forwardSpeedMps = static_cast<float>(structuralLongitudinalSpeed);
    presentationContact.lateralSpeedMps = static_cast<float>(structuralLateralSpeed);
    presentationContact.longitudinalSlipSpeedMps = static_cast<float>(
        circumferentialSpeed - structuralLongitudinalSpeed);
    presentationContact.normalLoadN = static_cast<float>(suspensionForce);
    presentationContact.wetness = static_cast<float>(hitSurfaceConditions.wetness);
    presentationContact.tireWidthM = static_cast<float>(
        std::max(contactGeometryDescription.nominalWidthM, VehicleScalar{0.08}));
    presentationContact.sourceStreamId = wheel.tireMarkStreamId;
    presentationContact.slipDissipationWatts = rubberContact.slipDissipationWatts;
    presentationContact.gripUtilization = static_cast<float>(state.gripUtilization);
    presentationContact.slipRatio = static_cast<float>(state.slipRatio);
    presentationContact.slipAngleDegrees = static_cast<float>(state.slipAngleDegrees);
    presentationContact.treadTemperatureC = rubberContact.treadTemperatureC;
    presentationContact.camberAngleRadians = static_cast<float>(
        radians(state.camberAngleDegrees));
    // SurfacePresentation stores its three lateral mark loads in world/tire-right
    // order (negative-right, centre, positive-right). TireWear names its bands
    // construction-relative inside/centre/outside, so mirror the shoulders for
    // left-side wheels before the renderer receives them.
    const VehicleScalar markInsideLoad = wearAfter.valid
        ? wearAfter.insideLoadFraction : VehicleScalar{0.30};
    const VehicleScalar markCenterLoad = wearAfter.valid
        ? wearAfter.centerLoadFraction : VehicleScalar{0.40};
    const VehicleScalar markOutsideLoad = wearAfter.valid
        ? wearAfter.outsideLoadFraction : VehicleScalar{0.30};
    const bool markInsideIsPositiveRight = wheel.description.localMount.x < 0.0f;
    presentationContact.insideLoadFraction = static_cast<float>(
        markInsideIsPositiveRight ? markOutsideLoad : markInsideLoad);
    presentationContact.centerLoadFraction = static_cast<float>(markCenterLoad);
    presentationContact.outsideLoadFraction = static_cast<float>(
        markInsideIsPositiveRight ? markInsideLoad : markOutsideLoad);
    presentationContact.freshRubberShed = trackRubberAfter.freshLooseGenerated;
    presentationContact.rubberFragmentSeverity =
        trackRubberAfter.freshFragmentSeverity;
    presentationContact.tireSurfaceSpeedMps = static_cast<float>(
        std::abs(circumferentialSpeed));
    presentationContact.rutDepthM = deformableTerrainField.valid
        ? deformableTerrainField.rutDepthM : 0.0f;
    presentationContact.rutDepthDeltaM = static_cast<float>(presentationRutDepthDeltaM);
    presentationContact.displacedVolumeDeltaM3 = static_cast<float>(
        presentationDisplacedVolumeDeltaM3);
    presentationContact.looseDepthM = deformableTerrainField.valid
        ? deformableTerrainField.looseDepthM : 0.0f;
    surfaces.recordContactPresentation(hit.point, presentationContact);

