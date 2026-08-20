#pragma once

#include <cstddef>

namespace heritage::physics {

enum class SurfaceMaterial;

// TIRE15B scene-authored physical properties for a deformable drive surface.
// These values belong to the world/surface, not to a tire. Defaults reproduce
// the TIRE15 clean-room terrain presets so existing scenes keep their current
// behavior when no custom GLB metadata is authored.
struct SurfaceDeformableProperties
{
    bool enabled = false;
    bool authored = false;

    double densityKgM3 = 1600.0;
    double initialLooseDepthM = 0.30;
    double initialMoisture = 0.20;

    // Bekker pressure-sinkage: sigma=(kc/b+kphi)*z^n.
    double bekkerKc = 1200.0;
    double bekkerKphi = 1.35e6;
    double sinkageExponent = 1.10;

    // Mohr-Coulomb / Janosi-Hanamoto reduced-order shear properties.
    double cohesionPa = 1000.0;
    double frictionAngleDegrees = 30.0;
    double shearDeformationModulusM = 0.025;

    double compactionStiffnessGain = 2.0;
    double compactionShearGain = 0.25;
    double plasticRutFraction = 0.65;
    double compactionRateHz = 0.70;
    double looseDepthLossPerCompactionM = 0.06;

    double mfBaseFrictionScale = 0.12;
    double baseStiffnessScale = 0.30;
    double rollingResistanceScale = 2.5;
    double relaxationScale = 2.0;
};

// Spatial rain/runoff authoring attached to the same collision triangle as
// the tire material. Defaults describe the material family; scene metadata
// may override invisible construction details that geometry cannot reveal
// (porous asphalt, a grated drain, sealed soil, and so on).
struct SurfaceHydrologyProperties
{
    bool authored = false;

    // Water lost vertically into the material. Dense road asphalt is nearly
    // impermeable; grass/gravel accept considerably more water.
    double infiltrationCapacityMmPerHour = 0.15;

    // Engineered drainage local to this surface cell. Ordinary asphalt is
    // zero: it must shed water by slope toward an outlet. A drain/grate can
    // author a large value without requiring underground pipe geometry.
    double drainageCapacityMmPerHour = 0.0;

    // Reduced-order overland-flow resistance. This is a bounded gameplay-
    // scale coefficient, not a claim of CFD or a surveyed Manning value.
    double flowRoughness = 0.020;

    // Microscopic texture/depression storage that must fill before connected
    // runoff becomes efficient.
    double depressionStorageMm = 0.20;
};

struct SurfaceMaterialProperties
{
    SurfaceDeformableProperties deformable{};
    SurfaceHydrologyProperties hydrology{};

    // Optional authored local surface temperature. Runtime weather may
    // explicitly override it through SurfaceWorld; otherwise this value wins
    // over the material-family fallback.
    bool hasAuthoredSurfaceTemperature = false;
    double authoredSurfaceTemperatureC = 20.0;
};

bool deformableSurfaceMaterial(SurfaceMaterial material);
double defaultSurfaceTemperatureC(SurfaceMaterial material);
SurfaceMaterialProperties defaultSurfaceMaterialProperties(SurfaceMaterial material);
bool validSurfaceDeformableProperties(const SurfaceDeformableProperties& value);
bool validSurfaceMaterialProperties(const SurfaceMaterialProperties& value);

// Weighted blend used by the finite footprint when several deformable surface
// samples share one tire contact patch. Disabled entries are ignored.
SurfaceDeformableProperties blendSurfaceDeformableProperties(
    const SurfaceDeformableProperties* values,
    const double* weights,
    std::size_t count);

} // namespace heritage::physics
