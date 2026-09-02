#include "TirePropertyImportInternal.hpp"

namespace heritage::vehicles::tires::authoring_detail {

bool mapHeritageExtensions(
    const RawPropertyFile& raw,
    TirePropertyFileLoadResult& result)
{
    // TIRE07 Heritage-owned clean-room thermal network. This section is
    // deliberately namespaced and is NOT claimed to be a proprietary MF-Tyre
    // T&V parameter block. Official FITTYP=70/T&V data remain preserved for a
    // future licensed/equation-parity provider.
    int thermalEnabled = 0;
    if (integerFrom(raw, "HERITAGE_THERMAL", "ENABLED", thermalEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageThermalModel = thermalEnabled != 0;
        result.data.thermal.enabled = result.data.hasHeritageThermalModel;
    }
#define MAP_THERMAL(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_THERMAL", KEY, result.data.thermal.MEMBER)
    MAP_THERMAL("REFERENCE_TEMP_C", referenceTemperatureC);
    MAP_THERMAL("INITIAL_TREAD_C", initialTreadTemperatureC);
    MAP_THERMAL("INITIAL_BELT_C", initialBeltTemperatureC);
    MAP_THERMAL("INITIAL_CARCASS_C", initialCarcassTemperatureC);
    MAP_THERMAL("INITIAL_INNER_SIDEWALL_C", initialInnerSidewallTemperatureC);
    MAP_THERMAL("INITIAL_OUTER_SIDEWALL_C", initialOuterSidewallTemperatureC);
    MAP_THERMAL("INITIAL_GAS_C", initialGasTemperatureC);
    MAP_THERMAL("INITIAL_RIM_C", initialRimTemperatureC);
    MAP_THERMAL("AMBIENT_TEMP_C", ambientTemperatureC);
    MAP_THERMAL("ROAD_TEMP_C", roadTemperatureC);
    MAP_THERMAL("AMBIENT_PRESSURE_PA", ambientPressurePa);
    MAP_THERMAL("TREAD_CAPACITY_JPK", treadHeatCapacityJPerK);
    MAP_THERMAL("BELT_CAPACITY_JPK", beltHeatCapacityJPerK);
    MAP_THERMAL("CARCASS_CAPACITY_JPK", carcassHeatCapacityJPerK);
    MAP_THERMAL("INNER_SIDEWALL_CAPACITY_JPK", innerSidewallHeatCapacityJPerK);
    MAP_THERMAL("OUTER_SIDEWALL_CAPACITY_JPK", outerSidewallHeatCapacityJPerK);
    MAP_THERMAL("GAS_CAPACITY_JPK", gasHeatCapacityJPerK);
    MAP_THERMAL("RIM_CAPACITY_JPK", rimHeatCapacityJPerK);
    MAP_THERMAL("K_TREAD_BELT_WPK", treadToBeltConductanceWPerK);
    MAP_THERMAL("K_TREAD_CARCASS_WPK", treadToCarcassConductanceWPerK);
    MAP_THERMAL("K_BELT_CARCASS_WPK", beltToCarcassConductanceWPerK);
    MAP_THERMAL("K_CARCASS_INNER_SIDEWALL_WPK", carcassToInnerSidewallConductanceWPerK);
    MAP_THERMAL("K_CARCASS_OUTER_SIDEWALL_WPK", carcassToOuterSidewallConductanceWPerK);
    MAP_THERMAL("K_TREAD_ROAD_WPK", treadToRoadConductanceWPerK);
    MAP_THERMAL("K_TREAD_AIR_WPK", treadToAirConductanceWPerK);
    MAP_THERMAL("K_CARCASS_AIR_WPK", carcassToAirConductanceWPerK);
    MAP_THERMAL("K_INNER_SIDEWALL_AIR_WPK", innerSidewallToAirConductanceWPerK);
    MAP_THERMAL("K_OUTER_SIDEWALL_AIR_WPK", outerSidewallToAirConductanceWPerK);
    MAP_THERMAL("K_CARCASS_GAS_WPK", carcassToGasConductanceWPerK);
    MAP_THERMAL("K_INNER_SIDEWALL_GAS_WPK", innerSidewallToGasConductanceWPerK);
    MAP_THERMAL("K_OUTER_SIDEWALL_GAS_WPK", outerSidewallToGasConductanceWPerK);
    MAP_THERMAL("K_GAS_AMBIENT_WPK", gasToAmbientConductanceWPerK);
    MAP_THERMAL("K_CARCASS_RIM_WPK", carcassToRimConductanceWPerK);
    MAP_THERMAL("K_INNER_SIDEWALL_RIM_WPK", innerSidewallToRimConductanceWPerK);
    MAP_THERMAL("K_OUTER_SIDEWALL_RIM_WPK", outerSidewallToRimConductanceWPerK);
    MAP_THERMAL("K_RIM_AIR_WPK", rimToAirConductanceWPerK);
    MAP_THERMAL("K_TREAD_AIR_SPEED", treadAirSpeedConductanceWPerKPerMps);
    MAP_THERMAL("K_CARCASS_AIR_SPEED", carcassAirSpeedConductanceWPerKPerMps);
    MAP_THERMAL("K_SIDEWALL_AIR_SPEED", sidewallAirSpeedConductanceWPerKPerMps);
    MAP_THERMAL("K_RIM_AIR_SPEED", rimAirSpeedConductanceWPerKPerMps);
    MAP_THERMAL("SLIP_HEAT_TREAD_FRACTION", slipHeatFractionToTread);
    MAP_THERMAL("SLIP_HEAT_BELT_FRACTION", slipHeatFractionToBelt);
    MAP_THERMAL("SLIP_HEAT_EFFICIENCY", slipHeatEfficiency);
    MAP_THERMAL("CARCASS_LOSS_EFFICIENCY", carcassLossHeatEfficiency);
    MAP_THERMAL("CARCASS_LOSS_BELT_FRACTION", carcassLossHeatFractionToBelt);
    MAP_THERMAL("SIDEWALL_FLEX_HEAT_FRACTION", sidewallFlexHeatFraction);
    MAP_THERMAL("BRAKE_HEAT_TO_RIM_FRACTION", brakeHeatFractionToRim);
    MAP_THERMAL("OPTIMUM_TREAD_C", optimumTreadTemperatureC);
    MAP_THERMAL("COLD_SPAN_C", coldTemperatureSpanC);
    MAP_THERMAL("HOT_SPAN_C", hotTemperatureSpanC);
    MAP_THERMAL("MAX_COLD_GRIP_LOSS", maximumColdFrictionLoss);
    MAP_THERMAL("MAX_HOT_GRIP_LOSS", maximumHotFrictionLoss);
    MAP_THERMAL("MIN_FRICTION_SCALE", minimumFrictionScale);
    MAP_THERMAL("MAX_FRICTION_SCALE", maximumFrictionScale);
    MAP_THERMAL("STIFFNESS_TEMP_SLOPE", stiffnessTemperatureSlopePerC);
    MAP_THERMAL("MIN_STIFFNESS_SCALE", minimumStiffnessScale);
    MAP_THERMAL("MAX_STIFFNESS_SCALE", maximumStiffnessScale);
#undef MAP_THERMAL
    if (result.data.hasHeritageThermalModel)
    {
        result.data.thermal.referenceGaugePressurePa = result.data.inflationPressurePa;
        if (!validTireThermalDescription(result.data.thermal))
        {
            result.errorMessage = "Invalid [HERITAGE_THERMAL] clean-room thermal parameter set.";
            return false;
        }
    }

    // TIRE46 construction/damage model. Historical tire values may be
    // evidence-informed estimates with explicit provenance/confidence; the
    // section is Heritage-owned and does not imply proprietary manufacturer data.
    int damageEnabled = 0;
    if (integerFrom(raw, "HERITAGE_DAMAGE", "ENABLED", damageEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageDamageModel = damageEnabled != 0;
        result.data.failure.enabled = result.data.hasHeritageDamageModel;
    }
#define MAP_DAMAGE(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_DAMAGE", KEY, result.data.failure.MEMBER)
    MAP_DAMAGE("DISCHARGE_COEFFICIENT", dischargeCoefficient);
    MAP_DAMAGE("SLOW_PUNCTURE_AREA_M2", slowPunctureAreaM2);
    MAP_DAMAGE("RAPID_LOSS_AREA_M2", rapidPressureLossAreaM2);
    MAP_DAMAGE("BLOWOUT_AREA_M2", blowoutAreaM2);
    MAP_DAMAGE("TREAD_CUT_AREA_M2", treadCutAreaM2);
    MAP_DAMAGE("SIDEWALL_CUT_AREA_M2", sidewallCutAreaM2);
    MAP_DAMAGE("VALVE_LEAK_AREA_M2", valveLeakAreaM2);
    MAP_DAMAGE("BEAD_LEAK_AREA_M2", beadLeakAreaM2);
    MAP_DAMAGE("UNDERINFLATION_DAMAGE_START_RATIO", underinflationDamageStartRatio);
    MAP_DAMAGE("COLLAPSED_PRESSURE_RATIO", collapsedPressureRatio);
    MAP_DAMAGE("COLLAPSE_LOADED_DELAY_S", collapseLoadedDelaySeconds);
    MAP_DAMAGE("MAX_SAFE_CARCASS_TEMP_C", maximumSafeCarcassTemperatureC);
    MAP_DAMAGE("BELT_FATIGUE_REFERENCE_J", beltFatigueReferenceEnergyJ);
    MAP_DAMAGE("CORD_FATIGUE_REFERENCE_J", cordFatigueReferenceEnergyJ);
    MAP_DAMAGE("SIDEWALL_FATIGUE_REFERENCE_J", sidewallFatigueReferenceEnergyJ);
    MAP_DAMAGE("FATIGUE_HEAT_ACCEL_PER_C", fatigueHeatAccelerationPerC);
    MAP_DAMAGE("FATIGUE_OVERLOAD_EXPONENT", fatigueOverloadExponent);
    MAP_DAMAGE("GRAINING_COLD_BELOW_OPTIMUM_C", grainingColdThresholdBelowOptimumC);
    MAP_DAMAGE("GRAINING_BUILD_PER_KJ", grainingBuildPerKJ);
    MAP_DAMAGE("GRAINING_RECOVERY_PER_S", grainingRecoveryPerSecond);
    MAP_DAMAGE("BLISTER_TEMP_C", blisterTemperatureC);
    MAP_DAMAGE("BLISTER_BUILD_PER_KJ", blisterBuildPerKJ);
    MAP_DAMAGE("BLISTER_RECOVERY_PER_S", blisterRecoveryPerSecond);
    MAP_DAMAGE("DELAMINATION_TEMP_C", delaminationTemperatureC);
    MAP_DAMAGE("DELAMINATION_BUILD_PER_S", delaminationBuildPerSecond);
    MAP_DAMAGE("BEAD_UNSEAT_PRESSURE_RATIO", beadUnseatPressureRatio);
    MAP_DAMAGE("BEAD_UNSEAT_LATERAL_FORCE_RATIO", beadUnseatLateralForceRatio);
    MAP_DAMAGE("BEAD_DAMAGE_PER_S", beadDamageRatePerSecond);
    MAP_DAMAGE("RIM_DAMAGE_POWER_THRESHOLD_W", rimDamagePowerThresholdW);
    MAP_DAMAGE("RIM_DAMAGE_PER_S", rimDamageRatePerSecond);
    MAP_DAMAGE("RUNFLAT_LOAD_FRACTION", runFlatSupportLoadFraction);
    MAP_DAMAGE("RUNFLAT_MAX_SPEED_MPS", runFlatMaximumSpeedMps);
    MAP_DAMAGE("RUNFLAT_MAX_TEMP_C", runFlatMaximumTemperatureC);
    MAP_DAMAGE("RUNFLAT_HEALTH_LOSS_PER_S", runFlatHealthLossPerSecond);
#undef MAP_DAMAGE
    int runFlatEnabled = 0;
    if (integerFrom(raw, "HERITAGE_DAMAGE", "RUNFLAT_ENABLED", runFlatEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.failure.runFlatSupportEnabled = runFlatEnabled != 0;
    }
    if (result.data.hasHeritageDamageModel
        && !validTireFailureDescription(result.data.failure))
    {
        result.errorMessage = "Invalid [HERITAGE_DAMAGE] construction/failure parameter set.";
        return false;
    }

    // TIRE08 Heritage-owned spatial tread temperature/wear state. This section
    // is intentionally namespaced; it is not a claim about proprietary LFS or
    // Simcenter implementation details.
    int treadStateEnabled = 0;
    if (integerFrom(raw, "HERITAGE_TREAD_STATE", "ENABLED", treadStateEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageTreadState = treadStateEnabled != 0;
        result.data.wear.enabled = result.data.hasHeritageTreadState;
    }
#define MAP_TREAD(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_TREAD_STATE", KEY, result.data.wear.MEMBER)
    MAP_TREAD("INITIAL_TREAD_DEPTH_M", initialTreadDepthM);
    MAP_TREAD("MINIMUM_TREAD_DEPTH_M", minimumTreadDepthM);
    MAP_TREAD("WEAR_DEPTH_PER_J", wearDepthPerJoule);
    MAP_TREAD("WEAR_LOAD_EXPONENT", wearLoadExponent);
    MAP_TREAD("WEAR_TEMP_SENSITIVITY_PER_C", wearTemperatureSensitivityPerC);
    MAP_TREAD("MIN_WEAR_TEMP_SCALE", minimumWearTemperatureScale);
    MAP_TREAD("MAX_WEAR_TEMP_SCALE", maximumWearTemperatureScale);
    MAP_TREAD("RUBBER_SHEDDING_PROPENSITY", rubberSheddingPropensity);
    MAP_TREAD("SURFACE_CELL_CAPACITY_JPK", surfaceHeatCapacityJPerKPerCell);
    MAP_TREAD("SURFACE_SLIP_HEAT_FRACTION", surfaceSlipHeatFraction);
    MAP_TREAD("SURFACE_TO_BULK_HZ", surfaceToBulkRelaxationHz);
    MAP_TREAD("CIRCUMFERENTIAL_DIFFUSION_HZ", circumferentialDiffusionHz);
    MAP_TREAD("LATERAL_DIFFUSION_HZ", lateralDiffusionHz);
    MAP_TREAD("MAX_SURFACE_OFFSET_C", maximumSurfaceOffsetC);
    MAP_TREAD("BASE_CENTER_LOAD_FRACTION", baseCenterLoadFraction);
    MAP_TREAD("PRESSURE_CENTER_BIAS_GAIN", pressureCenterBiasGain);
    MAP_TREAD("CAMBER_SHOULDER_BIAS_PER_RAD", camberShoulderBiasPerRad);
    MAP_TREAD("MAX_SHOULDER_BIAS", maximumShoulderBias);
    MAP_TREAD("MAX_WEAR_FRICTION_LOSS", maximumWearFrictionLoss);
    MAP_TREAD("WEAR_FRICTION_EXPONENT", wearFrictionExponent);
    MAP_TREAD("FLATSPOT_FRICTION_LOSS_PER_MM", flatSpotFrictionLossPerMm);
    MAP_TREAD("MAX_FLATSPOT_FRICTION_LOSS", maximumFlatSpotFrictionLoss);
#undef MAP_TREAD
    if (result.data.hasHeritageTreadState
        && !validTireWearDescription(result.data.wear))
    {
        result.errorMessage = "Invalid [HERITAGE_TREAD_STATE] spatial tread/wear parameter set.";
        return false;
    }

    // TIRE11 Heritage-owned contamination/pickup model. This operates on the
    // same 16x3 tread cells as TIRE08 but remains a separate surface-interaction
    // mechanism rather than being folded into MF6.2 coefficients.
    int contaminationEnabled = 0;
    if (integerFrom(raw, "HERITAGE_CONTAMINATION", "ENABLED", contaminationEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageContaminationModel = contaminationEnabled != 0;
        result.data.contamination.enabled = result.data.hasHeritageContaminationModel;
    }
#define MAP_CONTAM(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_CONTAMINATION", KEY, result.data.contamination.MEMBER)
    MAP_CONTAM("GRASS_ORGANIC_PICKUP_HZ", grassOrganicPickupRateHz);
    MAP_CONTAM("DIRT_MINERAL_PICKUP_HZ", dirtMineralPickupRateHz);
    MAP_CONTAM("GRAVEL_FINES_PICKUP_HZ", gravelFinesPickupRateHz);
    MAP_CONTAM("RUBBER_PICKUP_HZ", rubberPickupRateHz);
    MAP_CONTAM("MUD_FILM_PICKUP_HZ", mudFilmPickupRateHz);
    MAP_CONTAM("BASE_HARD_CLEAN_HZ", baseHardSurfaceCleaningRateHz);
    MAP_CONTAM("SPEED_CLEAN_PER_M", speedCleaningRatePerM);
    MAP_CONTAM("SLIP_CLEAN_PER_M", slipCleaningRatePerM);
    MAP_CONTAM("HOT_CLEAN_PER_C", hotTreadCleaningRatePerC);
    MAP_CONTAM("HOT_CLEAN_THRESHOLD_C", hotTreadCleaningThresholdC);
    MAP_CONTAM("ORGANIC_RETENTION", organicRetention);
    MAP_CONTAM("MINERAL_RETENTION", mineralRetention);
    MAP_CONTAM("GRAVEL_RETENTION", gravelFinesRetention);
    MAP_CONTAM("RUBBER_RETENTION", rubberRetention);
    MAP_CONTAM("MUD_RETENTION", mudRetention);
    MAP_CONTAM("ORGANIC_MAX_FRICTION_LOSS", organicMaximumFrictionLoss);
    MAP_CONTAM("MINERAL_MAX_FRICTION_LOSS", mineralMaximumFrictionLoss);
    MAP_CONTAM("GRAVEL_MAX_FRICTION_LOSS", gravelFinesMaximumFrictionLoss);
    MAP_CONTAM("RUBBER_MAX_FRICTION_LOSS", rubberPickupMaximumFrictionLoss);
    MAP_CONTAM("MUD_MAX_FRICTION_LOSS", mudMaximumFrictionLoss);
    MAP_CONTAM("MAX_COMBINED_FRICTION_LOSS", maximumCombinedFrictionLoss);
    MAP_CONTAM("ORGANIC_HEAT_INSULATION", organicRoadHeatInsulation);
    MAP_CONTAM("MINERAL_HEAT_INSULATION", mineralRoadHeatInsulation);
    MAP_CONTAM("GRAVEL_HEAT_INSULATION", gravelRoadHeatInsulation);
    MAP_CONTAM("RUBBER_HEAT_INSULATION", rubberRoadHeatInsulation);
    MAP_CONTAM("MUD_HEAT_INSULATION", mudRoadHeatInsulation);
    MAP_CONTAM("MIN_ROAD_HEAT_TRANSFER", minimumRoadHeatTransferScale);
    MAP_CONTAM("ORGANIC_RR_GAIN", organicRollingResistanceGain);
    MAP_CONTAM("MINERAL_RR_GAIN", mineralRollingResistanceGain);
    MAP_CONTAM("GRAVEL_RR_GAIN", gravelRollingResistanceGain);
    MAP_CONTAM("RUBBER_RR_GAIN", rubberRollingResistanceGain);
    MAP_CONTAM("MUD_RR_GAIN", mudRollingResistanceGain);
    MAP_CONTAM("MAX_RR_SCALE", maximumRollingResistanceScale);
#undef MAP_CONTAM
    if (result.data.hasHeritageContaminationModel
        && (!result.data.hasHeritageTreadState
            || !validTireContaminationDescription(result.data.contamination)))
    {
        result.errorMessage = "Invalid [HERITAGE_CONTAMINATION] parameter set or missing [HERITAGE_TREAD_STATE].";
        return false;
    }

    // TIRE12 Heritage-owned wet hard-surface / hydroplaning layer. It consumes
    // normalized scene wetness as water depth through an explicit authoring
    // scale and operates around the MF6.2 force core. These are NOT proprietary
    // Simcenter 2512 coefficients.
    int wetSurfaceEnabled = 0;
    if (integerFrom(raw, "HERITAGE_WET_SURFACE", "ENABLED", wetSurfaceEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageWetSurfaceModel = wetSurfaceEnabled != 0;
        result.data.wetSurface.enabled = result.data.hasHeritageWetSurfaceModel;
    }
#define MAP_WET(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_WET_SURFACE", KEY, result.data.wetSurface.MEMBER)
    MAP_WET("WETNESS_ONE_WATER_DEPTH_M", wetnessOneWaterDepthM);
    MAP_WET("MIN_ACTIVE_WATER_DEPTH_M", minimumActiveWaterDepthM);
    MAP_WET("FULLY_WETTED_WATER_DEPTH_M", fullyWettedWaterDepthM);
    MAP_WET("TREAD_VOID_RATIO", treadVoidRatio);
    MAP_WET("DRAINAGE_EFFICIENCY", drainageEfficiency);
    MAP_WET("DRAINAGE_REFERENCE_SPEED_MPS", drainageReferenceSpeedMps);
    MAP_WET("MIN_DRAINAGE_TREAD_DEPTH_M", minimumDrainageTreadDepthM);
    MAP_WET("WATER_DENSITY_KGM3", waterDensityKgPerM3);
    MAP_WET("HYDRO_LIFT_COEFFICIENT", hydrodynamicLiftCoefficient);
    MAP_WET("HYDRO_DRAG_COEFFICIENT", hydrodynamicDragCoefficient);
    MAP_WET("DRAINAGE_ONSET_RATIO", drainageOnsetRatio);
    MAP_WET("DRAINAGE_FULL_RATIO", drainageFullRatio);
    MAP_WET("MAX_HYDROPLANING_FRACTION", maximumHydroplaningFraction);
    MAP_WET("THIN_FILM_MAX_FRICTION_LOSS", thinFilmMaximumFrictionLoss);
    MAP_WET("THIN_FILM_SPEED_REFERENCE_MPS", thinFilmSpeedReferenceMps);
    MAP_WET("HYDRO_FRICTION_FLOOR", hydroplaningFrictionFloor);
    MAP_WET("HYDRO_STIFFNESS_FLOOR", hydroplaningStiffnessFloor);
    MAP_WET("HYDRO_RELAXATION_GAIN", hydroplaningRelaxationGain);
    MAP_WET("MAX_RELAXATION_SCALE", maximumRelaxationScale);
    MAP_WET("WET_RR_GAIN", wetRollingResistanceGain);
    MAP_WET("MAX_RR_SCALE", maximumRollingResistanceScale);
    MAP_WET("WET_ROAD_HEAT_TRANSFER_GAIN", wetRoadHeatTransferGain);
    MAP_WET("MAX_ROAD_HEAT_TRANSFER_SCALE", maximumRoadHeatTransferScale);
    MAP_WET("RETAINED_WATER_MAX_DEPTH_M", retainedWaterMaximumDepthM);
    MAP_WET("RETAINED_WATER_PICKUP_HZ", retainedWaterPickupRateHz);
    MAP_WET("RETAINED_WATER_RELEASE_HZ", retainedWaterReleaseRateHz);
    MAP_WET("RETAINED_WATER_SPEED_RELEASE_PER_M", retainedWaterSpeedReleasePerM);
#undef MAP_WET
    if (result.data.hasHeritageWetSurfaceModel
        && (!result.data.hasHeritageTreadState
            || !validTireWetSurfaceDescription(result.data.wetSurface)))
    {
        result.errorMessage = "Invalid [HERITAGE_WET_SURFACE] parameter set or missing [HERITAGE_TREAD_STATE].";
        return false;
    }

    int winterSurfaceEnabled = 0;
    if (integerFrom(raw, "HERITAGE_WINTER_SURFACE", "ENABLED", winterSurfaceEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageWinterSurfaceModel = winterSurfaceEnabled != 0;
        result.data.winterSurface.enabled = result.data.hasHeritageWinterSurfaceModel;
    }
#define MAP_WINTER(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_WINTER_SURFACE", KEY, result.data.winterSurface.MEMBER)
    MAP_WINTER("WINTER_COMPOUND_EFFECTIVENESS", winterCompoundEffectiveness);
    MAP_WINTER("SIPING_DENSITY", sipingDensity);
    MAP_WINTER("SNOW_TREAD_INTERLOCK", snowTreadInterlock);
    MAP_WINTER("SNOW_SELF_CLEANING", snowSelfCleaning);
    int studsEnabled = 0;
    if (integerFrom(raw, "HERITAGE_WINTER_SURFACE", "STUDS_ENABLED", studsEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.winterSurface.studsEnabled = studsEnabled != 0;
    }
    int studCount = 0;
    if (integerFrom(raw, "HERITAGE_WINTER_SURFACE", "STUD_COUNT", studCount))
    {
        ++result.data.mappedAssignmentCount;
        result.data.winterSurface.studCount = studCount;
    }
    MAP_WINTER("STUD_PROTRUSION_M", studProtrusionM);
    MAP_WINTER("STUD_REFERENCE_COUNT", studReferenceCount);
    MAP_WINTER("STUD_REFERENCE_PROTRUSION_M", studReferenceProtrusionM);
    MAP_WINTER("STUD_ICE_FRICTION_GAIN", studIceFrictionGain);
    MAP_WINTER("MAX_STUD_ICE_FRICTION_GAIN", maximumStudIceFrictionGain);
    MAP_WINTER("ICE_COLD_REFERENCE_TEMP_C", iceColdReferenceTemperatureC);
    MAP_WINTER("ICE_NEAR_MELT_TEMP_C", iceNearMeltTemperatureC);
    MAP_WINTER("ICE_COLD_BASE_FRICTION_SCALE", iceColdBaseFrictionScale);
    MAP_WINTER("ICE_NEAR_MELT_BASE_FRICTION_SCALE", iceNearMeltBaseFrictionScale);
    MAP_WINTER("ICE_WINTER_COMPOUND_GAIN", iceWinterCompoundGain);
    MAP_WINTER("ICE_SIPING_GAIN", iceSipingGain);
    MAP_WINTER("ICE_SLIP_SPEED_LOSS", iceSlipSpeedLoss);
    MAP_WINTER("ICE_SLIP_SPEED_REFERENCE_MPS", iceSlipSpeedReferenceMps);
    MAP_WINTER("ICE_MELT_FILM_MAX_DEPTH_M", iceMeltFilmMaximumDepthM);
    MAP_WINTER("ICE_MELT_FILM_FRICTION_LOSS", iceMeltFilmFrictionLoss);
    MAP_WINTER("ICE_FLASH_HEAT_FILM_GAIN", iceFlashHeatFilmGain);
    MAP_WINTER("SNOW_BASE_FRICTION_SCALE", snowBaseFrictionScale);
    MAP_WINTER("SNOW_WINTER_COMPOUND_GAIN", snowWinterCompoundGain);
    MAP_WINTER("SNOW_SIPING_GAIN", snowSipingGain);
    MAP_WINTER("SNOW_INTERLOCK_GAIN", snowInterlockGain);
    MAP_WINTER("SNOW_PACKED_TREAD_GAIN", snowPackedTreadGain);
    MAP_WINTER("SNOW_SLIP_BUILD_GAIN", snowSlipBuildGain);
    MAP_WINTER("SNOW_HIGH_SLIP_LOSS", snowHighSlipLoss);
    MAP_WINTER("SNOW_SLIP_BUILD_REFERENCE_MPS", snowSlipBuildReferenceMps);
    MAP_WINTER("SNOW_HIGH_SLIP_REFERENCE_MPS", snowHighSlipReferenceMps);
    MAP_WINTER("ICE_STIFFNESS_SCALE", iceStiffnessScale);
    MAP_WINTER("SNOW_STIFFNESS_SCALE", snowStiffnessScale);
    MAP_WINTER("ICE_RR_SCALE", iceRollingResistanceScale);
    MAP_WINTER("SNOW_RR_SCALE", snowRollingResistanceScale);
    MAP_WINTER("ICE_RELAXATION_SCALE", iceRelaxationScale);
    MAP_WINTER("SNOW_RELAXATION_SCALE", snowRelaxationScale);
    MAP_WINTER("MIN_FRICTION_SCALE", minimumFrictionScale);
    MAP_WINTER("MAX_FRICTION_SCALE", maximumFrictionScale);
    MAP_WINTER("PACKED_SNOW_PICKUP_HZ", packedSnowPickupRateHz);
    MAP_WINTER("PACKED_SNOW_BASE_RELEASE_HZ", packedSnowBaseReleaseRateHz);
    MAP_WINTER("PACKED_SNOW_SPEED_RELEASE_PER_M", packedSnowSpeedReleasePerM);
    MAP_WINTER("PACKED_SNOW_SLIP_RELEASE_PER_M", packedSnowSlipReleasePerM);
#undef MAP_WINTER
    if (result.data.hasHeritageWinterSurfaceModel
        && (!result.data.hasHeritageTreadState
            || !validTireWinterSurfaceDescription(result.data.winterSurface)))
    {
        result.errorMessage = "Invalid [HERITAGE_WINTER_SURFACE] parameter set or missing [HERITAGE_TREAD_STATE].";
        return false;
    }

    int shallowGranularEnabled = 0;
    if (integerFrom(raw, "HERITAGE_SHALLOW_GRANULAR", "ENABLED", shallowGranularEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageShallowGranularModel = shallowGranularEnabled != 0;
        result.data.shallowGranularSurface.enabled =
            result.data.hasHeritageShallowGranularModel;
    }
#define MAP_SHALLOW_GRANULAR(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_SHALLOW_GRANULAR", KEY, result.data.shallowGranularSurface.MEMBER)
    MAP_SHALLOW_GRANULAR("TREAD_AGGRESSIVENESS", treadAggressiveness);
    MAP_SHALLOW_GRANULAR("TREAD_EDGE_DENSITY", treadEdgeDensity);
    MAP_SHALLOW_GRANULAR("OPEN_VOID_RATIO", openVoidRatio);
    MAP_SHALLOW_GRANULAR("GRANULAR_SHEAR_COUPLING", granularShearCoupling);
    MAP_SHALLOW_GRANULAR("BULLDOZING_COUPLING", bulldozingCoupling);
    MAP_SHALLOW_GRANULAR("PLOWING_COUPLING", plowingCoupling);
    MAP_SHALLOW_GRANULAR("MIN_WORN_TREAD_EFFECTIVENESS", minimumWornTreadEffectiveness);
    MAP_SHALLOW_GRANULAR("TREAD_DEPTH_EFFECT_EXPONENT", treadDepthEffectExponent);
    MAP_SHALLOW_GRANULAR("MAX_SINKAGE_M", maximumSinkageM);
    MAP_SHALLOW_GRANULAR("MAX_GRANULAR_FORCE_RATIO", maximumGranularForceRatio);
    MAP_SHALLOW_GRANULAR("MAX_PLOWING_FORCE_RATIO", maximumPlowingForceRatio);
    MAP_SHALLOW_GRANULAR("MIN_BASE_FRICTION_SCALE", minimumBaseFrictionScale);
    MAP_SHALLOW_GRANULAR("MAX_BASE_FRICTION_SCALE", maximumBaseFrictionScale);
#undef MAP_SHALLOW_GRANULAR
    if (result.data.hasHeritageShallowGranularModel
        && !validTireShallowGranularDescription(
            result.data.shallowGranularSurface))
    {
        result.errorMessage =
            "Invalid [HERITAGE_SHALLOW_GRANULAR] tire-trait parameter set.";
        return false;
    }

    int deformableTerrainEnabled = 0;
    if (integerFrom(raw, "HERITAGE_DEFORMABLE_TERRAIN", "ENABLED", deformableTerrainEnabled))
    {
        ++result.data.mappedAssignmentCount;
        result.data.hasHeritageDeformableTerrainModel = deformableTerrainEnabled != 0;
        result.data.deformableTerrainSurface.enabled =
            result.data.hasHeritageDeformableTerrainModel;
    }
#define MAP_DEFORMABLE_TERRAIN(KEY, MEMBER) mapScalar(raw, result.data, "HERITAGE_DEFORMABLE_TERRAIN", KEY, result.data.deformableTerrainSurface.MEMBER)
    MAP_DEFORMABLE_TERRAIN("TREAD_AGGRESSIVENESS", treadAggressiveness);
    MAP_DEFORMABLE_TERRAIN("TREAD_EDGE_DENSITY", treadEdgeDensity);
    MAP_DEFORMABLE_TERRAIN("OPEN_VOID_RATIO", openVoidRatio);
    MAP_DEFORMABLE_TERRAIN("SOIL_SHEAR_COUPLING", soilShearCoupling);
    MAP_DEFORMABLE_TERRAIN("BULLDOZING_COUPLING", bulldozingCoupling);
    MAP_DEFORMABLE_TERRAIN("PLOWING_COUPLING", plowingCoupling);
    MAP_DEFORMABLE_TERRAIN("FLOTATION_COUPLING", flotationCoupling);
    MAP_DEFORMABLE_TERRAIN("MIN_WORN_TREAD_EFFECTIVENESS", minimumWornTreadEffectiveness);
    MAP_DEFORMABLE_TERRAIN("TREAD_DEPTH_EFFECT_EXPONENT", treadDepthEffectExponent);
    MAP_DEFORMABLE_TERRAIN("MAX_SINKAGE_M", maximumSinkageM);
    MAP_DEFORMABLE_TERRAIN("MAX_TERRAIN_FORCE_RATIO", maximumTerrainForceRatio);
    MAP_DEFORMABLE_TERRAIN("MAX_PLOWING_FORCE_RATIO", maximumPlowingForceRatio);
    MAP_DEFORMABLE_TERRAIN("MIN_MF_FRICTION_SCALE", minimumMfFrictionScale);
    MAP_DEFORMABLE_TERRAIN("MAX_MF_FRICTION_SCALE", maximumMfFrictionScale);
#undef MAP_DEFORMABLE_TERRAIN
    if (result.data.hasHeritageDeformableTerrainModel
        && !validTireDeformableTerrainDescription(
            result.data.deformableTerrainSurface))
    {
        result.errorMessage =
            "Invalid [HERITAGE_DEFORMABLE_TERRAIN] tire-trait parameter set.";
        return false;
    }

    return true;
}

} // namespace heritage::vehicles::tires::authoring_detail
