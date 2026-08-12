#include "TireRoadEnveloping.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kOffsetToleranceM = 1.0e-6;

bool finiteValue(VehicleScalar value)
{
    return std::isfinite(static_cast<double>(value));
}

VehicleScalar superEllipseSag(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar relativeX)
{
    const VehicleScalar a = std::max(d.ellipseLengthM, VehicleScalar{0.001});
    const VehicleScalar b = std::max(d.ellipseHeightM, VehicleScalar{0.001});
    const VehicleScalar n = std::max(d.ellipseOrder, VehicleScalar{1.0});
    const VehicleScalar q = std::abs(relativeX) / a;
    if (q >= 1.0)
        return b;

    const VehicleScalar inside = std::max(
        VehicleScalar{1.0} - std::pow(q, n),
        VehicleScalar{0.0});
    const VehicleScalar verticalFraction = std::pow(
        inside, VehicleScalar{1.0} / n);
    return b * (VehicleScalar{1.0} - verticalFraction);
}

VehicleScalar supportCam(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar camCenterX,
    const std::vector<const TireRoadEnvelopeSample*>& samples,
    std::size_t& validCount)
{
    VehicleScalar support = -std::numeric_limits<VehicleScalar>::infinity();
    for (const TireRoadEnvelopeSample* sample : samples)
    {
        if (!sample || !sample->valid
            || !finiteValue(sample->longitudinalOffsetM)
            || !finiteValue(sample->roadHeightRelativeToCenterM))
        {
            continue;
        }
        ++validCount;
        const VehicleScalar roadHeight = std::clamp(
            sample->roadHeightRelativeToCenterM,
            -d.maximumRoadStepM,
            d.maximumRoadStepM);
        const VehicleScalar sag = superEllipseSag(
            d, sample->longitudinalOffsetM - camCenterX);
        support = std::max(support, roadHeight - sag);
    }

    return finiteValue(support) ? support : VehicleScalar{0.0};
}

int boundedOddAxisCount(int requested, int maximumAxisSamples)
{
    const int maximum = std::max(1, maximumAxisSamples);
    int count = std::clamp(requested, 1, maximum);
    if (count > 1 && (count % 2) == 0)
    {
        if (count < maximum)
            ++count;
        else
            --count;
    }
    return std::max(count, 1);
}

std::vector<VehicleScalar> evenlySpacedOffsets(VehicleScalar halfSpan, int count)
{
    std::vector<VehicleScalar> values;
    count = std::max(count, 1);
    values.reserve(static_cast<std::size_t>(count));
    if (count == 1 || halfSpan <= 1.0e-9)
    {
        values.push_back(0.0);
        return values;
    }

    for (int i = 0; i < count; ++i)
    {
        const VehicleScalar t = static_cast<VehicleScalar>(i)
            / static_cast<VehicleScalar>(count - 1);
        values.push_back(-halfSpan + VehicleScalar{2.0} * halfSpan * t);
    }
    return values;
}

bool nearlyEqual(VehicleScalar a, VehicleScalar b)
{
    return std::abs(a - b) <= kOffsetToleranceM;
}

} // namespace

bool validTireRoadEnvelopingDescription(
    const TireRoadEnvelopingDescription& d)
{
    if (!d.enabled)
        return true;

    return finiteValue(d.ellipseShiftScale)
        && d.ellipseShiftScale >= 0.0 && d.ellipseShiftScale <= 4.0
        && finiteValue(d.ellipseLengthM)
        && d.ellipseLengthM >= 0.005 && d.ellipseLengthM <= 2.0
        && finiteValue(d.ellipseHeightM)
        && d.ellipseHeightM >= 0.001 && d.ellipseHeightM <= 1.0
        && finiteValue(d.ellipseOrder)
        && d.ellipseOrder >= 1.0 && d.ellipseOrder <= 10.0
        && finiteValue(d.maximumRoadStepM)
        && d.maximumRoadStepM >= 0.005 && d.maximumRoadStepM <= 1.0
        && d.widthCamCount >= 1 && d.widthCamCount <= 25
        && d.sideCamCount >= 1 && d.sideCamCount <= 25
        && finiteValue(d.effectiveHeightAttenuation)
        && d.effectiveHeightAttenuation >= 0.0
        && d.effectiveHeightAttenuation <= 2.0
        && finiteValue(d.effectivePlaneAngleAttenuation)
        && d.effectivePlaneAngleAttenuation >= 0.0
        && d.effectivePlaneAngleAttenuation <= 2.0
        && finiteValue(d.quietQueryIntervalSeconds)
        && d.quietQueryIntervalSeconds >= 0.001
        && d.quietQueryIntervalSeconds <= 0.10
        && finiteValue(d.queryIntervalSeconds)
        && d.queryIntervalSeconds >= 0.001
        && d.queryIntervalSeconds <= 0.05
        && d.quietQueryIntervalSeconds >= d.queryIntervalSeconds
        && finiteValue(d.refinementHeightThresholdM)
        && d.refinementHeightThresholdM >= 0.0001
        && d.refinementHeightThresholdM <= d.maximumRoadStepM
        && finiteValue(d.refinementWetnessThreshold)
        && d.refinementWetnessThreshold >= 0.0
        && d.refinementWetnessThreshold <= 1.0
        && finiteValue(d.lateralFootprintScale)
        && d.lateralFootprintScale > 0.05
        && d.lateralFootprintScale <= 1.5
        && d.maximumAxisSamples >= 1
        && d.maximumAxisSamples <= 9;
}

VehicleScalar roadEnvelopeCamCenterOffsetM(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar contactPatchLengthM)
{
    const VehicleScalar halfPatch = std::max(
        contactPatchLengthM * VehicleScalar{0.5}, VehicleScalar{0.01});
    return halfPatch * std::clamp(
        d.ellipseShiftScale, VehicleScalar{0.0}, VehicleScalar{4.0});
}

VehicleScalar roadEnvelopeLateralHalfSpanM(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar contactPatchWidthM)
{
    const VehicleScalar halfWidth = std::max(
        VehicleScalar{0.5} * contactPatchWidthM, VehicleScalar{0.015});
    return halfWidth * std::clamp(
        d.lateralFootprintScale, VehicleScalar{0.05}, VehicleScalar{1.5});
}

std::vector<TireRoadEnvelopeOffset> buildTireRoadEnvelopeSamplePattern(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar contactPatchLengthM,
    VehicleScalar contactPatchWidthM,
    bool refined)
{
    std::vector<TireRoadEnvelopeOffset> result;
    if (!d.enabled || !validTireRoadEnvelopingDescription(d))
        return result;

    const VehicleScalar halfLength = roadEnvelopeCamCenterOffsetM(
        d, contactPatchLengthM);
    const VehicleScalar halfWidth = roadEnvelopeLateralHalfSpanM(
        d, contactPatchWidthM);

    const int longitudinalCount = boundedOddAxisCount(
        std::max(d.sideCamCount, 3), d.maximumAxisSamples);
    const int lateralCount = boundedOddAxisCount(
        std::max(d.widthCamCount, 3), d.maximumAxisSamples);
    const auto xs = evenlySpacedOffsets(halfLength, longitudinalCount);
    const auto ys = evenlySpacedOffsets(halfWidth, lateralCount);

    if (refined || !d.adaptive2D)
    {
        result.reserve(xs.size() * ys.size());
        for (VehicleScalar y : ys)
            for (VehicleScalar x : xs)
                result.push_back({ x, y });
        return result;
    }

    // Five-point (or N-axis) cross: all longitudinal centre-row samples plus
    // all lateral centre-column samples. This catches height and split-surface
    // discontinuities cheaply before deciding whether the corners are useful.
    result.reserve(xs.size() + ys.size() - 1);
    for (VehicleScalar x : xs)
        result.push_back({ x, 0.0 });
    for (VehicleScalar y : ys)
    {
        if (!nearlyEqual(y, 0.0))
            result.push_back({ 0.0, y });
    }
    return result;
}

bool tireRoadEnvelopeNeedsHeightRefinement(
    const TireRoadEnvelopingDescription& d,
    const std::vector<TireRoadEnvelopeSample>& samples)
{
    VehicleScalar minimum = std::numeric_limits<VehicleScalar>::infinity();
    VehicleScalar maximum = -std::numeric_limits<VehicleScalar>::infinity();
    std::size_t valid = 0;
    for (const auto& sample : samples)
    {
        if (!sample.valid || !finiteValue(sample.roadHeightRelativeToCenterM))
            continue;
        minimum = std::min(minimum, sample.roadHeightRelativeToCenterM);
        maximum = std::max(maximum, sample.roadHeightRelativeToCenterM);
        ++valid;
    }
    return valid >= 2 && maximum - minimum >= d.refinementHeightThresholdM;
}

bool tireRoadEnvelopeHasPartialSupport(
    const std::vector<TireRoadEnvelopeSample>& samples)
{
    bool haveSupported = false;
    bool haveUnsupported = false;
    for (const auto& sample : samples)
    {
        if (sample.valid)
            haveSupported = true;
        else
            haveUnsupported = true;
        if (haveSupported && haveUnsupported)
            return true;
    }
    return false;
}

VehicleScalar roadEnvelopeLocalPlaneHeightM(
    VehicleScalar longitudinalOffsetM,
    VehicleScalar roadNormalAlongForward,
    VehicleScalar roadNormalAlongSupportRay)
{
    return roadEnvelopeLocalPlaneHeightM(
        longitudinalOffsetM,
        VehicleScalar{0.0},
        roadNormalAlongForward,
        VehicleScalar{0.0},
        roadNormalAlongSupportRay);
}

VehicleScalar roadEnvelopeLocalPlaneHeightM(
    VehicleScalar longitudinalOffsetM,
    VehicleScalar lateralOffsetM,
    VehicleScalar roadNormalAlongForward,
    VehicleScalar roadNormalAlongRight,
    VehicleScalar roadNormalAlongSupportRay)
{
    if (!finiteValue(longitudinalOffsetM)
        || !finiteValue(lateralOffsetM)
        || !finiteValue(roadNormalAlongForward)
        || !finiteValue(roadNormalAlongRight)
        || !finiteValue(roadNormalAlongSupportRay)
        || std::abs(roadNormalAlongSupportRay) <= VehicleScalar{1.0e-4})
    {
        return VehicleScalar{0.0};
    }

    return (longitudinalOffsetM * roadNormalAlongForward
        + lateralOffsetM * roadNormalAlongRight)
        / roadNormalAlongSupportRay;
}

TireRoadEnvelopeOutput evaluateTireRoadEnvelope(
    const TireRoadEnvelopingDescription& d,
    VehicleScalar contactPatchLengthM,
    const std::vector<TireRoadEnvelopeSample>& samples)
{
    TireRoadEnvelopeOutput out;
    out.totalSampleCount = samples.size();
    if (!d.enabled || !validTireRoadEnvelopingDescription(d)
        || !finiteValue(contactPatchLengthM) || samples.empty())
    {
        return out;
    }

    VehicleScalar minHeight = std::numeric_limits<VehicleScalar>::infinity();
    VehicleScalar maxHeight = -std::numeric_limits<VehicleScalar>::infinity();
    std::vector<VehicleScalar> lateralRows;
    for (const auto& sample : samples)
    {
        if (!sample.valid || !finiteValue(sample.roadHeightRelativeToCenterM))
            continue;
        ++out.validSampleCount;
        minHeight = std::min(minHeight, sample.roadHeightRelativeToCenterM);
        maxHeight = std::max(maxHeight, sample.roadHeightRelativeToCenterM);
        const auto existing = std::find_if(lateralRows.begin(), lateralRows.end(),
            [&](VehicleScalar y) { return nearlyEqual(y, sample.lateralOffsetM); });
        if (existing == lateralRows.end())
            lateralRows.push_back(sample.lateralOffsetM);
    }

    if (out.validSampleCount < 2)
        return out;

    out.supportedFraction = static_cast<VehicleScalar>(out.validSampleCount)
        / static_cast<VehicleScalar>(std::max<std::size_t>(samples.size(), 1));
    out.roughnessHeightRangeM = maxHeight - minHeight;

    const VehicleScalar camOffset = roadEnvelopeCamCenterOffsetM(
        d, contactPatchLengthM);
    struct RowResult
    {
        VehicleScalar lateralOffsetM = 0.0;
        VehicleScalar heightM = 0.0;
        VehicleScalar slopeRadians = 0.0;
        std::size_t count = 0;
    };
    std::vector<RowResult> rows;

    for (VehicleScalar rowY : lateralRows)
    {
        std::vector<const TireRoadEnvelopeSample*> rowSamples;
        for (const auto& sample : samples)
        {
            if (sample.valid && nearlyEqual(sample.lateralOffsetM, rowY))
                rowSamples.push_back(&sample);
        }
        if (rowSamples.size() < 2)
            continue;

        std::size_t frontCount = 0;
        std::size_t rearCount = 0;
        const VehicleScalar front = supportCam(
            d, camOffset, rowSamples, frontCount);
        const VehicleScalar rear = supportCam(
            d, -camOffset, rowSamples, rearCount);
        if (std::min(frontCount, rearCount) < 2)
            continue;

        const VehicleScalar camSpan = std::max(
            VehicleScalar{2.0} * camOffset, VehicleScalar{0.01});
        rows.push_back({
            rowY,
            VehicleScalar{0.5} * (front + rear),
            std::atan2(front - rear, camSpan),
            rowSamples.size()
        });
    }

    // A TIRE05 centre-row envelope remains a valid fallback when the adaptive
    // cross did not need corner refinement. Full 3x3+ sampling naturally adds
    // outer row solutions and therefore 3D support/cross-slope information.
    if (rows.empty())
        return out;

    VehicleScalar weightedHeight = 0.0;
    VehicleScalar weightedSlope = 0.0;
    VehicleScalar totalWeight = 0.0;
    for (const RowResult& row : rows)
    {
        const VehicleScalar weight = static_cast<VehicleScalar>(row.count);
        weightedHeight += row.heightM * weight;
        weightedSlope += row.slopeRadians * weight;
        totalWeight += weight;
    }
    if (totalWeight <= 0.0)
        return out;

    out.effectiveRoadHeightM = weightedHeight / totalWeight
        * d.effectiveHeightAttenuation;
    out.effectiveRoadSlopeRadians = weightedSlope / totalWeight
        * d.effectivePlaneAngleAttenuation;

    // Preserve the historical front/rear values using the row closest to the
    // wheel centre. These are useful diagnostics and maintain TIRE05 behavior.
    const auto centreRowIt = std::min_element(rows.begin(), rows.end(),
        [](const RowResult& a, const RowResult& b) {
            return std::abs(a.lateralOffsetM) < std::abs(b.lateralOffsetM);
        });
    if (centreRowIt != rows.end())
    {
        const VehicleScalar camSpan = std::max(
            VehicleScalar{2.0} * camOffset, VehicleScalar{0.01});
        const VehicleScalar delta = std::tan(centreRowIt->slopeRadians) * camSpan;
        out.frontCamHeightM = centreRowIt->heightM + VehicleScalar{0.5} * delta;
        out.rearCamHeightM = centreRowIt->heightM - VehicleScalar{0.5} * delta;
    }

    if (rows.size() >= 2)
    {
        const auto left = std::min_element(rows.begin(), rows.end(),
            [](const RowResult& a, const RowResult& b) {
                return a.lateralOffsetM < b.lateralOffsetM;
            });
        const auto right = std::max_element(rows.begin(), rows.end(),
            [](const RowResult& a, const RowResult& b) {
                return a.lateralOffsetM < b.lateralOffsetM;
            });
        const VehicleScalar span = right->lateralOffsetM - left->lateralOffsetM;
        if (std::abs(span) > 0.005)
        {
            out.effectiveCrossSlopeRadians = std::atan2(
                right->heightM - left->heightM,
                span) * d.effectivePlaneAngleAttenuation;
        }
    }
    else
    {
        // Coarse cross: infer cross-slope from the centre-column support points
        // even though they do not have enough longitudinal samples to form a
        // complete tandem-cam row.
        const TireRoadEnvelopeSample* left = nullptr;
        const TireRoadEnvelopeSample* right = nullptr;
        for (const auto& sample : samples)
        {
            if (!sample.valid || std::abs(sample.longitudinalOffsetM) > kOffsetToleranceM)
                continue;
            if (!left || sample.lateralOffsetM < left->lateralOffsetM)
                left = &sample;
            if (!right || sample.lateralOffsetM > right->lateralOffsetM)
                right = &sample;
        }
        if (left && right)
        {
            const VehicleScalar span = right->lateralOffsetM - left->lateralOffsetM;
            if (std::abs(span) > 0.005)
            {
                out.effectiveCrossSlopeRadians = std::atan2(
                    right->roadHeightRelativeToCenterM
                        - left->roadHeightRelativeToCenterM,
                    span) * d.effectivePlaneAngleAttenuation;
            }
        }
    }

    out.valid = true;
    return out;
}

} // namespace heritage::vehicles::tires
