#include "VehicleMassPropertiesAccumulator.hpp"

#include <cmath>

namespace heritage::vehicles {
namespace {

bool finiteScalar(VehicleScalar value)
{
    return std::isfinite(value);
}

bool finiteVector(const heritage::math::Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

VehicleAccumulatedMassProperties accumulateVehicleMassProperties(
    const std::vector<VehicleMassComponent>& components)
{
    VehicleAccumulatedMassProperties result;
    if (components.empty())
        return result;

    VehicleScalar totalMass = 0.0;
    VehicleScalar weightedX = 0.0;
    VehicleScalar weightedY = 0.0;
    VehicleScalar weightedZ = 0.0;

    for (const VehicleMassComponent& component : components)
    {
        if (component.stableId.empty()
            || !finiteScalar(component.massKg)
            || component.massKg <= 0.0
            || component.massKg > 1000000.0
            || !finiteVector(component.centerLocal)
            || !finiteVector(component.inertiaAtCenterKgM2)
            || component.inertiaAtCenterKgM2.x < 0.0f
            || component.inertiaAtCenterKgM2.y < 0.0f
            || component.inertiaAtCenterKgM2.z < 0.0f)
        {
            return result;
        }

        totalMass += component.massKg;
        weightedX += component.massKg * component.centerLocal.x;
        weightedY += component.massKg * component.centerLocal.y;
        weightedZ += component.massKg * component.centerLocal.z;
    }

    if (!finiteScalar(totalMass) || totalMass <= 0.0)
        return result;

    const VehicleScalar comX = weightedX / totalMass;
    const VehicleScalar comY = weightedY / totalMass;
    const VehicleScalar comZ = weightedZ / totalMass;

    VehicleScalar inertiaX = 0.0;
    VehicleScalar inertiaY = 0.0;
    VehicleScalar inertiaZ = 0.0;

    // Parallel-axis theorem around the combined COM. Heritage local axes are
    // X=right, Y=up, Z=forward, so X/Y/Z inertia correspond to pitch/yaw/roll.
    for (const VehicleMassComponent& component : components)
    {
        const VehicleScalar dx = component.centerLocal.x - comX;
        const VehicleScalar dy = component.centerLocal.y - comY;
        const VehicleScalar dz = component.centerLocal.z - comZ;

        inertiaX += component.inertiaAtCenterKgM2.x
            + component.massKg * (dy * dy + dz * dz);
        inertiaY += component.inertiaAtCenterKgM2.y
            + component.massKg * (dx * dx + dz * dz);
        inertiaZ += component.inertiaAtCenterKgM2.z
            + component.massKg * (dx * dx + dy * dy);
    }

    if (!finiteScalar(inertiaX) || !finiteScalar(inertiaY) || !finiteScalar(inertiaZ)
        || inertiaX <= 0.0 || inertiaY <= 0.0 || inertiaZ <= 0.0)
    {
        return result;
    }

    result.totalMassKg = totalMass;
    result.centerOfMassLocal = {
        static_cast<float>(comX),
        static_cast<float>(comY),
        static_cast<float>(comZ)
    };
    result.inertiaLocalKgM2 = {
        static_cast<float>(inertiaX),
        static_cast<float>(inertiaY),
        static_cast<float>(inertiaZ)
    };
    result.valid = true;
    return result;
}

} // namespace heritage::vehicles
