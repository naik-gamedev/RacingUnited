#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

namespace heritage::settings {

inline constexpr int kDefaultWindowWidth = 1280;
inline constexpr int kDefaultWindowHeight = 720;
inline constexpr int kMinimumInteractiveWindowWidth = 800;
inline constexpr int kMinimumInteractiveWindowHeight = 450;

struct VideoSettings
{
    int antiAliasingIndex = 2;
    int textureFilterIndex = 2;
    int scaleModeIndex = 0;
    int fpsCapIndex = 0;
    int renderApiIndex = 0;
    int windowModeIndex = 0;
    bool vsyncEnabled = true;

    // The selected output mode. These are stored as actual values rather
    // than combo-box indices so they remain meaningful if the driver mode
    // list changes or is reordered.
    int resolutionWidth = 0;
    int resolutionHeight = 0;
    int refreshRate = 0;

    float gamma = 2.2f;
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;

    // Last ordinary Windowed-mode rectangle. Keeping this separate from the
    // selected output mode means Borderless and Exclusive do not overwrite
    // the user's preferred window size and position.
    bool windowPlacementValid = false;
    int windowX = 0;
    int windowY = 0;
    int windowWidth = kDefaultWindowWidth;
    int windowHeight = kDefaultWindowHeight;
};

inline constexpr std::array<const char*, 8> kFpsCapOptionNames = {
    "Unlimited",
    "30",
    "60",
    "90",
    "120",
    "144",
    "165",
    "240"
};

inline constexpr std::array<int, 8> kFpsCapValues = {
    0,
    30,
    60,
    90,
    120,
    144,
    165,
    240
};

inline int selectedFpsCap(const VideoSettings& settings) noexcept
{
    const int maximumIndex = static_cast<int>(kFpsCapValues.size()) - 1;
    const int index = std::clamp(settings.fpsCapIndex, 0, maximumIndex);
    return kFpsCapValues[static_cast<std::size_t>(index)];
}

} // namespace heritage::settings
