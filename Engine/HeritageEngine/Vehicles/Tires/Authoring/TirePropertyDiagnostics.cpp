#include "TirePropertyImportInternal.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace heritage::vehicles::tires::authoring_detail {

void enumerateUnsupported(const RawPropertyFile& raw, TirePropertyFileData& data)
{
    // This list deliberately covers every assignment TIRE02 consumes. Anything
    // outside it remains visible in diagnostics for future Swift/turn-slip/T&V
    // implementation rather than being silently discarded.
    const std::unordered_map<std::string, std::vector<std::string>> supported = {
        { "UNITS", { "LENGTH", "FORCE", "ANGLE", "MASS", "TIME", "TEMPERATURE" } },
        { "MODEL", { "FITTYP", "TYRESIDE", "LONGVL", "TV_MODEL", "DAMP_LSG", "VX_STBL" } },
        { "DIMENSION", { "UNLOADED_RADIUS", "WIDTH", "RIM_RADIUS", "RIM_WIDTH", "ASPECT_RATIO" } },
        { "OPERATING_CONDITIONS", { "INFLPRES", "NOMPRES" } },
        { "INERTIA", { "MASS", "IXX", "IYY", "BELT_MASS", "BELT_IXX", "BELT_IYY" } },
        { "VERTICAL", { "FNOMIN", "VERTICAL_STIFFNESS", "VERTICAL_DAMPING", "MC_CONTOUR_A", "MC_CONTOUR_B", "BREFF", "DREFF", "FREFF", "Q_RE0", "Q_V1", "Q_V2" } },
        { "STRUCTURAL", { "LONGITUDINAL_STIFFNESS", "LATERAL_STIFFNESS", "YAW_STIFFNESS", "FREQ_LONG", "FREQ_LAT", "FREQ_YAW", "FREQ_WINDUP", "DAMP_LONG", "DAMP_LAT", "DAMP_YAW", "DAMP_WINDUP", "DAMP_RESIDUAL", "DAMP_VLOW", "Q_BVX", "Q_BVT", "PCFX1", "PCFX2", "PCFX3", "PCFY1", "PCFY2", "PCFY3", "PCMZ1" } },
        { "CONTACT_PATCH", { "Q_RA1", "Q_RA2", "Q_RB1", "Q_RB2", "Q_A1", "Q_A2", "ELLIPS_SHIFT", "ELLIPS_LENGTH", "ELLIPS_HEIGHT", "ELLIPS_ORDER", "ELLIPS_MAX_STEP", "ELLIPS_NWIDTH", "ELLIPS_NLENGTH", "ENV_C1", "ENV_C2", "Q_CFG1" } },
        { "INFLATION_PRESSURE_RANGE", { "PRESMIN", "PRESMAX" } },
        { "VERTICAL_FORCE_RANGE", { "FZMIN", "FZMAX" } },
        { "LONG_SLIP_RANGE", { "KPUMIN", "KPUMAX" } },
        { "SLIP_ANGLE_RANGE", { "ALPMIN", "ALPMAX" } },
        { "INCLINATION_ANGLE_RANGE", { "CAMMIN", "CAMMAX" } },
        { "SCALING_COEFFICIENTS", { "LFZO", "LCX", "LMUX", "LEX", "LKX", "LHX", "LVX", "LXAL", "LCY", "LMUY", "LEY", "LKY", "LKYC", "LHY", "LVY", "LYKA", "LVYKA", "LMX", "LVMX", "LMY", "LMP", "LTR", "LRES", "LKZC", "LS", "LSGKP", "LSGAL" } },
        { "LONGITUDINAL_COEFFICIENT", { "PCX1", "PDX1", "PDX2", "PDX3", "PEX1", "PEX2", "PEX3", "PEX4", "PKX1", "PKX2", "PKX3", "PHX1", "PHX2", "PVX1", "PVX2", "PPX1", "PPX2", "PPX3", "PPX4", "RBX1", "RBX2", "RBX3", "RCX1", "REX1", "REX2", "RHX1", "PTX1", "PTX2", "PTX3" } },
        { "OVERTURNING_COEFFICIENTS", { "QSX1", "QSX2", "QSX3", "QSX4", "QSX5", "QSX6", "QSX7", "QSX8", "QSX9", "QSX10", "QSX11", "QSX12", "QSX13", "QSX14", "PPMX1" } },
        { "LATERAL_COEFFICIENT", { "PCY1", "PDY1", "PDY2", "PDY3", "PEY1", "PEY2", "PEY3", "PEY4", "PEY5", "PKY1", "PKY2", "PKY3", "PKY4", "PKY5", "PKY6", "PKY7", "PHY1", "PHY2", "PVY1", "PVY2", "PVY3", "PVY4", "PPY1", "PPY2", "PPY3", "PPY4", "PPY5", "PTY1", "PTY2", "RBY1", "RBY2", "RBY3", "RBY4", "RCY1", "REY1", "REY2", "RHY1", "RHY2", "RVY1", "RVY2", "RVY3", "RVY4", "RVY5", "RVY6" } },
        { "ROLLING_COEFFICIENTS", { "QSY1", "QSY2", "QSY3", "QSY4", "QSY5", "QSY6", "QSY7", "QSY8" } },
        { "ALIGNING_COEFFICIENTS", { "QBZ1", "QBZ2", "QBZ3", "QBZ4", "QBZ5", "QBZ9", "QBZ10", "QCZ1", "QDZ1", "QDZ2", "QDZ3", "QDZ4", "QDZ6", "QDZ7", "QDZ8", "QDZ9", "QDZ10", "QDZ11", "QEZ1", "QEZ2", "QEZ3", "QEZ4", "QEZ5", "QHZ1", "QHZ2", "QHZ3", "QHZ4", "SSZ1", "SSZ2", "SSZ3", "SSZ4", "PPZ1", "PPZ2" } },
        { "TURNSLIP_COEFFICIENTS", { "PDXP1", "PDXP2", "PDXP3", "PKYP1", "PDYP1", "PDYP2", "PDYP3", "PDYP4", "PHYP1", "PHYP2", "PHYP3", "PHYP4", "PECP1", "PECP2", "QDTP1", "QCRP1", "QCRP2", "QBRP1", "QDRP1" } },
        { "HERITAGE_THERMAL", { "ENABLED", "REFERENCE_TEMP_C", "INITIAL_TREAD_C", "INITIAL_CARCASS_C", "INITIAL_GAS_C", "AMBIENT_TEMP_C", "ROAD_TEMP_C", "AMBIENT_PRESSURE_PA", "TREAD_CAPACITY_JPK", "CARCASS_CAPACITY_JPK", "GAS_CAPACITY_JPK", "K_TREAD_CARCASS_WPK", "K_TREAD_ROAD_WPK", "K_TREAD_AIR_WPK", "K_CARCASS_AIR_WPK", "K_CARCASS_GAS_WPK", "K_GAS_AMBIENT_WPK", "K_TREAD_AIR_SPEED", "K_CARCASS_AIR_SPEED", "SLIP_HEAT_TREAD_FRACTION", "SLIP_HEAT_EFFICIENCY", "CARCASS_LOSS_EFFICIENCY", "OPTIMUM_TREAD_C", "COLD_SPAN_C", "HOT_SPAN_C", "MAX_COLD_GRIP_LOSS", "MAX_HOT_GRIP_LOSS", "MIN_FRICTION_SCALE", "MAX_FRICTION_SCALE", "STIFFNESS_TEMP_SLOPE", "MIN_STIFFNESS_SCALE", "MAX_STIFFNESS_SCALE" } },
        { "HERITAGE_TREAD_STATE", { "ENABLED", "INITIAL_TREAD_DEPTH_M", "MINIMUM_TREAD_DEPTH_M", "WEAR_DEPTH_PER_J", "WEAR_LOAD_EXPONENT", "WEAR_TEMP_SENSITIVITY_PER_C", "MIN_WEAR_TEMP_SCALE", "MAX_WEAR_TEMP_SCALE", "SURFACE_CELL_CAPACITY_JPK", "SURFACE_SLIP_HEAT_FRACTION", "SURFACE_TO_BULK_HZ", "CIRCUMFERENTIAL_DIFFUSION_HZ", "LATERAL_DIFFUSION_HZ", "MAX_SURFACE_OFFSET_C", "BASE_CENTER_LOAD_FRACTION", "PRESSURE_CENTER_BIAS_GAIN", "CAMBER_SHOULDER_BIAS_PER_RAD", "MAX_SHOULDER_BIAS", "MAX_WEAR_FRICTION_LOSS", "WEAR_FRICTION_EXPONENT", "FLATSPOT_FRICTION_LOSS_PER_MM", "MAX_FLATSPOT_FRICTION_LOSS" } },
        { "HERITAGE_CONTAMINATION", { "ENABLED", "GRASS_ORGANIC_PICKUP_HZ", "DIRT_MINERAL_PICKUP_HZ", "GRAVEL_FINES_PICKUP_HZ", "RUBBER_PICKUP_HZ", "MUD_FILM_PICKUP_HZ", "BASE_HARD_CLEAN_HZ", "SPEED_CLEAN_PER_M", "SLIP_CLEAN_PER_M", "HOT_CLEAN_PER_C", "HOT_CLEAN_THRESHOLD_C", "ORGANIC_RETENTION", "MINERAL_RETENTION", "GRAVEL_RETENTION", "RUBBER_RETENTION", "MUD_RETENTION", "ORGANIC_MAX_FRICTION_LOSS", "MINERAL_MAX_FRICTION_LOSS", "GRAVEL_MAX_FRICTION_LOSS", "RUBBER_MAX_FRICTION_LOSS", "MUD_MAX_FRICTION_LOSS", "MAX_COMBINED_FRICTION_LOSS", "ORGANIC_HEAT_INSULATION", "MINERAL_HEAT_INSULATION", "GRAVEL_HEAT_INSULATION", "RUBBER_HEAT_INSULATION", "MUD_HEAT_INSULATION", "MIN_ROAD_HEAT_TRANSFER", "ORGANIC_RR_GAIN", "MINERAL_RR_GAIN", "GRAVEL_RR_GAIN", "RUBBER_RR_GAIN", "MUD_RR_GAIN", "MAX_RR_SCALE" } },
        { "HERITAGE_WET_SURFACE", { "ENABLED", "WETNESS_ONE_WATER_DEPTH_M", "MIN_ACTIVE_WATER_DEPTH_M", "FULLY_WETTED_WATER_DEPTH_M", "TREAD_VOID_RATIO", "DRAINAGE_EFFICIENCY", "DRAINAGE_REFERENCE_SPEED_MPS", "MIN_DRAINAGE_TREAD_DEPTH_M", "WATER_DENSITY_KGM3", "HYDRO_LIFT_COEFFICIENT", "HYDRO_DRAG_COEFFICIENT", "DRAINAGE_ONSET_RATIO", "DRAINAGE_FULL_RATIO", "MAX_HYDROPLANING_FRACTION", "THIN_FILM_MAX_FRICTION_LOSS", "THIN_FILM_SPEED_REFERENCE_MPS", "HYDRO_FRICTION_FLOOR", "HYDRO_STIFFNESS_FLOOR", "HYDRO_RELAXATION_GAIN", "MAX_RELAXATION_SCALE", "WET_RR_GAIN", "MAX_RR_SCALE", "WET_ROAD_HEAT_TRANSFER_GAIN", "MAX_ROAD_HEAT_TRANSFER_SCALE", "RETAINED_WATER_MAX_DEPTH_M", "RETAINED_WATER_PICKUP_HZ", "RETAINED_WATER_RELEASE_HZ", "RETAINED_WATER_SPEED_RELEASE_PER_M" } },
        { "HERITAGE_WINTER_SURFACE", { "ENABLED", "WINTER_COMPOUND_EFFECTIVENESS", "SIPING_DENSITY", "SNOW_TREAD_INTERLOCK", "SNOW_SELF_CLEANING", "STUDS_ENABLED", "STUD_COUNT", "STUD_PROTRUSION_M", "STUD_REFERENCE_COUNT", "STUD_REFERENCE_PROTRUSION_M", "STUD_ICE_FRICTION_GAIN", "MAX_STUD_ICE_FRICTION_GAIN", "ICE_COLD_REFERENCE_TEMP_C", "ICE_NEAR_MELT_TEMP_C", "ICE_COLD_BASE_FRICTION_SCALE", "ICE_NEAR_MELT_BASE_FRICTION_SCALE", "ICE_WINTER_COMPOUND_GAIN", "ICE_SIPING_GAIN", "ICE_SLIP_SPEED_LOSS", "ICE_SLIP_SPEED_REFERENCE_MPS", "ICE_MELT_FILM_MAX_DEPTH_M", "ICE_MELT_FILM_FRICTION_LOSS", "ICE_FLASH_HEAT_FILM_GAIN", "SNOW_BASE_FRICTION_SCALE", "SNOW_WINTER_COMPOUND_GAIN", "SNOW_SIPING_GAIN", "SNOW_INTERLOCK_GAIN", "SNOW_PACKED_TREAD_GAIN", "SNOW_SLIP_BUILD_GAIN", "SNOW_HIGH_SLIP_LOSS", "SNOW_SLIP_BUILD_REFERENCE_MPS", "SNOW_HIGH_SLIP_REFERENCE_MPS", "ICE_STIFFNESS_SCALE", "SNOW_STIFFNESS_SCALE", "ICE_RR_SCALE", "SNOW_RR_SCALE", "ICE_RELAXATION_SCALE", "SNOW_RELAXATION_SCALE", "MIN_FRICTION_SCALE", "MAX_FRICTION_SCALE", "PACKED_SNOW_PICKUP_HZ", "PACKED_SNOW_BASE_RELEASE_HZ", "PACKED_SNOW_SPEED_RELEASE_PER_M", "PACKED_SNOW_SLIP_RELEASE_PER_M" } },
        { "HERITAGE_SHALLOW_GRANULAR", { "ENABLED", "TREAD_AGGRESSIVENESS", "TREAD_EDGE_DENSITY", "OPEN_VOID_RATIO", "GRANULAR_SHEAR_COUPLING", "BULLDOZING_COUPLING", "PLOWING_COUPLING", "MIN_WORN_TREAD_EFFECTIVENESS", "TREAD_DEPTH_EFFECT_EXPONENT", "MAX_SINKAGE_M", "MAX_GRANULAR_FORCE_RATIO", "MAX_PLOWING_FORCE_RATIO", "MIN_BASE_FRICTION_SCALE", "MAX_BASE_FRICTION_SCALE" } },
        { "HERITAGE_DEFORMABLE_TERRAIN", { "ENABLED", "TREAD_AGGRESSIVENESS", "TREAD_EDGE_DENSITY", "OPEN_VOID_RATIO", "SOIL_SHEAR_COUPLING", "BULLDOZING_COUPLING", "PLOWING_COUPLING", "FLOTATION_COUPLING", "MIN_WORN_TREAD_EFFECTIVENESS", "TREAD_DEPTH_EFFECT_EXPONENT", "MAX_SINKAGE_M", "MAX_TERRAIN_FORCE_RATIO", "MAX_PLOWING_FORCE_RATIO", "MIN_MF_FRICTION_SCALE", "MAX_MF_FRICTION_SCALE" } }
    };

    for (const auto& [section, values] : raw.sections)
    {
        const auto sectionIt = supported.find(section);
        if (sectionIt == supported.end())
        {
            for (const auto& [key, ignored] : values)
                noteUnsupported(data, section, key);
            continue;
        }
        for (const auto& [key, ignored] : values)
        {
            if (std::find(sectionIt->second.begin(), sectionIt->second.end(), key)
                == sectionIt->second.end())
            {
                noteUnsupported(data, section, key);
            }
        }
    }
}

bool finalizeImportedTireProperty(TirePropertyFileLoadResult& result)
{
    if (result.data.magicFormula.unloadedRadiusM <= 0.05)
    {
        result.errorMessage = "Tire property file is missing a usable [DIMENSION] UNLOADED_RADIUS.";
        return false;
    }
    if (result.data.magicFormula.nominalLoadN < 100.0)
    {
        result.errorMessage = "Tire property file is missing a usable [VERTICAL] FNOMIN.";
        return false;
    }
    if (!validMagicFormula62Parameters(result.data.magicFormula))
    {
        result.errorMessage = "Mapped MF6.2 parameters are outside Heritage's numerical validity checks.";
        return false;
    }
    if (result.data.hasMotorcycleContour
        && !validMotorcycleTireProfile(
            result.data.motorcycleProfile,
            result.data.magicFormula.unloadedRadiusM))
    {
        result.errorMessage = "MC_CONTOUR_A/B describe an invalid motorcycle crown profile.";
        return false;
    }

    if (result.data.unsupportedAssignmentCount > 0)
    {
        result.warnings.push_back(
            std::to_string(result.data.unsupportedAssignmentCount)
            + " .tir assignments are preserved as unsupported diagnostics; TIRE02 does not claim their mechanisms are active.");
    }

    result.success = true;
    return true;
}

} // namespace heritage::vehicles::tires::authoring_detail
