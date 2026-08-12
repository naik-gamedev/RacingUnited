#include "TireCarcass3D.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace heritage::vehicles::tires {
namespace {

constexpr VehicleScalar kPi = 3.141592653589793238462643383279502884;
constexpr VehicleScalar kEpsilon = 1.0e-8;

heritage::math::Vec3 add(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

heritage::math::Vec3 subtract(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

heritage::math::Vec3 scale(
    const heritage::math::Vec3& value,
    VehicleScalar scalar)
{
    return {
        static_cast<float>(static_cast<VehicleScalar>(value.x) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.y) * scalar),
        static_cast<float>(static_cast<VehicleScalar>(value.z) * scalar)
    };
}

VehicleScalar dot(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return static_cast<VehicleScalar>(a.x) * b.x
        + static_cast<VehicleScalar>(a.y) * b.y
        + static_cast<VehicleScalar>(a.z) * b.z;
}

heritage::math::Vec3 cross(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

VehicleScalar lengthSquared(const heritage::math::Vec3& value)
{
    return dot(value, value);
}

VehicleScalar length(const heritage::math::Vec3& value)
{
    return std::sqrt(std::max(lengthSquared(value), VehicleScalar{0.0}));
}

heritage::math::Vec3 normalized(
    const heritage::math::Vec3& value,
    const heritage::math::Vec3& fallback)
{
    const VehicleScalar magnitude = length(value);
    return magnitude > kEpsilon
        ? scale(value, VehicleScalar{1.0} / magnitude)
        : fallback;
}

heritage::math::Vec3 closestPointOnTriangle(
    const heritage::math::Vec3& point,
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b,
    const heritage::math::Vec3& c)
{
    const heritage::math::Vec3 ab = subtract(b, a);
    const heritage::math::Vec3 ac = subtract(c, a);
    const heritage::math::Vec3 ap = subtract(point, a);
    const VehicleScalar d1 = dot(ab, ap);
    const VehicleScalar d2 = dot(ac, ap);
    if (d1 <= 0.0 && d2 <= 0.0)
        return a;

    const heritage::math::Vec3 bp = subtract(point, b);
    const VehicleScalar d3 = dot(ab, bp);
    const VehicleScalar d4 = dot(ac, bp);
    if (d3 >= 0.0 && d4 <= d3)
        return b;

    const VehicleScalar vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
    {
        const VehicleScalar v = d1 / std::max(d1 - d3, kEpsilon);
        return add(a, scale(ab, v));
    }

    const heritage::math::Vec3 cp = subtract(point, c);
    const VehicleScalar d5 = dot(ab, cp);
    const VehicleScalar d6 = dot(ac, cp);
    if (d6 >= 0.0 && d5 <= d6)
        return c;

    const VehicleScalar vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
    {
        const VehicleScalar w = d2 / std::max(d2 - d6, kEpsilon);
        return add(a, scale(ac, w));
    }

    const VehicleScalar va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0)
    {
        const heritage::math::Vec3 bc = subtract(c, b);
        const VehicleScalar w = (d4 - d3)
            / std::max((d4 - d3) + (d5 - d6), kEpsilon);
        return add(b, scale(bc, w));
    }

    const VehicleScalar denom = std::max(va + vb + vc, kEpsilon);
    const VehicleScalar invDenom = VehicleScalar{1.0} / denom;
    const VehicleScalar v = vb * invDenom;
    const VehicleScalar w = vc * invDenom;
    return add(a, add(scale(ab, v), scale(ac, w)));
}

constexpr std::array<VehicleScalar, TireCarcassCrossSectionBands> kBandCoordinates{
    VehicleScalar{-0.96}, VehicleScalar{-0.70}, VehicleScalar{-0.38},
    VehicleScalar{0.0},
    VehicleScalar{0.38}, VehicleScalar{0.70}, VehicleScalar{0.96}
};

// TIRE20 lower-hemisphere structural stations.  Angle is measured from wheel-up
// toward wheel-forward: +90 deg = forward equator, 180 deg = straight bottom,
// +270 deg = rear equator.  There are deliberately no upper-half stations.
constexpr VehicleScalar kLowerStartAngle = kPi * VehicleScalar{0.5};
constexpr VehicleScalar kLowerEndAngle = kPi * VehicleScalar{1.5};
constexpr VehicleScalar kLowerStationStep =
    (kLowerEndAngle - kLowerStartAngle)
    / static_cast<VehicleScalar>(TireCarcassCircumferentialStations - 1);
constexpr VehicleScalar kLowerHemisphereTolerance = VehicleScalar{0.025};

bool lowerHemisphereRadial(VehicleScalar radialUp)
{
    return radialUp <= kLowerHemisphereTolerance;
}

std::size_t nearestLowerStation(VehicleScalar radialUp, VehicleScalar radialForward)
{
    VehicleScalar length2 = radialUp * radialUp + radialForward * radialForward;
    if (length2 <= kEpsilon)
        return TireCarcassCircumferentialStations / 2;
    const VehicleScalar invLength = VehicleScalar{1.0} / std::sqrt(length2);
    radialUp *= invLength;
    radialForward *= invLength;

    VehicleScalar angle = std::atan2(radialForward, radialUp);
    if (angle < 0.0)
        angle += VehicleScalar{2.0} * kPi;
    // Contacts are restricted to the lower half before this function is used.
    angle = std::clamp(angle, kLowerStartAngle, kLowerEndAngle);
    const VehicleScalar station = (angle - kLowerStartAngle) / kLowerStationStep;
    return static_cast<std::size_t>(std::clamp<long long>(
        std::llround(station),
        0,
        static_cast<long long>(TireCarcassCircumferentialStations - 1)));
}

void radialForLowerStation(
    std::size_t station,
    VehicleScalar& radialUp,
    VehicleScalar& radialForward)
{
    station = std::min(station, TireCarcassCircumferentialStations - 1);
    const VehicleScalar angle = kLowerStartAngle
        + static_cast<VehicleScalar>(station) * kLowerStationStep;
    radialUp = std::cos(angle);
    radialForward = std::sin(angle);
}

TireCarcassRegion regionForBand(std::size_t band)
{
    if (band == 0 || band + 1 == TireCarcassCrossSectionBands)
        return TireCarcassRegion::Sidewall;
    if (band == 1 || band + 2 == TireCarcassCrossSectionBands)
        return TireCarcassRegion::Shoulder;
    return TireCarcassRegion::Tread;
}

VehicleScalar regionStiffnessScale(TireCarcassRegion region)
{
    switch (region)
    {
    case TireCarcassRegion::Sidewall:
        return 0.48;
    case TireCarcassRegion::Shoulder:
        return 0.78;
    case TireCarcassRegion::Tread:
    default:
        return 1.0;
    }
}

VehicleScalar radiusAtBand(
    const TireCarcassContact3DDescription& description,
    std::size_t band)
{
    const VehicleScalar sidewallHeight = std::max(
        description.unloadedRadiusM - description.rimRadiusM,
        VehicleScalar{0.02});
    const VehicleScalar axial = std::abs(kBandCoordinates[band]);
    // Rounded shoulder/sidewall envelope: centre tread owns the full unloaded
    // radius, while exposed sidewall control bands sit progressively closer to
    // the rim.  This is a structural collision envelope, not the render mesh.
    const VehicleScalar shoulderDrop = sidewallHeight
        * VehicleScalar{0.28}
        * std::pow(axial, VehicleScalar{2.1});
    return std::max(
        description.unloadedRadiusM - shoulderDrop,
        description.rimRadiusM + sidewallHeight * VehicleScalar{0.28});
}

std::size_t nearestBand(VehicleScalar normalizedAxial)
{
    std::size_t best = 0;
    VehicleScalar bestError = std::numeric_limits<VehicleScalar>::max();
    for (std::size_t i = 0; i < kBandCoordinates.size(); ++i)
    {
        const VehicleScalar error = std::abs(normalizedAxial - kBandCoordinates[i]);
        if (error < bestError)
        {
            bestError = error;
            best = i;
        }
    }
    return best;
}

std::size_t clampedLowerStation(int station)
{
    return static_cast<std::size_t>(std::clamp(
        station,
        0,
        static_cast<int>(TireCarcassCircumferentialStations) - 1));
}

VehicleScalar radiusAtNormalizedAxial(
    const TireCarcassContact3DDescription& description,
    VehicleScalar normalizedAxial)
{
    const VehicleScalar coordinate = std::clamp(
        normalizedAxial,
        kBandCoordinates.front(),
        kBandCoordinates.back());
    for (std::size_t i = 0; i + 1 < kBandCoordinates.size(); ++i)
    {
        const VehicleScalar a = kBandCoordinates[i];
        const VehicleScalar b = kBandCoordinates[i + 1];
        if (coordinate < a || coordinate > b)
            continue;
        const VehicleScalar t = std::abs(b - a) > kEpsilon
            ? (coordinate - a) / (b - a)
            : VehicleScalar{0.0};
        return radiusAtBand(description, i)
            + (radiusAtBand(description, i + 1) - radiusAtBand(description, i)) * t;
    }
    return radiusAtBand(
        description,
        coordinate <= 0.0 ? std::size_t{0} : kBandCoordinates.size() - 1);
}

TireCarcassRegion regionForNormalizedAxial(VehicleScalar normalizedAxial)
{
    const VehicleScalar absoluteAxial = std::abs(normalizedAxial);
    if (absoluteAxial <= VehicleScalar{0.54})
        return TireCarcassRegion::Tread;
    if (absoluteAxial <= VehicleScalar{0.84})
        return TireCarcassRegion::Shoulder;
    return TireCarcassRegion::Sidewall;
}

struct ContinuousShellSample
{
    heritage::math::Vec3 pointWorld{};
    VehicleScalar axialNormalized = 0.0;
    VehicleScalar radialUp = -1.0;
    VehicleScalar radialForward = 0.0;
};

ContinuousShellSample continuousShellPointToward(
    const TireCarcassContact3DDescription& description,
    const TireCarcassContact3DInput& input,
    const heritage::math::Vec3& targetWorld,
    const heritage::math::Vec3& fallbackRadialWorld)
{
    ContinuousShellSample sample;
    const VehicleScalar halfWidth = std::max(
        description.widthM * VehicleScalar{0.5}, VehicleScalar{0.015});
    const heritage::math::Vec3 fromCenter = subtract(
        targetWorld, input.wheelCenterWorld);
    const VehicleScalar rawAxial = dot(fromCenter, input.wheelRightWorld);
    sample.axialNormalized = std::clamp(
        rawAxial / halfWidth,
        VehicleScalar{-0.96},
        VehicleScalar{0.96});

    const heritage::math::Vec3 ringCenter = add(
        input.wheelCenterWorld,
        scale(input.wheelRightWorld, sample.axialNormalized * halfWidth));
    heritage::math::Vec3 radial = subtract(targetWorld, ringCenter);
    radial = subtract(radial,
        scale(input.wheelRightWorld, dot(radial, input.wheelRightWorld)));
    if (lengthSquared(radial) <= kEpsilon)
    {
        radial = subtract(
            fallbackRadialWorld,
            scale(input.wheelRightWorld,
                dot(fallbackRadialWorld, input.wheelRightWorld)));
    }
    radial = normalized(radial, scale(input.wheelUpWorld, -1.0));

    sample.radialUp = std::clamp(
        dot(radial, input.wheelUpWorld), VehicleScalar{-1.0}, VehicleScalar{1.0});
    sample.radialForward = std::clamp(
        dot(radial, input.wheelForwardWorld), VehicleScalar{-1.0}, VehicleScalar{1.0});
    const VehicleScalar radialLength = std::sqrt(std::max(
        sample.radialUp * sample.radialUp
            + sample.radialForward * sample.radialForward,
        VehicleScalar{0.0}));
    if (radialLength > kEpsilon)
    {
        sample.radialUp /= radialLength;
        sample.radialForward /= radialLength;
    }
    else
    {
        sample.radialUp = -1.0;
        sample.radialForward = 0.0;
    }

    const VehicleScalar radius = radiusAtNormalizedAxial(
        description, sample.axialNormalized);
    sample.pointWorld = add(ringCenter, scale(radial, radius));
    return sample;
}

void populateStructuralCoordinates(
    TireCarcassContact3D& contact,
    const TireCarcassContact3DInput& input,
    VehicleScalar axialNormalized,
    VehicleScalar radialUp,
    VehicleScalar radialForward)
{
    contact.structuralAxialNormalized = std::clamp(
        axialNormalized, VehicleScalar{-1.0}, VehicleScalar{1.0});
    VehicleScalar radialLength = std::sqrt(std::max(
        radialUp * radialUp + radialForward * radialForward,
        VehicleScalar{0.0}));
    if (radialLength <= kEpsilon)
    {
        radialUp = -1.0;
        radialForward = 0.0;
        radialLength = 1.0;
    }
    contact.structuralRadialUp = radialUp / radialLength;
    contact.structuralRadialForward = radialForward / radialLength;

    contact.structuralSector = static_cast<std::uint16_t>(
        nearestLowerStation(
            contact.structuralRadialUp,
            contact.structuralRadialForward));
    contact.structuralBand = static_cast<std::uint8_t>(
        nearestBand(contact.structuralAxialNormalized));
    contact.region = regionForNormalizedAxial(contact.structuralAxialNormalized);

    const heritage::math::Vec3 normal = normalized(
        contact.normalWorld, { 0.0f, 1.0f, 0.0f });
    contact.normalWheelLocal = {
        static_cast<float>(dot(normal, input.wheelRightWorld)),
        static_cast<float>(dot(normal, input.wheelUpWorld)),
        static_cast<float>(dot(normal, input.wheelForwardWorld))
    };
}

heritage::math::Vec3 surfacePointForNode(
    const TireCarcassContact3DDescription& description,
    const TireCarcassContact3DInput& input,
    std::size_t sector,
    std::size_t band)
{
    VehicleScalar radialUp = -1.0;
    VehicleScalar radialForward = 0.0;
    radialForLowerStation(sector, radialUp, radialForward);
    const heritage::math::Vec3 radial = normalized(
        add(
            scale(input.wheelUpWorld, radialUp),
            scale(input.wheelForwardWorld, radialForward)),
        scale(input.wheelUpWorld, -1.0));
    const VehicleScalar halfWidth = description.widthM * VehicleScalar{0.5};
    return add(
        input.wheelCenterWorld,
        add(
            scale(input.wheelRightWorld, kBandCoordinates[band] * halfWidth),
            scale(radial, radiusAtBand(description, band))));
}

struct RawContact
{
    TireCarcassContact3D contact;
    VehicleScalar score = 0.0;
};

bool duplicateOfSupport(
    const TireCarcassContact3DDescription& description,
    const TireCarcassContact3DInput& input,
    const TireCarcassContact3D& candidate)
{
    if (!input.supportContactValid)
        return false;

    const heritage::math::Vec3 supportNormal = normalized(
        input.supportNormalWorld, { 0.0f, 1.0f, 0.0f });
    if (dot(candidate.normalWorld, supportNormal)
        < description.supportDuplicateNormalCosine)
    {
        return false;
    }

    // TIRE20: a support-parallel surface is only a duplicate when it is also
    // the SAME support plane.  A raised sidewalk/step top can have exactly the
    // same normal as the road while contacting the lower/leading carcass at a
    // completely different elevation.  Older paths suppressed that valid contact,
    // which made horizontal step tops invisible to the 3D carcass solver.
    const VehicleScalar supportPlaneOffset = dot(
        subtract(candidate.pointWorld, input.supportPointWorld),
        supportNormal);
    const VehicleScalar samePlaneTolerance = std::max(
        description.surfaceSkinM * VehicleScalar{2.0},
        VehicleScalar{0.0025});
    if (std::abs(supportPlaneOffset) > samePlaneTolerance)
        return false;

    // Only suppress the already-authoritative lower road patch.  A wall/kerb
    // or raised lower-half feature is angularly/elevationally distinct and remains
    // even when the wheel is simultaneously supported by the road.
    const heritage::math::Vec3 relative = subtract(
        candidate.tireSurfacePointWorld, input.wheelCenterWorld);
    return dot(relative, supportNormal) < 0.0;
}

VehicleScalar pointDistanceSquared(
    const heritage::math::Vec3& a,
    const heritage::math::Vec3& b)
{
    return lengthSquared(subtract(a, b));
}

heritage::math::Vec3 closestPointOnPrimitive(
    const heritage::physics::NearbyColliderSurface& surface,
    const heritage::math::Vec3& point)
{
    if (surface.shapeType == heritage::physics::ColliderShapeType::Sphere)
    {
        const heritage::math::Vec3 direction = normalized(
            subtract(point, surface.centerWorld),
            { 0.0f, 1.0f, 0.0f });
        return add(surface.centerWorld, scale(direction, surface.radius));
    }

    const heritage::math::Vec3 offset = subtract(point, surface.centerWorld);
    const VehicleScalar x = std::clamp(
        dot(offset, surface.axisXWorld),
        -static_cast<VehicleScalar>(surface.halfExtents.x),
        static_cast<VehicleScalar>(surface.halfExtents.x));
    const VehicleScalar y = std::clamp(
        dot(offset, surface.axisYWorld),
        -static_cast<VehicleScalar>(surface.halfExtents.y),
        static_cast<VehicleScalar>(surface.halfExtents.y));
    const VehicleScalar z = std::clamp(
        dot(offset, surface.axisZWorld),
        -static_cast<VehicleScalar>(surface.halfExtents.z),
        static_cast<VehicleScalar>(surface.halfExtents.z));
    return add(surface.centerWorld,
        add(scale(surface.axisXWorld, x),
            add(scale(surface.axisYWorld, y),
                scale(surface.axisZWorld, z))));
}

bool primitiveNodeContact(
    const heritage::physics::NearbyColliderSurface& surface,
    const heritage::math::Vec3& wheelCenter,
    const heritage::math::Vec3& nodePoint,
    VehicleScalar probeRadius,
    VehicleScalar surfaceSkin,
    heritage::math::Vec3& contactPoint,
    heritage::math::Vec3& contactNormal,
    VehicleScalar& penetration,
    heritage::math::Vec3& obstacleVelocity)
{
    penetration = 0.0;
    if (surface.shapeType == heritage::physics::ColliderShapeType::Sphere)
    {
        const heritage::math::Vec3 sphereToNode = subtract(
            nodePoint, surface.centerWorld);
        const VehicleScalar distance = length(sphereToNode);
        contactNormal = normalized(
            sphereToNode,
            normalized(subtract(wheelCenter, surface.centerWorld),
                { 0.0f, 1.0f, 0.0f }));
        contactPoint = add(
            surface.centerWorld,
            scale(contactNormal, static_cast<VehicleScalar>(surface.radius)));
        penetration = probeRadius + surfaceSkin
            + static_cast<VehicleScalar>(surface.radius) - distance;
    }
    else
    {
        const heritage::math::Vec3 offset = subtract(
            nodePoint, surface.centerWorld);
        const VehicleScalar local[3]{
            dot(offset, surface.axisXWorld),
            dot(offset, surface.axisYWorld),
            dot(offset, surface.axisZWorld)
        };
        const VehicleScalar extents[3]{
            static_cast<VehicleScalar>(surface.halfExtents.x),
            static_cast<VehicleScalar>(surface.halfExtents.y),
            static_cast<VehicleScalar>(surface.halfExtents.z)
        };
        const heritage::math::Vec3 axes[3]{
            surface.axisXWorld, surface.axisYWorld, surface.axisZWorld
        };
        VehicleScalar clamped[3]{
            std::clamp(local[0], -extents[0], extents[0]),
            std::clamp(local[1], -extents[1], extents[1]),
            std::clamp(local[2], -extents[2], extents[2])
        };
        const heritage::math::Vec3 closest = add(
            surface.centerWorld,
            add(scale(axes[0], clamped[0]),
                add(scale(axes[1], clamped[1]), scale(axes[2], clamped[2]))));
        const heritage::math::Vec3 outside = subtract(nodePoint, closest);
        const VehicleScalar outsideDistance = length(outside);
        const bool inside = std::abs(local[0]) <= extents[0]
            && std::abs(local[1]) <= extents[1]
            && std::abs(local[2]) <= extents[2];

        if (!inside)
        {
            contactNormal = normalized(
                outside,
                normalized(subtract(wheelCenter, closest),
                    { 0.0f, 1.0f, 0.0f }));
            contactPoint = closest;
            penetration = probeRadius + surfaceSkin - outsideDistance;
        }
        else
        {
            // A tire node can be numerically inside a thin kerb/box after a
            // substep.  Choose the box face that faces the tire centre, not
            // merely the geometrically nearest face.  The latter creates a
            // false sideways kick when a tread-centre node enters a narrow
            // longitudinal rail exactly at a corner.
            const heritage::math::Vec3 towardTireCenter = normalized(
                subtract(wheelCenter, nodePoint),
                { 0.0f, 1.0f, 0.0f });
            int contactAxis = 0;
            VehicleScalar bestAlignment = std::abs(
                dot(towardTireCenter, axes[0]));
            for (int axis = 1; axis < 3; ++axis)
            {
                const VehicleScalar alignment = std::abs(
                    dot(towardTireCenter, axes[axis]));
                if (alignment > bestAlignment)
                {
                    bestAlignment = alignment;
                    contactAxis = axis;
                }
            }
            const VehicleScalar sign =
                dot(towardTireCenter, axes[contactAxis]) >= 0.0 ? 1.0 : -1.0;
            const VehicleScalar faceDistance = std::max(
                extents[contactAxis] - sign * local[contactAxis],
                VehicleScalar{0.0});
            heritage::math::Vec3 outward = scale(axes[contactAxis], sign);
            contactPoint = add(nodePoint, scale(outward, faceDistance));
            contactNormal = outward;
            penetration = probeRadius + surfaceSkin + faceDistance;
        }
    }

    if (penetration <= 0.0)
        return false;
    if (dot(contactNormal, subtract(wheelCenter, contactPoint)) < 0.0)
        contactNormal = scale(contactNormal, -1.0);

    obstacleVelocity = add(
        surface.linearVelocityWorld,
        cross(surface.angularVelocityRadiansWorld,
            subtract(contactPoint, surface.centerOfMassWorld)));
    return true;
}

} // namespace

TireCarcassContact3DOutput evaluateTireCarcassContact3D(
    const TireCarcassContact3DDescription& description,
    const TireCarcassContact3DInput& input)
{
    TireCarcassContact3DOutput output;
    const bool hasTriangles = input.candidateTriangles
        && input.candidateTriangleCount > 0;
    const bool hasPrimitives = input.candidatePrimitives
        && input.candidatePrimitiveCount > 0;
    const bool hasSupport = input.supportContactValid
        && input.supportNormalForceN > VehicleScalar{0.0};
    if (!description.enabled
        || description.unloadedRadiusM <= 0.05
        || description.widthM <= 0.03
        || description.radialStiffnessNPerM <= 1000.0
        || (!hasTriangles && !hasPrimitives && !hasSupport))
    {
        return output;
    }

    const heritage::math::Vec3 wheelRight = normalized(
        input.wheelRightWorld, { 1.0f, 0.0f, 0.0f });
    heritage::math::Vec3 wheelForward = input.wheelForwardWorld;
    wheelForward = subtract(wheelForward, scale(wheelRight, dot(wheelForward, wheelRight)));
    wheelForward = normalized(wheelForward, { 0.0f, 0.0f, 1.0f });
    heritage::math::Vec3 wheelUp = normalized(
        cross(wheelForward, wheelRight), input.wheelUpWorld);
    if (dot(wheelUp, input.wheelUpWorld) < 0.0)
        wheelUp = scale(wheelUp, -1.0);

    TireCarcassContact3DInput frameInput = input;
    frameInput.wheelRightWorld = wheelRight;
    frameInput.wheelForwardWorld = wheelForward;
    frameInput.wheelUpWorld = wheelUp;

    std::vector<RawContact> raw;
    raw.reserve(1u
        + (std::min)(input.candidateTriangleCount,
            TireCarcassMaximumCandidateTriangles) * 4u
        + (std::min)(input.candidatePrimitiveCount,
            TireCarcassMaximumCandidatePrimitives) * 4u);

    const VehicleScalar halfWidth = description.widthM * VehicleScalar{0.5};

    // TIRE20 primary support contact.  Ordinary flat-road running now enters
    // the same physical carcass manifold used by kerbs/rocks, so its physical
    // compression is guaranteed to reach presentation.  The suspension/
    // unsprung-mass solver already owns this contact's normal impulse; the
    // primarySupport flag prevents a second application later.
    if (hasSupport)
    {
        const heritage::math::Vec3 supportNormal = normalized(
            input.supportNormalWorld, wheelUp);
        ContinuousShellSample shell = continuousShellPointToward(
            description,
            frameInput,
            input.supportPointWorld,
            scale(supportNormal, -1.0));
        if (!lowerHemisphereRadial(shell.radialUp))
        {
            // A downward support query must describe the lower tire.  If a
            // malformed provider reports an upper structural coordinate, keep
            // the support point but map it to the straight-bottom station.
            shell.axialNormalized = std::clamp(
                shell.axialNormalized, VehicleScalar{-0.96}, VehicleScalar{0.96});
            shell.radialUp = -1.0;
            shell.radialForward = 0.0;
            const heritage::math::Vec3 ringCenter = add(
                input.wheelCenterWorld,
                scale(wheelRight, shell.axialNormalized * halfWidth));
            shell.pointWorld = add(
                ringCenter,
                scale(wheelUp, -radiusAtNormalizedAxial(
                    description, shell.axialNormalized)));
        }

        TireCarcassContact3D support;
        support.valid = true;
        support.primarySupport = true;
        support.pointWorld = input.supportPointWorld;
        support.tireSurfacePointWorld = shell.pointWorld;
        support.normalWorld = supportNormal;
        support.penetrationM = std::clamp(
            input.supportCompressionM,
            VehicleScalar{0.0},
            description.maximumCompressionM);
        support.compressionM = support.penetrationM;
        if (support.compressionM <= VehicleScalar{1.0e-6})
        {
            support.compressionM = std::clamp(
                input.supportNormalForceN
                    / std::max(description.radialStiffnessNPerM, VehicleScalar{1000.0}),
                VehicleScalar{0.0},
                description.maximumCompressionM);
            support.penetrationM = support.compressionM;
        }
        support.normalForceN = std::min(
            input.supportNormalForceN, description.maximumNormalForceN);
        populateStructuralCoordinates(
            support,
            frameInput,
            shell.axialNormalized,
            shell.radialUp,
            shell.radialForward);
        support.surfaceMaterial = input.supportSurfaceMaterial;
        support.surfaceWetness = input.supportWetness;

        const heritage::math::Vec3 radialFromAxle = normalized(
            add(scale(wheelUp, shell.radialUp),
                scale(wheelForward, shell.radialForward)),
            scale(wheelUp, -1.0));
        heritage::math::Vec3 rollingTangent = normalized(
            cross(wheelRight, radialFromAxle), wheelForward);
        if (dot(radialFromAxle, wheelUp) < 0.0
            && dot(rollingTangent, wheelForward) < 0.0)
        {
            rollingTangent = scale(rollingTangent, -1.0);
        }
        rollingTangent = subtract(
            rollingTangent,
            scale(support.normalWorld, dot(rollingTangent, support.normalWorld)));
        rollingTangent = normalized(rollingTangent, wheelForward);
        heritage::math::Vec3 lateralTangent = subtract(
            wheelRight,
            scale(support.normalWorld, dot(wheelRight, support.normalWorld)));
        lateralTangent = normalized(
            lateralTangent,
            normalized(cross(support.normalWorld, rollingTangent), wheelRight));
        support.localRollingTangentWorld = rollingTangent;
        support.localLateralTangentWorld = lateralTangent;

        const heritage::math::Vec3 wheelSpinOmega = scale(
            wheelRight, input.wheelAngularVelocityRadPerS);
        const heritage::math::Vec3 spinVelocity = cross(
            wheelSpinOmega, subtract(shell.pointWorld, input.wheelCenterWorld));
        const heritage::math::Vec3 surfaceVelocity = add(
            input.wheelCenterVelocityWorld, spinVelocity);
        support.normalVelocityMps = dot(surfaceVelocity, support.normalWorld);
        support.rollingSurfaceSpeedMps = dot(surfaceVelocity, rollingTangent);
        support.lateralSurfaceSpeedMps = dot(surfaceVelocity, lateralTangent);

        RawContact rawSupport;
        rawSupport.contact = support;
        // Always retain the support representative even when a complex rock/
        // kerb produces the maximum number of secondary contacts.
        rawSupport.score = VehicleScalar{1000.0} + support.compressionM;
        raw.push_back(rawSupport);
    }
    const VehicleScalar boundingRadius = std::sqrt(
        description.unloadedRadiusM * description.unloadedRadiusM
        + halfWidth * halfWidth);
    const VehicleScalar broadRejectDistance = boundingRadius
        + description.nodeProbeRadiusM + VehicleScalar{0.015};

    const std::size_t triangleCount = (std::min)(
        input.candidateTriangleCount,
        TireCarcassMaximumCandidateTriangles);
    for (std::size_t triangleIndex = 0;
         triangleIndex < triangleCount;
         ++triangleIndex)
    {
        const heritage::physics::StaticSceneTriangle& triangle =
            input.candidateTriangles[triangleIndex];
        heritage::math::Vec3 geometricNormal = cross(
            subtract(triangle.b, triangle.a),
            subtract(triangle.c, triangle.a));
        heritage::math::Vec3 normal = normalized(
            geometricNormal,
            normalized(triangle.normal, { 0.0f, 1.0f, 0.0f }));
        if (dot(normal, subtract(input.wheelCenterWorld, triangle.a)) < 0.0)
            normal = scale(normal, -1.0);

        const heritage::math::Vec3 closestToCenter = closestPointOnTriangle(
            input.wheelCenterWorld, triangle.a, triangle.b, triangle.c);
        const heritage::math::Vec3 centerToSurface = subtract(
            closestToCenter, input.wheelCenterWorld);
        if (length(centerToSurface) > broadRejectDistance)
            continue;

        // TIRE20: do NOT reject support-parallel triangles here.  Older code did
        // this before looking at triangle features, so a raised sidewalk top or
        // the boundary edge of a horizontal triangle was discarded simply
        // because its face normal resembled the road normal.  Duplicate flat
        // road support is now removed per CONTACT by duplicateOfSupport(), after
        // the true feature/contact normal and plane elevation are known.

        // Continuous toroidal-shell narrow phase.  The 9 x 7 lattice remains
        // a cheap structural basis, but contact detection must not depend on a
        // world feature happening to land within 4.5 mm of one of those nodes.
        // Alternating closest-point projection between the real triangle and
        // the analytic reduced-order tire shell gives a continuous contact
        // coordinate around circumference AND across width.
        bool continuousTriangleContactAccepted = false;
        {
            heritage::math::Vec3 target = closestToCenter;
            ContinuousShellSample shell = continuousShellPointToward(
                description, frameInput, target, scale(normal, -1.0));
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                target = closestPointOnTriangle(
                    shell.pointWorld, triangle.a, triangle.b, triangle.c);
                shell = continuousShellPointToward(
                    description, frameInput, target, scale(normal, -1.0));
            }

            // The expensive contact domain is intentionally the lower half.
            // Equator contacts are allowed for a tire meeting a vertical rock/
            // wall, but upper-half geometry is ignored by design.
            if (!lowerHemisphereRadial(shell.radialUp))
                continue;

            const heritage::math::Vec3 closest = closestPointOnTriangle(
                shell.pointWorld, triangle.a, triangle.b, triangle.c);
            const VehicleScalar signedDistance = dot(
                subtract(shell.pointWorld, triangle.a), normal);
            const heritage::math::Vec3 projected = subtract(
                shell.pointWorld, scale(normal, signedDistance));
            const heritage::math::Vec3 closestProjected = closestPointOnTriangle(
                projected, triangle.a, triangle.b, triangle.c);
            const VehicleScalar edgeDistance = length(
                subtract(projected, closestProjected));

            // A face hit uses signed plane penetration.  An edge/vertex hit uses
            // the distance of that real feature from the continuous toroidal
            // shell.  Unlike TIRE18's node probe this also detects a feature that
            // has moved several centimetres INSIDE the pneumatic envelope.
            VehicleScalar penetration = 0.0;
            heritage::math::Vec3 contactNormal = normal;
            heritage::math::Vec3 contactPoint = closestProjected;
            const VehicleScalar faceTolerance = std::max(
                description.nodeProbeRadiusM, VehicleScalar{0.003});
            if (edgeDistance <= faceTolerance)
                penetration = description.surfaceSkinM - signedDistance;

            if (penetration <= 0.0)
            {
                const VehicleScalar shellDistance = length(
                    subtract(shell.pointWorld, closest));
                const heritage::math::Vec3 ringCenter = add(
                    input.wheelCenterWorld,
                    scale(wheelRight, shell.axialNormalized * halfWidth));
                heritage::math::Vec3 closestRadial = subtract(closest, ringCenter);
                closestRadial = subtract(
                    closestRadial,
                    scale(wheelRight, dot(closestRadial, wheelRight)));
                const VehicleScalar featureRadius = length(closestRadial);
                const VehicleScalar shellRadius = radiusAtNormalizedAxial(
                    description, shell.axialNormalized);
                const VehicleScalar radialPenetration = shellRadius
                    + description.nodeProbeRadiusM - featureRadius;

                // Only use the radial/edge formulation when the triangle's
                // closest feature is near the shell in 3D or already lies inside
                // the shell.  This prevents a distant coplanar triangle from
                // becoming a contact merely because its infinite plane crosses
                // the tire.
                if (radialPenetration > 0.0
                    && (shellDistance <= description.maximumCompressionM
                            + description.nodeProbeRadiusM
                        || featureRadius <= shellRadius))
                {
                    penetration = radialPenetration;
                    contactPoint = closest;
                    contactNormal = normalized(
                        subtract(input.wheelCenterWorld, closest), normal);
                }
            }

            if (penetration > 0.0)
            {
                TireCarcassContact3D candidate;
                candidate.valid = true;
                candidate.pointWorld = contactPoint;
                candidate.tireSurfacePointWorld = shell.pointWorld;
                candidate.normalWorld = normalized(contactNormal, normal);
                candidate.penetrationM = std::clamp(
                    penetration,
                    VehicleScalar{0.0},
                    description.maximumCompressionM);
                candidate.compressionM = candidate.penetrationM;
                populateStructuralCoordinates(
                    candidate,
                    frameInput,
                    shell.axialNormalized,
                    shell.radialUp,
                    shell.radialForward);
                candidate.surfaceMaterial = triangle.surfaceMaterial;
                candidate.surfaceWetness = triangle.surfaceWetness;

                if (!duplicateOfSupport(description, input, candidate))
                {
                    const heritage::math::Vec3 radialFromAxle = normalized(
                        add(scale(wheelUp, shell.radialUp),
                            scale(wheelForward, shell.radialForward)),
                        scale(candidate.normalWorld, -1.0));
                    heritage::math::Vec3 rollingTangent = normalized(
                        cross(wheelRight, radialFromAxle), wheelForward);
                    if (dot(radialFromAxle, wheelUp) < 0.0
                        && dot(rollingTangent, wheelForward) < 0.0)
                    {
                        rollingTangent = scale(rollingTangent, -1.0);
                    }
                    rollingTangent = subtract(
                        rollingTangent,
                        scale(candidate.normalWorld,
                            dot(rollingTangent, candidate.normalWorld)));
                    rollingTangent = normalized(rollingTangent, wheelForward);
                    heritage::math::Vec3 lateralTangent = subtract(
                        wheelRight,
                        scale(candidate.normalWorld,
                            dot(wheelRight, candidate.normalWorld)));
                    lateralTangent = normalized(
                        lateralTangent,
                        normalized(cross(candidate.normalWorld, rollingTangent), wheelRight));
                    candidate.localRollingTangentWorld = rollingTangent;
                    candidate.localLateralTangentWorld = lateralTangent;

                    const heritage::math::Vec3 wheelSpinOmega = scale(
                        wheelRight, input.wheelAngularVelocityRadPerS);
                    const heritage::math::Vec3 spinVelocity = cross(
                        wheelSpinOmega,
                        subtract(shell.pointWorld, input.wheelCenterWorld));
                    const heritage::math::Vec3 surfaceVelocity = add(
                        input.wheelCenterVelocityWorld, spinVelocity);
                    candidate.normalVelocityMps = dot(
                        surfaceVelocity, candidate.normalWorld);
                    candidate.rollingSurfaceSpeedMps = dot(
                        surfaceVelocity, rollingTangent);
                    candidate.lateralSurfaceSpeedMps = dot(
                        surfaceVelocity, lateralTangent);

                    RawContact rawContact;
                    rawContact.contact = candidate;
                    rawContact.score = candidate.penetrationM
                        * (candidate.region == TireCarcassRegion::Tread
                            ? VehicleScalar{1.08}
                            : (candidate.region == TireCarcassRegion::Shoulder
                                ? VehicleScalar{1.0} : VehicleScalar{0.90}));
                    raw.push_back(rawContact);
                    continuousTriangleContactAccepted = true;
                }
            }
        }

        // The discrete nodes are now a fallback for degenerate/ambiguous cases,
        // not a competing authority that can snap a valid continuous contact
        // back onto the nearest 11.25-degree structural sector.
        if (continuousTriangleContactAccepted)
            continue;

        const VehicleScalar axial = dot(centerToSurface, wheelRight);
        const VehicleScalar normalizedAxial = halfWidth > kEpsilon
            ? std::clamp(axial / halfWidth, VehicleScalar{-1.0}, VehicleScalar{1.0})
            : VehicleScalar{0.0};
        const std::size_t baseBand = nearestBand(normalizedAxial);

        heritage::math::Vec3 radial = subtract(
            centerToSurface, scale(wheelRight, axial));
        if (lengthSquared(radial) <= kEpsilon)
        {
            // When the nearest point lies close to the axle, use the face
            // normal to choose the circumferential side facing the collider.
            radial = scale(normal, -1.0);
            radial = subtract(radial, scale(wheelRight, dot(radial, wheelRight)));
        }
        radial = normalized(radial, scale(wheelUp, -1.0));
        if (!lowerHemisphereRadial(dot(radial, wheelUp)))
            continue;
        const int baseSector = static_cast<int>(nearestLowerStation(
            dot(radial, wheelUp),
            dot(radial, wheelForward)));

        for (int sectorDelta = -1; sectorDelta <= 1; ++sectorDelta)
        {
            const std::size_t sector = clampedLowerStation(baseSector + sectorDelta);
            for (int bandDelta = -1; bandDelta <= 1; ++bandDelta)
            {
                const int bandSigned = static_cast<int>(baseBand) + bandDelta;
                if (bandSigned < 0
                    || bandSigned >= static_cast<int>(TireCarcassCrossSectionBands))
                {
                    continue;
                }
                const std::size_t band = static_cast<std::size_t>(bandSigned);
                const heritage::math::Vec3 tirePoint = surfacePointForNode(
                    description, frameInput, sector, band);
                const VehicleScalar signedDistance = dot(
                    subtract(tirePoint, triangle.a), normal);

                const heritage::math::Vec3 projected = subtract(
                    tirePoint, scale(normal, signedDistance));
                const heritage::math::Vec3 closestProjected = closestPointOnTriangle(
                    projected, triangle.a, triangle.b, triangle.c);
                const VehicleScalar edgeDistance = length(
                    subtract(projected, closestProjected));

                const VehicleScalar probeRadius = description.nodeProbeRadiusM;
                VehicleScalar penetration = 0.0;
                heritage::math::Vec3 contactNormal = normal;
                heritage::math::Vec3 contactPoint = closestProjected;

                if (edgeDistance <= probeRadius)
                {
                    // Plane-face contact.  The tire centre is on the positive
                    // side; negative signed distance means this structural
                    // surface node crossed into the obstacle half-space.
                    penetration = description.surfaceSkinM - signedDistance;
                    if (penetration <= 0.0 && edgeDistance > 0.0)
                    {
                        // Rounded metric edge probe lets a finite-width carcass
                        // meet a sharp authored triangle edge before a sampled
                        // node has crossed the infinite face plane.
                        const VehicleScalar edgeRadiusSquared = probeRadius * probeRadius;
                        const VehicleScalar separationSquared =
                            signedDistance * signedDistance + edgeDistance * edgeDistance;
                        if (signedDistance >= 0.0
                            && separationSquared < edgeRadiusSquared)
                        {
                            penetration = probeRadius
                                - std::sqrt(std::max(
                                    separationSquared, VehicleScalar{0.0}));
                            const heritage::math::Vec3 edgeToNode = subtract(
                                tirePoint, closestProjected);
                            contactNormal = normalized(edgeToNode, normal);
                            if (dot(contactNormal,
                                    subtract(input.wheelCenterWorld, closestProjected)) < 0.0)
                            {
                                contactNormal = scale(contactNormal, -1.0);
                            }
                        }
                    }
                }

                if (penetration <= 0.0)
                    continue;

                TireCarcassContact3D candidate;
                candidate.valid = true;
                candidate.pointWorld = contactPoint;
                candidate.tireSurfacePointWorld = tirePoint;
                candidate.normalWorld = normalized(
                    contactNormal, normal);
                candidate.penetrationM = std::clamp(
                    penetration,
                    VehicleScalar{0.0},
                    description.maximumCompressionM);
                candidate.compressionM = candidate.penetrationM;
                {
                    const heritage::math::Vec3 nodeRelative = subtract(
                        tirePoint, input.wheelCenterWorld);
                    const heritage::math::Vec3 nodeRadial = normalized(
                        subtract(nodeRelative,
                            scale(wheelRight, dot(nodeRelative, wheelRight))),
                        scale(wheelUp, -1.0));
                    populateStructuralCoordinates(
                        candidate,
                        frameInput,
                        kBandCoordinates[band],
                        dot(nodeRadial, wheelUp),
                        dot(nodeRadial, wheelForward));
                }
                candidate.surfaceMaterial = triangle.surfaceMaterial;
                candidate.surfaceWetness = triangle.surfaceWetness;

                if (duplicateOfSupport(description, input, candidate))
                    continue;

                const heritage::math::Vec3 radialFromAxle = normalized(
                    subtract(
                        subtract(tirePoint, input.wheelCenterWorld),
                        scale(wheelRight,
                            dot(subtract(tirePoint, input.wheelCenterWorld), wheelRight))),
                    scale(candidate.normalWorld, -1.0));
                heritage::math::Vec3 rollingTangent = normalized(
                    cross(wheelRight, radialFromAxle), wheelForward);
                // Choose the sign closest to authored wheel-forward over the
                // authoritative lower-half contact domain.
                if (dot(radialFromAxle, wheelUp) < 0.0
                    && dot(rollingTangent, wheelForward) < 0.0)
                {
                    rollingTangent = scale(rollingTangent, -1.0);
                }
                rollingTangent = subtract(
                    rollingTangent,
                    scale(candidate.normalWorld,
                        dot(rollingTangent, candidate.normalWorld)));
                rollingTangent = normalized(rollingTangent, wheelForward);
                heritage::math::Vec3 lateralTangent = subtract(
                    wheelRight,
                    scale(candidate.normalWorld,
                        dot(wheelRight, candidate.normalWorld)));
                lateralTangent = normalized(
                    lateralTangent,
                    normalized(cross(candidate.normalWorld, rollingTangent), wheelRight));
                candidate.localRollingTangentWorld = rollingTangent;
                candidate.localLateralTangentWorld = lateralTangent;

                const heritage::math::Vec3 wheelSpinOmega = scale(
                    wheelRight, input.wheelAngularVelocityRadPerS);
                const heritage::math::Vec3 spinVelocity = cross(
                    wheelSpinOmega,
                    subtract(tirePoint, input.wheelCenterWorld));
                const heritage::math::Vec3 surfaceVelocity = add(
                    input.wheelCenterVelocityWorld, spinVelocity);
                candidate.normalVelocityMps = dot(
                    surfaceVelocity, candidate.normalWorld);
                candidate.rollingSurfaceSpeedMps = dot(
                    surfaceVelocity, rollingTangent);
                candidate.lateralSurfaceSpeedMps = dot(
                    surfaceVelocity, lateralTangent);

                RawContact rawContact;
                rawContact.contact = candidate;
                rawContact.score = candidate.penetrationM
                    * (candidate.region == TireCarcassRegion::Tread
                        ? VehicleScalar{1.0}
                        : (candidate.region == TireCarcassRegion::Shoulder
                            ? VehicleScalar{0.92} : VehicleScalar{0.82}));
                raw.push_back(rawContact);
            }
        }
    }

    const std::size_t primitiveCount = (std::min)(
        input.candidatePrimitiveCount,
        TireCarcassMaximumCandidatePrimitives);
    for (std::size_t primitiveIndex = 0;
         primitiveIndex < primitiveCount;
         ++primitiveIndex)
    {
        const heritage::physics::NearbyColliderSurface& surface =
            input.candidatePrimitives[primitiveIndex];
        const heritage::math::Vec3 closestToCenter = closestPointOnPrimitive(
            surface, input.wheelCenterWorld);
        const heritage::math::Vec3 centerToSurface = subtract(
            closestToCenter, input.wheelCenterWorld);
        if (length(centerToSurface) > broadRejectDistance)
            continue;

        // TIRE20 continuous primitive contact.  As with static triangles, the
        // structural 9 x 7 lattice must not be the collision sampling grid.
        // Find the actual shell location facing the primitive and test that
        // continuous location first.  The old local 3 x 3 nodes remain below as
        // a conservative fallback/regression path.
        bool continuousPrimitiveContactAccepted = false;
        {
            heritage::math::Vec3 target = closestToCenter;
            ContinuousShellSample shell = continuousShellPointToward(
                description,
                frameInput,
                target,
                normalized(centerToSurface, scale(wheelUp, -1.0)));
            for (int iteration = 0; iteration < 2; ++iteration)
            {
                target = closestPointOnPrimitive(surface, shell.pointWorld);
                shell = continuousShellPointToward(
                    description,
                    frameInput,
                    target,
                    normalized(centerToSurface, scale(wheelUp, -1.0)));
            }

            if (!lowerHemisphereRadial(shell.radialUp))
                continue;

            heritage::math::Vec3 contactPoint{};
            heritage::math::Vec3 contactNormal{};
            heritage::math::Vec3 obstacleVelocity{};
            VehicleScalar penetration = 0.0;
            if (primitiveNodeContact(
                    surface,
                    input.wheelCenterWorld,
                    shell.pointWorld,
                    description.nodeProbeRadiusM,
                    description.surfaceSkinM,
                    contactPoint,
                    contactNormal,
                    penetration,
                    obstacleVelocity))
            {
                TireCarcassContact3D candidate;
                candidate.valid = true;
                candidate.pointWorld = contactPoint;
                candidate.tireSurfacePointWorld = shell.pointWorld;
                candidate.normalWorld = normalized(
                    contactNormal, { 0.0f, 1.0f, 0.0f });
                candidate.penetrationM = std::clamp(
                    penetration,
                    VehicleScalar{0.0},
                    description.maximumCompressionM);
                candidate.compressionM = candidate.penetrationM;
                populateStructuralCoordinates(
                    candidate,
                    frameInput,
                    shell.axialNormalized,
                    shell.radialUp,
                    shell.radialForward);
                candidate.surfaceMaterial = surface.surfaceMaterial;
                candidate.surfaceWetness = surface.surfaceWetness;
                candidate.sourceCollider = surface.collider;
                candidate.sourceBody = surface.body;
                candidate.sourceMotionType = surface.motionType;

                if (!duplicateOfSupport(description, input, candidate))
                {
                    const heritage::math::Vec3 radialFromAxle = normalized(
                        add(scale(wheelUp, shell.radialUp),
                            scale(wheelForward, shell.radialForward)),
                        scale(candidate.normalWorld, -1.0));
                    heritage::math::Vec3 rollingTangent = normalized(
                        cross(wheelRight, radialFromAxle), wheelForward);
                    if (dot(radialFromAxle, wheelUp) < 0.0
                        && dot(rollingTangent, wheelForward) < 0.0)
                    {
                        rollingTangent = scale(rollingTangent, -1.0);
                    }
                    rollingTangent = subtract(
                        rollingTangent,
                        scale(candidate.normalWorld,
                            dot(rollingTangent, candidate.normalWorld)));
                    rollingTangent = normalized(rollingTangent, wheelForward);
                    heritage::math::Vec3 lateralTangent = subtract(
                        wheelRight,
                        scale(candidate.normalWorld,
                            dot(wheelRight, candidate.normalWorld)));
                    lateralTangent = normalized(
                        lateralTangent,
                        normalized(cross(candidate.normalWorld, rollingTangent), wheelRight));
                    candidate.localRollingTangentWorld = rollingTangent;
                    candidate.localLateralTangentWorld = lateralTangent;

                    const heritage::math::Vec3 wheelSpinOmega = scale(
                        wheelRight, input.wheelAngularVelocityRadPerS);
                    const heritage::math::Vec3 spinVelocity = cross(
                        wheelSpinOmega,
                        subtract(shell.pointWorld, input.wheelCenterWorld));
                    const heritage::math::Vec3 tireSurfaceVelocity = add(
                        input.wheelCenterVelocityWorld, spinVelocity);
                    const heritage::math::Vec3 relativeSurfaceVelocity = subtract(
                        tireSurfaceVelocity, obstacleVelocity);
                    candidate.normalVelocityMps = dot(
                        relativeSurfaceVelocity, candidate.normalWorld);
                    candidate.rollingSurfaceSpeedMps = dot(
                        relativeSurfaceVelocity, rollingTangent);
                    candidate.lateralSurfaceSpeedMps = dot(
                        relativeSurfaceVelocity, lateralTangent);

                    RawContact rawContact;
                    rawContact.contact = candidate;
                    rawContact.score = candidate.penetrationM
                        * (candidate.region == TireCarcassRegion::Tread
                            ? VehicleScalar{1.08}
                            : (candidate.region == TireCarcassRegion::Shoulder
                                ? VehicleScalar{1.0} : VehicleScalar{0.90}));
                    raw.push_back(rawContact);
                    continuousPrimitiveContactAccepted = true;
                }
            }
        }

        if (continuousPrimitiveContactAccepted)
            continue;

        const VehicleScalar axial = dot(centerToSurface, wheelRight);
        const VehicleScalar normalizedAxial = halfWidth > kEpsilon
            ? std::clamp(axial / halfWidth, VehicleScalar{-1.0}, VehicleScalar{1.0})
            : VehicleScalar{0.0};
        const std::size_t baseBand = nearestBand(normalizedAxial);

        heritage::math::Vec3 radial = subtract(
            centerToSurface, scale(wheelRight, axial));
        if (lengthSquared(radial) <= kEpsilon)
        {
            radial = normalized(
                subtract(closestToCenter, surface.centerWorld),
                scale(wheelUp, -1.0));
            radial = subtract(radial, scale(wheelRight, dot(radial, wheelRight)));
        }
        radial = normalized(radial, scale(wheelUp, -1.0));
        if (!lowerHemisphereRadial(dot(radial, wheelUp)))
            continue;
        const int baseSector = static_cast<int>(nearestLowerStation(
            dot(radial, wheelUp),
            dot(radial, wheelForward)));

        for (int sectorDelta = -1; sectorDelta <= 1; ++sectorDelta)
        {
            const std::size_t sector = clampedLowerStation(baseSector + sectorDelta);
            for (int bandDelta = -1; bandDelta <= 1; ++bandDelta)
            {
                const int bandSigned = static_cast<int>(baseBand) + bandDelta;
                if (bandSigned < 0
                    || bandSigned >= static_cast<int>(TireCarcassCrossSectionBands))
                {
                    continue;
                }
                const std::size_t band = static_cast<std::size_t>(bandSigned);
                const heritage::math::Vec3 tirePoint = surfacePointForNode(
                    description, frameInput, sector, band);

                heritage::math::Vec3 contactPoint{};
                heritage::math::Vec3 contactNormal{};
                heritage::math::Vec3 obstacleVelocity{};
                VehicleScalar penetration = 0.0;
                if (!primitiveNodeContact(
                        surface,
                        input.wheelCenterWorld,
                        tirePoint,
                        description.nodeProbeRadiusM,
                        description.surfaceSkinM,
                        contactPoint,
                        contactNormal,
                        penetration,
                        obstacleVelocity))
                {
                    continue;
                }

                TireCarcassContact3D candidate;
                candidate.valid = true;
                candidate.pointWorld = contactPoint;
                candidate.tireSurfacePointWorld = tirePoint;
                candidate.normalWorld = normalized(
                    contactNormal, { 0.0f, 1.0f, 0.0f });
                candidate.penetrationM = std::clamp(
                    penetration,
                    VehicleScalar{0.0},
                    description.maximumCompressionM);
                candidate.compressionM = candidate.penetrationM;
                {
                    const heritage::math::Vec3 nodeRelative = subtract(
                        tirePoint, input.wheelCenterWorld);
                    const heritage::math::Vec3 nodeRadial = normalized(
                        subtract(nodeRelative,
                            scale(wheelRight, dot(nodeRelative, wheelRight))),
                        scale(wheelUp, -1.0));
                    populateStructuralCoordinates(
                        candidate,
                        frameInput,
                        kBandCoordinates[band],
                        dot(nodeRadial, wheelUp),
                        dot(nodeRadial, wheelForward));
                }
                candidate.surfaceMaterial = surface.surfaceMaterial;
                candidate.surfaceWetness = surface.surfaceWetness;
                candidate.sourceCollider = surface.collider;
                candidate.sourceBody = surface.body;
                candidate.sourceMotionType = surface.motionType;

                if (duplicateOfSupport(description, input, candidate))
                    continue;

                const heritage::math::Vec3 radialFromAxle = normalized(
                    subtract(
                        subtract(tirePoint, input.wheelCenterWorld),
                        scale(wheelRight,
                            dot(subtract(tirePoint, input.wheelCenterWorld), wheelRight))),
                    scale(candidate.normalWorld, -1.0));
                heritage::math::Vec3 rollingTangent = normalized(
                    cross(wheelRight, radialFromAxle), wheelForward);
                if (dot(radialFromAxle, wheelUp) < 0.0
                    && dot(rollingTangent, wheelForward) < 0.0)
                {
                    rollingTangent = scale(rollingTangent, -1.0);
                }
                rollingTangent = subtract(
                    rollingTangent,
                    scale(candidate.normalWorld,
                        dot(rollingTangent, candidate.normalWorld)));
                rollingTangent = normalized(rollingTangent, wheelForward);
                heritage::math::Vec3 lateralTangent = subtract(
                    wheelRight,
                    scale(candidate.normalWorld,
                        dot(wheelRight, candidate.normalWorld)));
                lateralTangent = normalized(
                    lateralTangent,
                    normalized(cross(candidate.normalWorld, rollingTangent), wheelRight));
                candidate.localRollingTangentWorld = rollingTangent;
                candidate.localLateralTangentWorld = lateralTangent;

                const heritage::math::Vec3 wheelSpinOmega = scale(
                    wheelRight, input.wheelAngularVelocityRadPerS);
                const heritage::math::Vec3 spinVelocity = cross(
                    wheelSpinOmega,
                    subtract(tirePoint, input.wheelCenterWorld));
                const heritage::math::Vec3 tireSurfaceVelocity = add(
                    input.wheelCenterVelocityWorld, spinVelocity);
                const heritage::math::Vec3 relativeSurfaceVelocity = subtract(
                    tireSurfaceVelocity, obstacleVelocity);
                candidate.normalVelocityMps = dot(
                    relativeSurfaceVelocity, candidate.normalWorld);
                candidate.rollingSurfaceSpeedMps = dot(
                    relativeSurfaceVelocity, rollingTangent);
                candidate.lateralSurfaceSpeedMps = dot(
                    relativeSurfaceVelocity, lateralTangent);

                RawContact rawContact;
                rawContact.contact = candidate;
                rawContact.score = candidate.penetrationM
                    * (candidate.region == TireCarcassRegion::Tread
                        ? VehicleScalar{1.0}
                        : (candidate.region == TireCarcassRegion::Shoulder
                            ? VehicleScalar{0.92} : VehicleScalar{0.82}));
                raw.push_back(rawContact);
            }
        }
    }

    output.rawCandidateCount = raw.size();
    if (raw.empty())
        return output;

    std::stable_sort(raw.begin(), raw.end(),
        [](const RawContact& a, const RawContact& b) {
            return a.score > b.score;
        });

    const VehicleScalar clusterDistanceSquared =
        description.clusterDistanceM * description.clusterDistanceM;
    for (const RawContact& rawContact : raw)
    {
        if (output.contactCount >= TireCarcassMaximumContacts)
            break;

        bool merged = false;
        for (std::size_t i = 0; i < output.contactCount; ++i)
        {
            TireCarcassContact3D& existing = output.contacts[i];
            if (existing.primarySupport != rawContact.contact.primarySupport
                || existing.sourceCollider != rawContact.contact.sourceCollider
                || existing.sourceBody != rawContact.contact.sourceBody
                || dot(existing.normalWorld, rawContact.contact.normalWorld)
                    < description.clusterNormalCosine
                || pointDistanceSquared(existing.pointWorld,
                        rawContact.contact.pointWorld) > clusterDistanceSquared)
            {
                continue;
            }

            // Keep the deepest representative of a tessellated local patch.
            // This prevents creator triangle density from multiplying stiffness.
            if (rawContact.contact.penetrationM > existing.penetrationM)
                existing = rawContact.contact;
            merged = true;
            break;
        }

        if (!merged)
            output.contacts[output.contactCount++] = rawContact.contact;
    }

    if (output.contactCount == 0)
        return output;

    const VehicleScalar pressureReference = std::max(
        input.referencePressurePa, VehicleScalar{50000.0});
    const VehicleScalar pressureRatio = std::clamp(
        input.inflationPressurePa / pressureReference,
        VehicleScalar{0.35}, VehicleScalar{2.0});
    const VehicleScalar pneumaticStiffnessScale =
        VehicleScalar{0.58} + VehicleScalar{0.42} * pressureRatio;

    // Contacts with near-identical normals act in parallel on one structural
    // patch.  The clustering step already removes duplicates, so divide only
    // mildly by total manifold size; corner contacts retain independent normal
    // authority instead of making a kerb artificially soft.
    const VehicleScalar manifoldScale = VehicleScalar{1.0}
        / std::sqrt(static_cast<VehicleScalar>(output.contactCount));

    for (std::size_t i = 0; i < output.contactCount; ++i)
    {
        TireCarcassContact3D& contact = output.contacts[i];
        if (!contact.primarySupport)
        {
            const VehicleScalar localScale = regionStiffnessScale(contact.region);
            const VehicleScalar stiffness = description.radialStiffnessNPerM
                * pneumaticStiffnessScale * localScale * manifoldScale;
            const VehicleScalar damping = description.radialDampingNSecondsPerM
                * std::sqrt(std::max(localScale, VehicleScalar{0.05}))
                * manifoldScale;
            const VehicleScalar closingSpeed = std::max(
                -contact.normalVelocityMps, VehicleScalar{0.0});

            // TIRE21 authoritative shell stabilization. A soft carcass spring is
            // physically correct, but by itself it can allow centimetres of
            // visible wall/kerb penetration before enough load develops. Convert
            // a bounded fraction of penetration into a target separation speed
            // and the corresponding effective-mass impulse. This is a standard
            // velocity-level contact correction layered on top of the compliant
            // pneumatic force, not a rigid positional teleport.
            const VehicleScalar dt = std::max(
                input.deltaTimeSeconds, VehicleScalar{1.0e-5});
            const VehicleScalar targetSeparationSpeed = std::clamp(
                description.penetrationCorrectionFraction
                    * contact.compressionM / dt,
                VehicleScalar{0.0},
                description.maximumCorrectionSpeedMps);
            const VehicleScalar stabilizationDeltaSpeed = std::max(
                targetSeparationSpeed - contact.normalVelocityMps,
                VehicleScalar{0.0});
            const VehicleScalar stabilizationForce = std::max(
                description.contactEffectiveMassKg, VehicleScalar{1.0})
                * stabilizationDeltaSpeed / dt;

            const VehicleScalar normalForce = stiffness * contact.compressionM
                + damping * closingSpeed
                + stabilizationForce;
            contact.normalForceN = std::clamp(
                normalForce,
                VehicleScalar{0.0},
                description.maximumNormalForceN);
        }
        // Primary support keeps the exact externally solved normal load.  It is
        // still included in telemetry/deformation totals so presentation sees
        // the same tire state that supplies ordinary road traction.
        output.totalNormalForceN += contact.normalForceN;
        output.maximumCompressionM = std::max(
            output.maximumCompressionM, contact.compressionM);
    }

    output.valid = output.contactCount > 0;
    return output;
}

void applyTireCarcassContactImpulsePair(
    heritage::physics::RigidBodySystem& bodies,
    heritage::physics::BodyHandle tireCarrierBody,
    const TireCarcassContact3D& contact,
    const heritage::math::Vec3& forceWorld,
    VehicleScalar deltaTimeSeconds)
{
    if (tireCarrierBody == heritage::physics::InvalidBody
        || deltaTimeSeconds <= 0.0)
    {
        return;
    }

    const heritage::math::Vec3 tireImpulse = scale(
        forceWorld, deltaTimeSeconds);
    bodies.applyImpulseAtPoint(
        tireCarrierBody, tireImpulse, contact.pointWorld);

    if (contact.sourceBody != heritage::physics::InvalidBody
        && contact.sourceBody != tireCarrierBody
        && contact.sourceMotionType == heritage::physics::BodyMotionType::Dynamic)
    {
        bodies.applyImpulseAtPoint(
            contact.sourceBody,
            scale(tireImpulse, VehicleScalar{-1.0}),
            contact.pointWorld);
    }
}

} // namespace heritage::vehicles::tires
