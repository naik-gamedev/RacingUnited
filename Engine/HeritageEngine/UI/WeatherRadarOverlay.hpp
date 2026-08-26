#pragma once

#include "../Core/Math/Math.hpp"

namespace heritage::physics { class SurfaceWorld; }

namespace heritage::ui {

// WEATHER08 developer/gameplay-facing regional rain radar. The data is owned
// by the world precipitation field, not by this UI. The overlay is merely one
// view of current regional rain intensity and accumulated precipitation.
void drawWeatherRadarOverlay(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::DVec3& cameraGlobal,
    double cameraHeadingRadians);

} // namespace heritage::ui
