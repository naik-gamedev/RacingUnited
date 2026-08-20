#include "TireDistributedContact.hpp"

#include <algorithm>
#include <cmath>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kEpsilon = 1.0e-9;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar magnitude(VehicleScalar x, VehicleScalar y)
{
    return std::sqrt(x * x + y * y);
}

std::size_t cellIndex(std::size_t longitudinal, std::size_t lateral)
{
    return longitudinal * kDistributedContactLateralCells + lateral;
}

} // namespace

TireContactWorkEstimate tireContactWorkEstimate(int fidelityTier)
{
    TireContactWorkEstimate result;
    if (fidelityTier == 1)
    {
        // One calibrated MF target plus a fixed 3x3 local allocator. The
        // currently supported road-envelope authoring bound is 5x5.
        result.localBrushCells = kDistributedContactCellCount;
        result.maximumRoadEnvelopeSamples = 25;
    }
    return result;
}

TireDistributedContactOutput evaluateTireDistributedContact(
    const TireModelDescription& tire,
    const TireDistributedContactInput& input)
{
    TireDistributedContactOutput output;
    if (!validTireModelDescription(tire)
        || !finiteValue(input.contactPatchLengthM)
        || !finiteValue(input.contactPatchWidthM)
        || input.contactPatchLengthM <= 0.0
        || input.contactPatchWidthM <= 0.0
        || !finiteValue(input.aggregateInput.normalLoad)
        || input.aggregateInput.normalLoad <= 0.0)
    {
        return output;
    }

    VehicleScalar totalAuthoredWeight = 0.0;
    VehicleScalar supportedAuthoredWeight = 0.0;
    for (const TireDistributedContactCell& cell : input.cells)
    {
        if (!finiteValue(cell.normalLoadFraction)
            || !finiteValue(cell.frictionScale)
            || !finiteValue(cell.stiffnessScale)
            || cell.normalLoadFraction < 0.0
            || cell.frictionScale < 0.0
            || cell.stiffnessScale < 0.0)
        {
            return output;
        }
        totalAuthoredWeight += cell.normalLoadFraction;
        if (cell.supported)
            supportedAuthoredWeight += cell.normalLoadFraction;
    }
    if (totalAuthoredWeight <= kEpsilon
        || supportedAuthoredWeight <= kEpsilon)
    {
        return output;
    }

    output.aggregateBaseline = evaluateAdvancedRoadTire(
        tire, input.aggregateInput);
    output.integrated = output.aggregateBaseline;
    output.supportedLoadFraction = std::clamp(
        supportedAuthoredWeight / totalAuthoredWeight,
        VehicleScalar{0.0}, VehicleScalar{1.0});

    std::array<VehicleScalar, kDistributedContactCellCount> forceX{};
    std::array<VehicleScalar, kDistributedContactCellCount> forceY{};
    std::array<VehicleScalar, kDistributedContactCellCount> capacity{};
    std::array<VehicleScalar, kDistributedContactCellCount> weight{};
    std::array<VehicleScalar, kDistributedContactCellCount> complianceWeight{};

    const VehicleScalar aggregateDemand = magnitude(
        output.aggregateBaseline.longitudinalForce,
        output.aggregateBaseline.lateralForce);
    // MF combined-slip weighting is the calibrated authority. Some fitted
    // datasets legitimately do not describe their combined envelope as a
    // Euclidean mu*Fz circle, so the local allocator may never clip the
    // homogeneous aggregate result merely because its simpler brush capacity
    // metric is more restrictive.
    const VehicleScalar aggregateCapacityPerNormal = std::max(
        output.aggregateBaseline.effectiveFriction,
        aggregateDemand / input.aggregateInput.normalLoad);

    VehicleScalar complianceSum = 0.0;
    for (std::size_t longitudinal = 0;
         longitudinal < kDistributedContactLongitudinalCells;
         ++longitudinal)
    {
        const VehicleScalar normalizedX =
            (static_cast<VehicleScalar>(longitudinal) - 1.0) * 0.5;
        const VehicleScalar x = normalizedX * input.contactPatchLengthM;
        for (std::size_t lateral = 0;
             lateral < kDistributedContactLateralCells;
             ++lateral)
        {
            const std::size_t index = cellIndex(longitudinal, lateral);
            const TireDistributedContactCell& cell = input.cells[index];
            const VehicleScalar normalizedY =
                (static_cast<VehicleScalar>(lateral) - 1.0) * 0.5;
            const VehicleScalar y = normalizedY * input.contactPatchWidthM;
            TireDistributedContactCellOutput& cellOutput = output.cells[index];
            cellOutput.longitudinalOffsetM = x;
            cellOutput.lateralOffsetM = y;
            cellOutput.localLongitudinalSlip =
                input.aggregateInput.longitudinalSlip
                + input.aggregateInput.turnSlipPerM * y;
            cellOutput.localSlipAngleRadians =
                input.aggregateInput.slipAngleRadians
                + input.aggregateInput.turnSlipPerM * x;
            if (!cell.supported)
                continue;

            weight[index] = cell.normalLoadFraction
                / supportedAuthoredWeight;
            cellOutput.normalLoadN = input.aggregateInput.normalLoad
                * weight[index];
            capacity[index] = std::max(
                aggregateCapacityPerNormal * cell.frictionScale
                    * cellOutput.normalLoadN,
                VehicleScalar{0.0});
            cellOutput.capacityN = capacity[index];
            complianceWeight[index] = weight[index]
                * std::max(cell.stiffnessScale, VehicleScalar{0.0});
            complianceSum += complianceWeight[index];
        }
    }
    if (complianceSum <= kEpsilon)
        return output;

    // Allocate the calibrated whole-tire target according to local contact
    // stiffness. Turn-slip has already been evaluated by the fitted whole-
    // tire provider; applying a second uncalibrated force law here would
    // double-count it. Per-cell local slip is retained above for diagnostics
    // and for a future measured brush fit.
    for (std::size_t index = 0; index < input.cells.size(); ++index)
    {
        if (!input.cells[index].supported)
            continue;
        const VehicleScalar share = complianceWeight[index] / complianceSum;
        forceX[index] = output.aggregateBaseline.longitudinalForce * share;
        forceY[index] = output.aggregateBaseline.lateralForce * share;
        const VehicleScalar demand = magnitude(forceX[index], forceY[index]);
        if (demand > capacity[index] && demand > kEpsilon)
        {
            const VehicleScalar scale = capacity[index] / demand;
            forceX[index] *= scale;
            forceY[index] *= scale;
        }
    }

    // Allow bounded shear redistribution into cells that still have capacity.
    // Four deterministic passes are sufficient for nine cells and avoid an
    // unbounded iterative solver in the 1000 Hz vehicle loop.
    for (int pass = 0; pass < 4; ++pass)
    {
        VehicleScalar summedX = 0.0;
        VehicleScalar summedY = 0.0;
        VehicleScalar totalSpare = 0.0;
        for (std::size_t index = 0; index < input.cells.size(); ++index)
        {
            summedX += forceX[index];
            summedY += forceY[index];
            totalSpare += std::max(
                capacity[index] - magnitude(forceX[index], forceY[index]),
                VehicleScalar{0.0});
        }
        const VehicleScalar residualX =
            output.aggregateBaseline.longitudinalForce
            - summedX;
        const VehicleScalar residualY =
            output.aggregateBaseline.lateralForce
            - summedY;
        if (magnitude(residualX, residualY) <= 1.0e-6
            || totalSpare <= kEpsilon)
        {
            break;
        }
        for (std::size_t index = 0; index < input.cells.size(); ++index)
        {
            const VehicleScalar spare = std::max(
                capacity[index] - magnitude(forceX[index], forceY[index]),
                VehicleScalar{0.0});
            if (spare <= 0.0)
                continue;
            forceX[index] += residualX * spare / totalSpare;
            forceY[index] += residualY * spare / totalSpare;
            const VehicleScalar demand = magnitude(forceX[index], forceY[index]);
            if (demand > capacity[index] && demand > kEpsilon)
            {
                const VehicleScalar scale = capacity[index] / demand;
                forceX[index] *= scale;
                forceY[index] *= scale;
            }
        }
    }

    VehicleScalar summedX = 0.0;
    VehicleScalar summedY = 0.0;
    VehicleScalar leverMz = 0.0;
    VehicleScalar normalMx = 0.0;
    VehicleScalar normalMy = 0.0;
    for (std::size_t index = 0; index < input.cells.size(); ++index)
    {
        TireDistributedContactCellOutput& cell = output.cells[index];
        cell.longitudinalForceN = forceX[index];
        cell.lateralForceN = forceY[index];
        cell.utilization = capacity[index] > kEpsilon
            ? std::clamp(
                magnitude(forceX[index], forceY[index]) / capacity[index],
                VehicleScalar{0.0}, VehicleScalar{1.0})
            : 0.0;
        output.maximumCellUtilization = std::max(
            output.maximumCellUtilization, cell.utilization);
        summedX += forceX[index];
        summedY += forceY[index];
        leverMz += cell.longitudinalOffsetM * forceY[index]
            - cell.lateralOffsetM * forceX[index];
        normalMx -= cell.lateralOffsetM * cell.normalLoadN;
        normalMy += cell.longitudinalOffsetM * cell.normalLoadN;
    }

    output.integrated.longitudinalForce = summedX;
    output.integrated.lateralForce = summedY;
    output.integrated.aligningTorque =
        output.aggregateBaseline.aligningTorque
        + leverMz;
    output.integrated.overturningMoment =
        output.aggregateBaseline.overturningMoment
            + normalMx;
    output.integrated.rollingResistanceMoment =
        output.aggregateBaseline.rollingResistanceMoment
            + normalMy;
    const VehicleScalar supportedCapacity =
        output.aggregateBaseline.effectiveFriction
        * input.aggregateInput.normalLoad;
    output.integrated.gripUtilization = supportedCapacity > kEpsilon
        ? std::clamp(
            magnitude(summedX, summedY) / supportedCapacity,
            VehicleScalar{0.0}, VehicleScalar{1.0})
        : 0.0;
    output.forceDifferenceFromAggregateN = magnitude(
        summedX - output.aggregateBaseline.longitudinalForce,
        summedY - output.aggregateBaseline.lateralForce);
    output.valid = finiteValue(summedX) && finiteValue(summedY)
        && finiteValue(output.integrated.aligningTorque)
        && finiteValue(output.integrated.overturningMoment)
        && finiteValue(output.integrated.rollingResistanceMoment);
    return output;
}

} // namespace heritage::vehicles::tires
