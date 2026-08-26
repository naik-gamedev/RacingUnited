#include "WeatherRadarOverlay.hpp"

#include "../Physics/Surfaces/SurfaceWorld.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include <imgui.h>

namespace heritage::ui {
namespace {

ImU32 radarColour(float normalized)
{
    const float t = std::clamp(normalized, 0.0f, 1.0f);
    // Conventional radar progression: transparent/dark -> green -> yellow -> red.
    // Alpha stays high enough to remain readable over the neutral radar panel.
    if (t < 0.34f)
    {
        const float u = t / 0.34f;
        return IM_COL32(
            static_cast<int>(20.0f + 20.0f * u),
            static_cast<int>(90.0f + 130.0f * u),
            static_cast<int>(55.0f + 20.0f * u),
            static_cast<int>(70.0f + 120.0f * u));
    }
    if (t < 0.68f)
    {
        const float u = (t - 0.34f) / 0.34f;
        return IM_COL32(
            static_cast<int>(40.0f + 210.0f * u),
            static_cast<int>(220.0f - 20.0f * u),
            static_cast<int>(75.0f - 55.0f * u),
            205);
    }
    const float u = (t - 0.68f) / 0.32f;
    return IM_COL32(
        250,
        static_cast<int>(200.0f - 155.0f * u),
        static_cast<int>(20.0f + 15.0f * u),
        225);
}

template <std::size_t N>
float normalizeRadarValue(float value, const std::array<float, N>& stops)
{
    static_assert(N >= 2u);
    if (!std::isfinite(value) || value <= stops.front())
        return 0.0f;
    if (value >= stops.back())
        return 1.0f;

    const auto upper = std::upper_bound(stops.begin(), stops.end(), value);
    const std::size_t upperIndex = static_cast<std::size_t>(upper - stops.begin());
    const std::size_t lowerIndex = upperIndex - 1u;
    const float interval = stops[upperIndex] - stops[lowerIndex];
    const float local = interval > 0.0f
        ? (value - stops[lowerIndex]) / interval
        : 0.0f;
    return (static_cast<float>(lowerIndex) + local)
        / static_cast<float>(N - 1u);
}

float normalizeRainRate(float millimetresPerHour)
{
    // A fixed meteorological-style scale keeps colours comparable between
    // radar ranges and weather states. A weak shower must not become red just
    // because it happens to be the strongest rain currently on the display.
    constexpr std::array<float, 9> kStopsMmPerHour{
        0.0f, 0.1f, 0.5f, 2.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f
    };
    return normalizeRadarValue(millimetresPerHour, kStopsMmPerHour);
}

float normalizeAccumulatedRain(float millimetres)
{
    constexpr std::array<float, 9> kStopsMm{
        0.0f, 0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 25.0f, 50.0f
    };
    return normalizeRadarValue(millimetres, kStopsMm);
}

} // namespace

void drawWeatherRadarOverlay(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::DVec3& cameraGlobal,
    double cameraHeadingRadians)
{
    static int rangeIndex = 2;
    static int mode = 0; // 0 current rate, 1 accumulated rain
    static heritage::physics::weather::RainRadarSnapshot snapshot;
    static double lastSnapshotSeconds = -1.0;
    static double lastCenterX = 0.0;
    static double lastCenterZ = 0.0;
    constexpr std::array<double, 4> kHalfRangesM{ 2500.0, 5000.0, 10000.0, 20000.0 };
    constexpr std::uint32_t kRadarResolution = 48u;

    const auto& precipitation = surfaces.precipitation();
    const double now = precipitation.elapsedSeconds();
    const double halfRangeM = kHalfRangesM[static_cast<std::size_t>(rangeIndex)];
    const double movedM = std::hypot(cameraGlobal.x - lastCenterX, cameraGlobal.z - lastCenterZ);
    const bool refresh = !snapshot.valid
        || lastSnapshotSeconds < 0.0
        || now < lastSnapshotSeconds
        || now - lastSnapshotSeconds >= 0.50
        || movedM >= 125.0
        || std::abs(snapshot.halfRangeM - halfRangeM) > 1.0;
    if (refresh)
    {
        precipitation.buildRainRadarSnapshot(
            cameraGlobal.x, cameraGlobal.z, halfRangeM,
            kRadarResolution, snapshot);
        lastSnapshotSeconds = now;
        lastCenterX = cameraGlobal.x;
        lastCenterZ = cameraGlobal.z;
    }

    ImGui::SetNextWindowSize(ImVec2(360.0f, 430.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("HERITAGE WEATHER RADAR [F10]"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Camera-heading regional precipitation");
    ImGui::SameLine();
    ImGui::TextDisabled("deterministic regional weather field (heading-up radar)");

    const char* rangeLabels[] = { "5 km", "10 km", "20 km", "40 km" };
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("Radar span", &rangeIndex, rangeLabels, IM_ARRAYSIZE(rangeLabels));
    ImGui::SameLine();
    ImGui::RadioButton("Rain rate", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Accum.", &mode, 1);

    const auto centerSample = precipitation.regionalWeatherSample(
        cameraGlobal.x, cameraGlobal.z);
    double centerAccumulationMm = 0.0;
    if (snapshot.valid && snapshot.resolution > 0u
        && !snapshot.cumulativePrecipitationMm.empty())
    {
        const std::uint32_t centerIndex = snapshot.resolution / 2u;
        const std::size_t index = static_cast<std::size_t>(centerIndex)
            * snapshot.resolution + centerIndex;
        if (index < snapshot.cumulativePrecipitationMm.size())
            centerAccumulationMm = snapshot.cumulativePrecipitationMm[index];
    }
    ImGui::Text(
        "Here: %.1f mm/h | accumulated %.2f mm",
        centerSample.currentRateMmPerHour,
        centerAccumulationMm);
    const auto surfaceWind = precipitation.atmosphericWindVelocityMps(0.0);
    const auto steeringWind = precipitation.weatherSteeringWindVelocityMps();
    ImGui::TextDisabled(
        "Wind surface %.1f m/s | storm steering %.1f m/s",
        std::hypot(surfaceWind.x, surfaceWind.z),
        std::hypot(steeringWind.x, steeringWind.z));

    const ImVec2 canvasSize(
        std::min(320.0f, ImGui::GetContentRegionAvail().x),
        std::min(320.0f, ImGui::GetContentRegionAvail().x));
    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasMax(canvasMin.x + canvasSize.x, canvasMin.y + canvasSize.y);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(canvasMin, canvasMax, IM_COL32(15, 19, 22, 235));

    if (snapshot.valid && snapshot.resolution > 0u)
    {
        const float cellW = canvasSize.x / static_cast<float>(snapshot.resolution);
        const float cellH = canvasSize.y / static_cast<float>(snapshot.resolution);
        const float cs = static_cast<float>(std::cos(-cameraHeadingRadians));
        const float sn = static_cast<float>(std::sin(-cameraHeadingRadians));
        const ImVec2 center(
            (canvasMin.x + canvasMax.x) * 0.5f,
            (canvasMin.y + canvasMax.y) * 0.5f);
        for (std::uint32_t z = 0; z < snapshot.resolution; ++z)
        {
            for (std::uint32_t x = 0; x < snapshot.resolution; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(z)
                    * snapshot.resolution + x;
                const float value = mode == 0
                    ? normalizeRainRate(snapshot.currentRateMmPerHour[index])
                    : normalizeAccumulatedRain(snapshot.cumulativePrecipitationMm[index]);
                if (value <= 0.015f)
                    continue;
                const float x0 = -canvasSize.x * 0.5f + static_cast<float>(x) * cellW;
                const float x1 = x0 + cellW + 0.5f;
                const float y0 = canvasSize.y * 0.5f - static_cast<float>(z + 1u) * cellH;
                const float y1 = y0 + cellH + 0.5f;
                const auto rot = [&](float px, float py) -> ImVec2
                {
                    return ImVec2(center.x + px * cs - py * sn, center.y + px * sn + py * cs);
                };
                const ImVec2 p0 = rot(x0, y0);
                const ImVec2 p1 = rot(x1, y0);
                const ImVec2 p2 = rot(x1, y1);
                const ImVec2 p3 = rot(x0, y1);
                draw->AddQuadFilled(p0, p1, p2, p3, radarColour(value));
            }
        }
    }

    // Range rings and player position. Heading-up radar rotates the field with
    // the active camera view, while the player remains centred.
    const ImVec2 center(
        (canvasMin.x + canvasMax.x) * 0.5f,
        (canvasMin.y + canvasMax.y) * 0.5f);
    const float radius = canvasSize.x * 0.5f;
    draw->AddCircle(center, radius * 0.50f, IM_COL32(210, 220, 225, 90), 64, 1.0f);
    draw->AddCircle(center, radius * 0.99f, IM_COL32(210, 220, 225, 120), 64, 1.0f);
    draw->AddLine(ImVec2(center.x - 7.0f, center.y), ImVec2(center.x + 7.0f, center.y),
        IM_COL32(255, 255, 255, 235), 2.0f);
    draw->AddLine(ImVec2(center.x, center.y - 7.0f), ImVec2(center.x, center.y + 7.0f),
        IM_COL32(255, 255, 255, 235), 2.0f);
    const ImVec2 forwardTip(center.x, center.y - radius * 0.12f);
    draw->AddTriangleFilled(
        ImVec2(forwardTip.x, forwardTip.y - 7.0f),
        ImVec2(forwardTip.x - 5.0f, forwardTip.y + 4.0f),
        ImVec2(forwardTip.x + 5.0f, forwardTip.y + 4.0f),
        IM_COL32(255, 255, 255, 230));
    draw->AddText(ImVec2(forwardTip.x + 8.0f, forwardTip.y - 10.0f),
        IM_COL32(255, 255, 255, 210), "FWD");
    draw->AddRect(canvasMin, canvasMax, IM_COL32(220, 225, 230, 150));
    ImGui::Dummy(canvasSize);

    ImGui::TextDisabled(mode == 0
        ? "Fixed scale: 0.1 light | 10 heavy | 50+ extreme mm/h"
        : "Fixed accumulation scale: 0.1 trace | 5 wet | 25+ saturated mm");

    ImGui::TextDisabled(
        "Same regional weather authority drives radar, volumetric clouds, local rain and cloud shadows.");
    ImGui::End();
}

} // namespace heritage::ui
