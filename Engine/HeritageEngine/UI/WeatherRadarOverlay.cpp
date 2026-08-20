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

} // namespace

void drawWeatherRadarOverlay(
    const heritage::physics::SurfaceWorld& surfaces,
    const heritage::math::DVec3& cameraGlobal)
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

    ImGui::TextUnformatted("World-space regional precipitation");
    ImGui::SameLine();
    ImGui::TextDisabled("250 m storage cells");

    const char* rangeLabels[] = { "5 km", "10 km", "20 km", "40 km" };
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("Radar span", &rangeIndex, rangeLabels, IM_ARRAYSIZE(rangeLabels));
    ImGui::SameLine();
    ImGui::RadioButton("Rain rate", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Accum.", &mode, 1);

    const auto centerSample = precipitation.regionalRainSample(
        cameraGlobal.x, cameraGlobal.z);
    ImGui::Text(
        "Here: %.1f mm/h | accumulated %.2f mm",
        centerSample.currentRateMmPerHour,
        centerSample.cumulativePrecipitationMm);

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
        const float currentScale = static_cast<float>(std::max(
            snapshot.maximumCurrentRateMmPerHour, 1.0));
        const float accumulatedScale = static_cast<float>(std::max(
            snapshot.maximumCumulativePrecipitationMm, 0.25));
        for (std::uint32_t z = 0; z < snapshot.resolution; ++z)
        {
            for (std::uint32_t x = 0; x < snapshot.resolution; ++x)
            {
                const std::size_t index = static_cast<std::size_t>(z)
                    * snapshot.resolution + x;
                const float value = mode == 0
                    ? snapshot.currentRateMmPerHour[index] / currentScale
                    : snapshot.cumulativePrecipitationMm[index] / accumulatedScale;
                if (value <= 0.015f)
                    continue;
                const ImVec2 a(
                    canvasMin.x + static_cast<float>(x) * cellW,
                    canvasMin.y + canvasSize.y - static_cast<float>(z + 1u) * cellH);
                const ImVec2 b(a.x + cellW + 0.5f, a.y + cellH + 0.5f);
                draw->AddRectFilled(a, b, radarColour(value));
            }
        }
    }

    // Range rings, north marker and player position.
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
    draw->AddText(ImVec2(center.x - 4.0f, canvasMin.y + 4.0f),
        IM_COL32(255, 255, 255, 210), "N");
    draw->AddRect(canvasMin, canvasMax, IM_COL32(220, 225, 230, 150));
    ImGui::Dummy(canvasSize);

    ImGui::TextDisabled(
        "Regional rain is persistent and lazily catches up while distant regions are unloaded.");
    ImGui::End();
}

} // namespace heritage::ui
