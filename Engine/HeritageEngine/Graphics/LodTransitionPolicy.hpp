#pragma once

#include <algorithm>
#include <cmath>

// Heritage Engine master LOD transition policy.
//
// Renderers should not hard-switch visibility or representation at a distance
// boundary. This header provides one cheap, reusable smooth transition rule:
//   * visibility fades in/out near an outer draw-distance boundary;
//   * neighboring LODs receive complementary weights through a blend band;
//   * renderers may alpha-crossfade, dither, or morph geometry/material detail
//     using the returned weights. Morphing is preferred for translucent layers
//     such as tire marks because double-drawing two alpha overlays can darken
//     the result.
//
// The policy is deliberately arithmetic-only and allocation-free. A transition
// evaluation costs a handful of scalar operations and is suitable for large
// instance populations.
namespace heritage::graphics::lod {

struct MasterTransitionPolicy
{
    // Total width of the LOD blend band as a fraction of the boundary distance.
    // The result is clamped so small props still get a readable transition and
    // kilometre-scale LODs do not waste hundreds of metres blending.
    float lodBlendFraction = 0.20f;
    float minimumLodBlendMeters = 12.0f;
    float maximumLodBlendMeters = 80.0f;

    // Fade-to-zero width at the final visibility boundary.
    float visibilityFadeFraction = 0.16f;
    float minimumVisibilityFadeMeters = 20.0f;
    float maximumVisibilityFadeMeters = 120.0f;
};

inline constexpr MasterTransitionPolicy kMasterTransitionPolicy{};

inline float smoothstep01(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float lodBlendWidthMeters(
    float boundaryMeters,
    const MasterTransitionPolicy& policy = kMasterTransitionPolicy)
{
    if (!std::isfinite(boundaryMeters) || boundaryMeters <= 0.0f)
        return policy.minimumLodBlendMeters;
    return std::clamp(
        boundaryMeters * policy.lodBlendFraction,
        policy.minimumLodBlendMeters,
        policy.maximumLodBlendMeters);
}

inline float visibilityFadeWidthMeters(
    float drawDistanceMeters,
    const MasterTransitionPolicy& policy = kMasterTransitionPolicy)
{
    if (!std::isfinite(drawDistanceMeters) || drawDistanceMeters <= 0.0f)
        return policy.minimumVisibilityFadeMeters;
    return std::clamp(
        drawDistanceMeters * policy.visibilityFadeFraction,
        policy.minimumVisibilityFadeMeters,
        policy.maximumVisibilityFadeMeters);
}

struct LodCrossfade
{
    float nearWeight = 1.0f;
    float farWeight = 0.0f;
};

inline LodCrossfade crossfadeAtBoundary(
    float distanceMeters,
    float boundaryMeters,
    float blendWidthMeters = 0.0f,
    const MasterTransitionPolicy& policy = kMasterTransitionPolicy)
{
    const float distance = std::isfinite(distanceMeters)
        ? (std::max)(0.0f, distanceMeters)
        : 0.0f;
    const float width = blendWidthMeters > 0.0f
        ? blendWidthMeters
        : lodBlendWidthMeters(boundaryMeters, policy);
    const float halfWidth = (std::max)(width * 0.5f, 0.0001f);
    const float start = boundaryMeters - halfWidth;
    const float t = smoothstep01((distance - start) / (halfWidth * 2.0f));
    return { 1.0f - t, t };
}

inline LodCrossfade crossfadeAfterBoundary(
    float distanceMeters,
    float boundaryMeters,
    float blendWidthMeters = 0.0f,
    const MasterTransitionPolicy& policy = kMasterTransitionPolicy)
{
    const float distance = std::isfinite(distanceMeters)
        ? (std::max)(0.0f, distanceMeters)
        : 0.0f;
    const float width = blendWidthMeters > 0.0f
        ? blendWidthMeters
        : lodBlendWidthMeters(boundaryMeters, policy);
    const float t = smoothstep01(
        (distance - boundaryMeters) / (std::max)(width, 0.0001f));
    return { 1.0f - t, t };
}

inline float visibilityWeight(
    float distanceMeters,
    float drawDistanceMeters,
    float fadeWidthMeters = 0.0f,
    const MasterTransitionPolicy& policy = kMasterTransitionPolicy)
{
    if (!std::isfinite(distanceMeters) || !std::isfinite(drawDistanceMeters)
        || drawDistanceMeters <= 0.0f)
    {
        return 0.0f;
    }

    const float distance = (std::max)(0.0f, distanceMeters);
    if (distance >= drawDistanceMeters)
        return 0.0f;

    const float width = fadeWidthMeters > 0.0f
        ? fadeWidthMeters
        : visibilityFadeWidthMeters(drawDistanceMeters, policy);
    const float start = (std::max)(0.0f, drawDistanceMeters - width);
    if (distance <= start)
        return 1.0f;

    return 1.0f - smoothstep01(
        (distance - start) / (std::max)(drawDistanceMeters - start, 0.0001f));
}

} // namespace heritage::graphics::lod
