#pragma once

namespace heritage::graphics::entity_mesh_shadow_config {

// CLEAN05 shadow-quality ownership. The current cascaded shadow map is a
// texture array, so all four layers share one resolution. Keeping the quality
// presets here keeps the live Video Settings quality selector data-driven and
// shared by allocation, diagnostics and validation.
enum class Quality
{
    Low,
    Medium,
    High,
    Ultra
};

inline constexpr int kCascadeCount = 4;
inline constexpr int kLowResolution = 1024;
inline constexpr int kMediumResolution = 1536;
inline constexpr int kHighResolution = 2048;
inline constexpr int kUltraResolution = 3072;
inline constexpr Quality kDefaultQuality = Quality::Ultra;

constexpr int resolutionFor(Quality quality)
{
    switch (quality)
    {
    case Quality::Low: return kLowResolution;
    case Quality::Medium: return kMediumResolution;
    case Quality::Ultra: return kUltraResolution;
    case Quality::High:
    default: return kHighResolution;
    }
}

inline constexpr int kDefaultMapResolution = resolutionFor(kDefaultQuality);
inline constexpr float kNearDistance = 0.10f;
inline constexpr float kDepthPaddingMeters = 180.0f;

} // namespace heritage::graphics::entity_mesh_shadow_config
