#include "PhysicsRegressionCommon.hpp"

#include <iomanip>
#include <iostream>

int main()
{
    using namespace heritage::tests;
    std::cout << std::fixed << std::setprecision(6);

    int failed = 0;
    const bool jobSystemPassed = jobSystemParallelForIsBoundedAndDeterministic();
    std::cout << (jobSystemPassed ? "PASS" : "FAIL")
        << " engine job-system parallel-for and nested safety\n";
    failed += jobSystemPassed ? 0 : 1;

    const bool parkedPassed = parkedVehicleStaysQuiet();
    std::cout << (parkedPassed ? "PASS" : "FAIL")
        << " parked vehicle stability\n";
    failed += parkedPassed ? 0 : 1;

    const bool restWakePassed = flatRestSleepsAndThrottleWakes();
    std::cout << (restWakePassed ? "PASS" : "FAIL")
        << " flat parked sleep and throttle wake\n";
    failed += restWakePassed ? 0 : 1;

    const bool brakeHeldSteeringPassed = brakeHeldSteeringWakesAndTracks();
    std::cout << (brakeHeldSteeringPassed ? "PASS" : "FAIL")
        << " brake-held parked steering wake and tracking\n";
    failed += brakeHeldSteeringPassed ? 0 : 1;

    const bool ratePassed = highRateSuspensionAgreesWithNativeRate();
    std::cout << (ratePassed ? "PASS" : "FAIL")
        << " high-rate suspension timing\n";
    failed += ratePassed ? 0 : 1;

    const bool parkingBrakePassed = parkingBrakeHoldsOnSlope();
    std::cout << (parkingBrakePassed ? "PASS" : "FAIL")
        << " parking brake slope hold\n";
    failed += parkingBrakePassed ? 0 : 1;

    const bool unbrakedSlopePassed = unbrakedVehicleRollsOnSlope();
    std::cout << (unbrakedSlopePassed ? "PASS" : "FAIL")
        << " unbraked slope roll\n";
    failed += unbrakedSlopePassed ? 0 : 1;

    const bool turnBrakePassed = turnThenBrakeRemainsStableAtLowSpeed();
    std::cout << (turnBrakePassed ? "PASS" : "FAIL")
        << " turn-then-brake low-speed stability\n";
    failed += turnBrakePassed ? 0 : 1;

    const bool centerOfMassPassed =
        rigidBodyCenterOfMassOffsetGeneratesTorque();
    std::cout << (centerOfMassPassed ? "PASS" : "FAIL")
        << " rigid-body centre-of-mass torque semantics\n";
    failed += centerOfMassPassed ? 0 : 1;

    const bool chassisRollPassed = vehicleChassisRollRespondsToCornering();
    std::cout << (chassisRollPassed ? "PASS" : "FAIL")
        << " chassis body-roll response and load transfer\n";
    failed += chassisRollPassed ? 0 : 1;

    const bool combinedDynamicsPassed =
        vehicleCombinedPitchRollYawRespondsToBrakingTurn();
    std::cout << (combinedDynamicsPassed ? "PASS" : "FAIL")
        << " combined pitch-roll-yaw and asymmetric suspension response\n";
    failed += combinedDynamicsPassed ? 0 : 1;

    const bool chassisFlexEstimatePassed =
        chassisFlexEstimatorProducesBoundedEpistemicEstimate();
    std::cout << (chassisFlexEstimatePassed ? "PASS" : "FAIL")
        << " epistemic chassis-flex estimation\n";
    failed += chassisFlexEstimatePassed ? 0 : 1;

    const bool chassisFlexCorePassed =
        chassisTorsionalComplianceRespondsToDiagonalLoad();
    std::cout << (chassisFlexCorePassed ? "PASS" : "FAIL")
        << " chassis torsional compliance mechanics\n";
    failed += chassisFlexCorePassed ? 0 : 1;

    const bool chassisFlexVehiclePassed =
        chassisFlexIntegratesWithHighRateVehicleDynamics();
    std::cout << (chassisFlexVehiclePassed ? "PASS" : "FAIL")
        << " high-rate chassis-flex vehicle integration\n";
    failed += chassisFlexVehiclePassed ? 0 : 1;

    const bool massEstimatePassed =
        vehicleMassPropertiesEstimatorProducesBoundedEstimate();
    std::cout << (massEstimatePassed ? "PASS" : "FAIL")
        << " epistemic vehicle mass-property estimation\n";
    failed += massEstimatePassed ? 0 : 1;

    const bool massComponentAccumulationPassed =
        vehicleMassComponentAccumulationUsesParallelAxisTheorem();
    std::cout << (massComponentAccumulationPassed ? "PASS" : "FAIL")
        << " installed-component mass-property accumulation\n";
    failed += massComponentAccumulationPassed ? 0 : 1;

    const bool explicitInertiaPassed =
        rigidBodyExplicitInertiaIsAuthoritative();
    std::cout << (explicitInertiaPassed ? "PASS" : "FAIL")
        << " explicit rigid-body inertia semantics\n";
    failed += explicitInertiaPassed ? 0 : 1;

    const bool fitmentPassed = wheelFitmentAndAlignmentAreReferenceSafe();
    std::cout << (fitmentPassed ? "PASS" : "FAIL")
        << " wheel fitment geometry and per-corner alignment\n";
    failed += fitmentPassed ? 0 : 1;

    const bool terrainDiagnosticsPassed =
        terrainContactDiagnosticsClassifyFailureModes();
    std::cout << (terrainDiagnosticsPassed ? "PASS" : "FAIL")
        << " terrain contact diagnostics and boundaries\n";
    failed += terrainDiagnosticsPassed ? 0 : 1;

    const bool staticTriangleRigidBodyPassed =
        staticTriangleRigidBodyContactsSettle();
    std::cout << (staticTriangleRigidBodyPassed ? "PASS" : "FAIL")
        << " static-triangle rigid-body contacts\n";
    failed += staticTriangleRigidBodyPassed ? 0 : 1;

    const bool chaseCameraOrbitPassed =
        chaseCameraOrbitPersistsAndReturnsOnForwardTravel();
    std::cout << (chaseCameraOrbitPassed ? "PASS" : "FAIL")
        << " chase-camera orbit persistence and forward return\n";
    failed += chaseCameraOrbitPassed ? 0 : 1;

    const bool vehicleCameraPassed =
        vehicleCameraAuthoringPoseAndFlyAreVehicleLocal();
    std::cout << (vehicleCameraPassed ? "PASS" : "FAIL")
        << " vehicle-camera local pose and fly authoring\n";
    failed += vehicleCameraPassed ? 0 : 1;

    const bool detachedCameraPassed =
        detachedFreeCameraCopiesCurrentFrameAndMovesInWorldSpace();
    std::cout << (detachedCameraPassed ? "PASS" : "FAIL")
        << " detached FP64 free camera world-space flight\n";
    failed += detachedCameraPassed ? 0 : 1;

    const bool dynamicChaseCameraPassed =
        chaseCameraDynamicOffsetsAreDampedAndBounded();
    std::cout << (dynamicChaseCameraPassed ? "PASS" : "FAIL")
        << " damped dynamic chase-camera motion offsets\n";
    failed += dynamicChaseCameraPassed ? 0 : 1;

    const bool rainMicrophysicsPassed =
        physicalRainPopulationAndWorldFieldAreDeterministic();
    std::cout << (rainMicrophysicsPassed ? "PASS" : "FAIL")
        << " physical rain population and world precipitation field\n";
    failed += rainMicrophysicsPassed ? 0 : 1;

    const bool dynamicsLabPassed = dynamicsLabCapturesHighRateTelemetry();
    std::cout << (dynamicsLabPassed ? "PASS" : "FAIL")
        << " high-rate vehicle dynamics laboratory\n";
    failed += dynamicsLabPassed ? 0 : 1;

    const bool unsprungMassPassed =
        unsprungMassSettlesAndRespondsToRoadStep();
    std::cout << (unsprungMassPassed ? "PASS" : "FAIL")
        << " scalar unsprung-mass wheel-hop response\n";
    failed += unsprungMassPassed ? 0 : 1;

    const bool steeringPassed =
        steeringDirectionAndAckermannAreSymmetric();
    std::cout << (steeringPassed ? "PASS" : "FAIL")
        << " steering direction and Ackermann symmetry\n";
    failed += steeringPassed ? 0 : 1;

    const bool suspensionGeometryPassed =
        suspensionGeometryProducesAuthoritativePose();
    std::cout << (suspensionGeometryPassed ? "PASS" : "FAIL")
        << " authoritative suspension upright pose\n";
    failed += suspensionGeometryPassed ? 0 : 1;

    const bool macPhersonPassed =
        macPhersonHardpointKinematicsAreDeterministic();
    std::cout << (macPhersonPassed ? "PASS" : "FAIL")
        << " MacPherson hardpoint kinematics\n";
    failed += macPhersonPassed ? 0 : 1;

    const bool assistedMacPhersonPassed =
        assistedMacPhersonEstimateIsPlausibleAndMirrored();
    std::cout << (assistedMacPhersonPassed ? "PASS" : "FAIL")
        << " assisted MacPherson hardpoint estimation\n";
    failed += assistedMacPhersonPassed ? 0 : 1;

    const bool assistedMacPhersonVehiclePassed =
        assistedFrontMacPhersonVehicleStaysStable();
    std::cout << (assistedMacPhersonVehiclePassed ? "PASS" : "FAIL")
        << " assisted MacPherson front vehicle stability\n";
    failed += assistedMacPhersonVehiclePassed ? 0 : 1;

    const bool trailingArmPassed =
        trailingArmTorsionBarKinematicsAreDeterministic();
    std::cout << (trailingArmPassed ? "PASS" : "FAIL")
        << " trailing-arm torsion-bar kinematics\n";
    failed += trailingArmPassed ? 0 : 1;

    const bool fullSuspensionPassed =
        assistedFrontRearSuspensionVehicleStaysStable();
    std::cout << (fullSuspensionPassed ? "PASS" : "FAIL")
        << " assisted front+rear suspension vehicle stability\n";
    failed += fullSuspensionPassed ? 0 : 1;

    const bool antiRollBarPassed =
        suspensionAntiRollBarCouplesWheelPairs();
    std::cout << (antiRollBarPassed ? "PASS" : "FAIL")
        << " reusable suspension anti-roll-bar coupling\n";
    failed += antiRollBarPassed ? 0 : 1;

    const bool definitionCompilerPassed =
        vehicleDefinitionCompilerAndLoaderWork();
    std::cout << (definitionCompilerPassed ? "PASS" : "FAIL")
        << " native vehicle-definition compiler and loader\n";
    failed += definitionCompilerPassed ? 0 : 1;

    const bool mf62RoadPassed = magicFormula62RoadCoreBehaves();
    std::cout << (mf62RoadPassed ? "PASS" : "FAIL")
        << " MF6.2 road force/moment core\n";
    failed += mf62RoadPassed ? 0 : 1;

    const bool tireCalibrationLabPassed =
        tireCalibrationLabProducesDeterministicSweeps();
    std::cout << (tireCalibrationLabPassed ? "PASS" : "FAIL")
        << " deterministic TIRE18 calibration sweeps\n";
    failed += tireCalibrationLabPassed ? 0 : 1;

    const bool tireAcceptancePassed =
        tireCalibrationAcceptanceRejectsOutOfEnvelopeChanges();
    std::cout << (tireAcceptancePassed ? "PASS" : "FAIL")
        << " provenance-labelled tire calibration acceptance envelopes\n";
    failed += tireAcceptancePassed ? 0 : 1;

    const bool tireScenarioLabPassed =
        tireScenarioLabProducesStatefulEvidence();
    std::cout << (tireScenarioLabPassed ? "PASS" : "FAIL")
        << " deterministic TIRE18 stateful scenarios\n";
    failed += tireScenarioLabPassed ? 0 : 1;

    const bool tireDistributedContactPassed =
        tireDistributedContactPatchIntegratesLocalShear();
    std::cout << (tireDistributedContactPassed ? "PASS" : "FAIL")
        << " bounded distributed tire contact integration\n";
    failed += tireDistributedContactPassed ? 0 : 1;

    const bool tireFleetBenchmarkPassed =
        tireFleetBenchmarkExecutesBoundedWork();
    std::cout << (tireFleetBenchmarkPassed ? "PASS" : "FAIL")
        << " executable 150-car / 600-tire workload benchmark\n";
    failed += tireFleetBenchmarkPassed ? 0 : 1;

    const bool mf62TurnSlipPassed = magicFormula62TurnSlipReducesGripAndTrail();
    std::cout << (mf62TurnSlipPassed ? "PASS" : "FAIL")
        << " MF6.2 turn-slip force/moment modifiers\n";
    failed += mf62TurnSlipPassed ? 0 : 1;

    const bool mf62MotorcyclePassed =
        magicFormula62MotorcycleLargeCamberBehaves();
    std::cout << (mf62MotorcyclePassed ? "PASS" : "FAIL")
        << " MF6.2 motorcycle large-camber contour path\n";
    failed += mf62MotorcyclePassed ? 0 : 1;

    const bool tireTransientPassed = tireRelaxationDynamicsAreRateStable();
    std::cout << (tireTransientPassed ? "PASS" : "FAIL")
        << " tire relaxation dynamics rate stability\n";
    failed += tireTransientPassed ? 0 : 1;

    const bool tireContactPatchPassed = tireContactPatchParkingTwistIsRateStable();
    std::cout << (tireContactPatchPassed ? "PASS" : "FAIL")
        << " standstill contact-patch torsion rate stability\n";
    failed += tireContactPatchPassed ? 0 : 1;

    const bool tireContactGeometryPassed =
        tireContactGeometryEffectiveRadiusAndFootprintBehave();
    std::cout << (tireContactGeometryPassed ? "PASS" : "FAIL")
        << " MF6.2 loaded/effective radius and finite footprint geometry\n";
    failed += tireContactGeometryPassed ? 0 : 1;

    const bool tireFlexibleRingFieldPassed =
        tireFlexibleRingFieldIsSmoothBoundedAndAsymmetric();
    std::cout << (tireFlexibleRingFieldPassed ? "PASS" : "FAIL")
        << " single-authority flexible-ring deformation field\n";
    failed += tireFlexibleRingFieldPassed ? 0 : 1;

    const bool tireRigidRingPassed = tireRigidRingStructuralModesAreRateStable();
    std::cout << (tireRigidRingPassed ? "PASS" : "FAIL")
        << " SWIFT-like rigid-ring structural mode rate stability\n";
    failed += tireRigidRingPassed ? 0 : 1;

    const bool tireRoadEnvelopePassed = tireRoadEnvelopeFiltersShortObstacle();
    std::cout << (tireRoadEnvelopePassed ? "PASS" : "FAIL")
        << " adaptive 2D road-enveloping footprint filter\n";
    failed += tireRoadEnvelopePassed ? 0 : 1;

    const bool tireThermalPassed =
        tireThermalPressureAndGripStateAreRateStable();
    std::cout << (tireThermalPassed ? "PASS" : "FAIL")
        << " tire thermal/pressure energy-state rate stability\n";
    failed += tireThermalPassed ? 0 : 1;

    const bool tireFailurePassed =
        tireFailurePressureLossAndStructuralStagesBehave();
    std::cout << (tireFailurePassed ? "PASS" : "FAIL")
        << " persistent puncture/blowout gas and structural progression\n";
    failed += tireFailurePassed ? 0 : 1;

    const bool tireSpatialTreadPassed =
        tireSpatialTreadThermalWearAndFlatSpotBehave();
    std::cout << (tireSpatialTreadPassed ? "PASS" : "FAIL")
        << " 48-cell spatial tread temperature/wear and flat-spot state\n";
    failed += tireSpatialTreadPassed ? 0 : 1;

    const bool tireContaminationPassed =
        tireTreadContaminationPickupAndCleaningBehave();
    std::cout << (tireContaminationPassed ? "PASS" : "FAIL")
        << " 48-cell tread contamination pickup and self-cleaning\n";
    failed += tireContaminationPassed ? 0 : 1;

    const bool tireWetSurfacePassed =
        tireWetSurfaceHydroplaningAndDrainageBehave();
    std::cout << (tireWetSurfacePassed ? "PASS" : "FAIL")
        << " wet hard-surface drainage and progressive hydroplaning\n";
    failed += tireWetSurfacePassed ? 0 : 1;

    const bool tireWinterSurfacePassed =
        tireWinterSurfaceIceSnowAndStudsBehave();
    std::cout << (tireWinterSurfacePassed ? "PASS" : "FAIL")
        << " compacted-snow / hard-ice winter tire interaction\n";
    failed += tireWinterSurfacePassed ? 0 : 1;

    const bool tireShallowGranularPassed =
        tireShallowGranularGravelDirtBehaves();
    std::cout << (tireShallowGranularPassed ? "PASS" : "FAIL")
        << " shallow gravel / hard-dirt granular tire interaction\n";
    failed += tireShallowGranularPassed ? 0 : 1;

    const bool surfaceWorldPassed =
        surfaceWorldGlobalAddressingAndChunkCacheBehave();
    std::cout << (surfaceWorldPassed ? "PASS" : "FAIL")
        << " world-owned chunked SurfaceWorld addressing\n";
    failed += surfaceWorldPassed ? 0 : 1;

    const bool dynamicSurfaceBakePassed =
        dynamicSurfaceStaticBakeSeparatesSheetsAndCaches();
    std::cout << (dynamicSurfaceBakePassed ? "PASS" : "FAIL")
        << " DSURF01 static chunk/sheet bake, curb isolation and cache\n";
    failed += dynamicSurfaceBakePassed ? 0 : 1;

    const bool dynamicSurfacePagePoolPassed =
        dynamicSurfacePagePoolIsPersistentBudgetedAndLruSafe();
    std::cout << (dynamicSurfacePagePoolPassed ? "PASS" : "FAIL")
        << " DSURF02 persistent budgeted Dynamic Surface page residency\n";
    failed += dynamicSurfacePagePoolPassed ? 0 : 1;

    const bool dynamicSurfaceHydroResidencyPassed =
        dynamicSurfaceHydroResidencyUsesRealSurfacePagesAndNearestSources();
    std::cout << (dynamicSurfaceHydroResidencyPassed ? "PASS" : "FAIL")
        << " DSURF03 real-surface Hydro residency and multi-source selection\n";
    failed += dynamicSurfaceHydroResidencyPassed ? 0 : 1;

    const bool dynamicSurfaceHydroConservationPassed =
        dynamicSurfaceHydrologyConservesCappedVolume();
    std::cout << (dynamicSurfaceHydroConservationPassed ? "PASS" : "FAIL")
        << " DSURF03B capped rain/flow Hydro mass conservation\n";
    failed += dynamicSurfaceHydroConservationPassed ? 0 : 1;

    const bool dynamicSurfaceHydroAuthorityPassed =
        dynamicSurfaceHydrologyOwnsRainCoverAndTireClearing();
    std::cout << (dynamicSurfaceHydroAuthorityPassed ? "PASS" : "FAIL")
        << " DSURF03B Dynamic Surface rain/cover/tire Hydro authority\n";
    failed += dynamicSurfaceHydroAuthorityPassed ? 0 : 1;

    const bool dynamicSurfaceThermalPassed =
        dynamicSurfaceThermalIsSheetAwareAndTireHeated();
    std::cout << (dynamicSurfaceThermalPassed ? "PASS" : "FAIL")
        << " DSURF04 Dynamic Surface sheet-aware thermal/tire-heat authority\n";
    failed += dynamicSurfaceThermalPassed ? 0 : 1;

    const bool surfacePresentationPassed =
        surfacePresentationIsBoundedAndWorldAddressed();
    std::cout << (surfacePresentationPassed ? "PASS" : "FAIL")
        << " bounded world-addressed driven-surface presentation\n";
    failed += surfacePresentationPassed ? 0 : 1;

    const bool trackRubberPassed =
        trackRubberBuildsMigratesAndWashes();
    std::cout << (trackRubberPassed ? "PASS" : "FAIL")
        << " dynamic rubbering-in, marble migration and rain wash\n";
    failed += trackRubberPassed ? 0 : 1;

    const bool tireDeformableTerrainPassed =
        tireDeformableTerrainPersistenceBehaves();
    std::cout << (tireDeformableTerrainPassed ? "PASS" : "FAIL")
        << " persistent mud / sand / deep-snow terramechanics SurfaceField\n";
    failed += tireDeformableTerrainPassed ? 0 : 1;

    const bool tirePropertyFilePassed =
        tirePropertyFileImporterMapsMf62AndMotorcycleData();
    std::cout << (tirePropertyFilePassed ? "PASS" : "FAIL")
        << " MF6.2 .tir property-file import and provenance\n";
    failed += tirePropertyFilePassed ? 0 : 1;

    const bool tireFamilyBaselinePassed =
        tireFamilyBaselinesAreCoherentAndBrandNeutral();
    std::cout << (tireFamilyBaselinePassed ? "PASS" : "FAIL")
        << " TIRE17 specialty-family baseline generation and brand neutrality\n";
    failed += tireFamilyBaselinePassed ? 0 : 1;

    const bool tirePerformanceBiasPassed =
        tirePerformanceBiasesMapToMechanismsWithoutForceMultipliers();
    std::cout << (tirePerformanceBiasPassed ? "PASS" : "FAIL")
        << " TIRE17B creator biases map to physical tire mechanisms and preserve fitted data\n";
    failed += tirePerformanceBiasPassed ? 0 : 1;

    const bool tirePartRuntimePassed =
        tirePartsResolveAndAssignReusableFitments();
    std::cout << (tirePartRuntimePassed ? "PASS" : "FAIL")
        << " TIRE17C reusable tire parts resolve and assign with per-wheel cold pressure\n";
    failed += tirePartRuntimePassed ? 0 : 1;

    const bool vehicleAudioPassed = vehicleAudioSynthesisAndMixAreBounded();
    std::cout << (vehicleAudioPassed ? "PASS" : "FAIL")
        << " layered vehicle audio synthesis, response and bounds\n";
    failed += vehicleAudioPassed ? 0 : 1;

    const bool weatherAudioPassed = weatherAudioMixIsPhysicalSmoothAndBounded();
    std::cout << (weatherAudioPassed ? "PASS" : "FAIL")
        << " native weather audio crossfade and smoothing\n";
    failed += weatherAudioPassed ? 0 : 1;

    std::cout << (failed == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
        << " count=" << failed << '\n';
    return failed == 0 ? 0 : 1;
}
