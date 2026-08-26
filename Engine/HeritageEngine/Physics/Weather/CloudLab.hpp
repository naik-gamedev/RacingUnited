#pragma once

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace heritage::physics::weather {

// CLOUDLAB02 persistent cloud-authoring controls. Live edits remain isolated
// from scene/weather serialization, while an explicit human-readable preset in
// UserData/CloudLabPreset.cfg can be saved and automatically reloaded at startup.
struct CloudLabSettings
{
    bool enabled = true;
    int shapeSolver = 5; // 0 adaptive, 1 cumulus, 2 soft-cumulus, 3 stratus, 4 cellular towers, 5 URP/HDRP port

    // Visible noon sky presentation.
    float noonHorizonR = 0.000f;
    float noonHorizonG = 0.525f;
    float noonHorizonB = 0.906f;
    float noonZenithR = 0.000f;
    float noonZenithG = 0.380f;
    float noonZenithB = 0.839f;
    float noonSkyBlend = 0.995f;
    float skyOvercastStart = 0.72f;
    float skyOvercastEnd = 0.98f;

    // Cloud lighting / tonal response.
    float stormDarkening = 0.10f;
    float interiorDarkening = 0.02f;
    float atmosphereTint = 0.03f;
    float atmosphereChromaticTint = 0.0f;
    float directSunStrength = 0.24f;
    float silverEdgeStrength = 0.06f;
    float thinBrightStrength = 0.045f;
    float multipleScatterMin = 0.74f;
    float multipleScatterMax = 0.90f;
    float multipleScatterTransmissionFloor = 0.92f;
    float nightBrightness = 0.30f;
    float noonReferenceBlend = 0.985f;
    float finalNoonContrast = 0.985f;
    float extinctionClear = 0.00082f;
    float extinctionStorm = 0.00105f;
    float bodyOpacityBoost = 1.70f;
    float sunAbsorptionClear = 0.00062f;
    float sunAbsorptionStorm = 0.00112f;

    // Reference-like cloud colours, linear shader values.
    float noonShadowR = 0.76f;
    float noonShadowG = 0.80f;
    float noonShadowB = 0.86f;
    float noonBodyR = 0.88f;
    float noonBodyG = 0.90f;
    float noonBodyB = 0.94f;
    float noonHighlightR = 0.975f;
    float noonHighlightG = 0.975f;
    float noonHighlightB = 0.985f;

    // Shape / density controls shared by all solver modes.
    float macroScaleM = 72000.0f;
    float mediumScaleM = 30000.0f;
    float smallScaleM = 15000.0f;
    float detailScaleM = 4200.0f;
    float detailVerticalScaleM = 3200.0f;
    float thresholdClear = 0.77f;
    float thresholdCovered = 0.33f;
    float stormThresholdBias = 0.045f;
    float erosionInterior = 0.10f;
    float erosionBoundary = 0.22f;
    float densitySoftLow = -0.045f;
    float densitySoftHigh = 0.105f;
    float cellularStrength = 1.0f;
    float towerStrength = 1.0f;
    float verticalRoundness = 1.0f;
};

inline bool applyCloudLabValue(CloudLabSettings& s, std::string_view key, float value)
{
    const auto clamp = [](float v, float lo, float hi) { return std::clamp(v, lo, hi); };
#define CLOUDLAB_SET(name, lo, hi) if (key == #name) { s.name = clamp(value, lo, hi); return true; }
    CLOUDLAB_SET(noonHorizonR, 0.0f, 2.0f)
    CLOUDLAB_SET(noonHorizonG, 0.0f, 2.0f)
    CLOUDLAB_SET(noonHorizonB, 0.0f, 2.0f)
    CLOUDLAB_SET(noonZenithR, 0.0f, 2.0f)
    CLOUDLAB_SET(noonZenithG, 0.0f, 2.0f)
    CLOUDLAB_SET(noonZenithB, 0.0f, 2.0f)
    CLOUDLAB_SET(noonSkyBlend, 0.0f, 1.0f)
    CLOUDLAB_SET(skyOvercastStart, 0.0f, 1.0f)
    CLOUDLAB_SET(skyOvercastEnd, 0.0f, 1.0f)
    CLOUDLAB_SET(stormDarkening, 0.0f, 1.0f)
    CLOUDLAB_SET(interiorDarkening, 0.0f, 1.0f)
    CLOUDLAB_SET(atmosphereTint, 0.0f, 1.0f)
    CLOUDLAB_SET(atmosphereChromaticTint, 0.0f, 1.0f)
    CLOUDLAB_SET(directSunStrength, 0.0f, 2.0f)
    CLOUDLAB_SET(silverEdgeStrength, 0.0f, 1.0f)
    CLOUDLAB_SET(thinBrightStrength, 0.0f, 1.0f)
    CLOUDLAB_SET(multipleScatterMin, 0.0f, 2.0f)
    CLOUDLAB_SET(multipleScatterMax, 0.0f, 2.0f)
    CLOUDLAB_SET(multipleScatterTransmissionFloor, 0.0f, 1.0f)
    CLOUDLAB_SET(nightBrightness, 0.0f, 1.0f)
    CLOUDLAB_SET(noonReferenceBlend, 0.0f, 1.0f)
    CLOUDLAB_SET(finalNoonContrast, 0.0f, 1.0f)
    CLOUDLAB_SET(extinctionClear, 0.00001f, 0.005f)
    CLOUDLAB_SET(extinctionStorm, 0.00001f, 0.005f)
    CLOUDLAB_SET(bodyOpacityBoost, 0.25f, 4.0f)
    CLOUDLAB_SET(sunAbsorptionClear, 0.00001f, 0.005f)
    CLOUDLAB_SET(sunAbsorptionStorm, 0.00001f, 0.005f)
    CLOUDLAB_SET(noonShadowR, 0.0f, 2.0f)
    CLOUDLAB_SET(noonShadowG, 0.0f, 2.0f)
    CLOUDLAB_SET(noonShadowB, 0.0f, 2.0f)
    CLOUDLAB_SET(noonBodyR, 0.0f, 2.0f)
    CLOUDLAB_SET(noonBodyG, 0.0f, 2.0f)
    CLOUDLAB_SET(noonBodyB, 0.0f, 2.0f)
    CLOUDLAB_SET(noonHighlightR, 0.0f, 2.0f)
    CLOUDLAB_SET(noonHighlightG, 0.0f, 2.0f)
    CLOUDLAB_SET(noonHighlightB, 0.0f, 2.0f)
    CLOUDLAB_SET(macroScaleM, 500.0f, 200000.0f)
    CLOUDLAB_SET(mediumScaleM, 250.0f, 100000.0f)
    CLOUDLAB_SET(smallScaleM, 100.0f, 50000.0f)
    CLOUDLAB_SET(detailScaleM, 50.0f, 20000.0f)
    CLOUDLAB_SET(detailVerticalScaleM, 50.0f, 20000.0f)
    CLOUDLAB_SET(thresholdClear, 0.0f, 1.0f)
    CLOUDLAB_SET(thresholdCovered, 0.0f, 1.0f)
    CLOUDLAB_SET(stormThresholdBias, 0.0f, 0.5f)
    CLOUDLAB_SET(erosionInterior, 0.0f, 1.0f)
    CLOUDLAB_SET(erosionBoundary, 0.0f, 1.0f)
    CLOUDLAB_SET(densitySoftLow, -1.0f, 1.0f)
    CLOUDLAB_SET(densitySoftHigh, -1.0f, 1.0f)
    CLOUDLAB_SET(cellularStrength, 0.0f, 3.0f)
    CLOUDLAB_SET(towerStrength, 0.0f, 3.0f)
    CLOUDLAB_SET(verticalRoundness, 0.25f, 3.0f)
#undef CLOUDLAB_SET
    return false;
}

inline std::filesystem::path cloudLabProjectRoot()
{
    if (const char* explicitRoot = std::getenv("HERITAGE_PROJECT_ROOT"))
    {
        std::error_code envEc;
        const std::filesystem::path root(explicitRoot);
        if (!root.empty()
            && std::filesystem::exists(root / "Modules" / "RacingUnited", envEc))
            return root;
    }

    std::error_code ec;
    auto current = std::filesystem::current_path(ec);
    if (ec)
        return {};

    for (int i = 0; i < 10; ++i)
    {
        if (std::filesystem::exists(current / "Modules" / "RacingUnited", ec)
            && std::filesystem::exists(current / "UserData", ec))
            return current;
        if (!current.has_parent_path())
            break;
        const auto parent = current.parent_path();
        if (parent == current)
            break;
        current = parent;
    }
    return {};
}

inline std::filesystem::path cloudLabPresetPath()
{
    const auto root = cloudLabProjectRoot();
    if (!root.empty())
        return root / "UserData" / "CloudLabPreset.cfg";
    return std::filesystem::path("UserData") / "CloudLabPreset.cfg";
}

inline std::string trimCloudLabText(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline bool loadCloudLabSettingsFromDisk(CloudLabSettings& settings)
{
    const auto path = cloudLabPresetPath();
    std::ifstream input(path);
    if (!input)
        return false;

    CloudLabSettings loaded{};
    std::string line;
    bool sawKnownValue = false;
    while (std::getline(input, line))
    {
        line = trimCloudLabText(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        const auto equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        const std::string key = trimCloudLabText(line.substr(0, equals));
        const std::string rawValue = trimCloudLabText(line.substr(equals + 1));
        if (key == "version")
            continue;
        if (key == "shapeSolver")
        {
            char* end = nullptr;
            const long solver = std::strtol(rawValue.c_str(), &end, 10);
            if (end != rawValue.c_str())
            {
                loaded.shapeSolver = static_cast<int>(std::clamp(solver, 0L, 5L));
                sawKnownValue = true;
            }
            continue;
        }
        char* end = nullptr;
        const float value = std::strtof(rawValue.c_str(), &end);
        if (end == rawValue.c_str())
            continue;
        sawKnownValue = applyCloudLabValue(loaded, key, value) || sawKnownValue;
    }

    if (!sawKnownValue)
        return false;
    settings = loaded;
    return true;
}

inline CloudLabSettings& cloudLabSettings()
{
    static CloudLabSettings settings = [] {
        CloudLabSettings initial{};
        loadCloudLabSettingsFromDisk(initial);
        return initial;
    }();
    return settings;
}

inline void resetCloudLabSettings()
{
    cloudLabSettings() = CloudLabSettings{};
}

inline bool setCloudLabValue(std::string_view key, float value)
{
    return applyCloudLabValue(cloudLabSettings(), key, value);
}

inline bool setCloudLabSolver(int solver)
{
    if (solver < 0 || solver > 5)
        return false;
    cloudLabSettings().shapeSolver = solver;
    return true;
}

inline bool saveCloudLabSettings()
{
    const auto path = cloudLabPresetPath();
    std::error_code ec;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
        return false;

    std::ofstream output(path, std::ios::trunc);
    if (!output)
        return false;

    const auto& s = cloudLabSettings();
    output << "# Heritage Engine Cloud Lab preset\n";
    output << "# Human-readable and safe to include in a project ZIP.\n";
    output << "version=1\n";
    output << "shapeSolver=" << s.shapeSolver << "\n";
    output << std::setprecision(9) << std::fixed;
#define CLOUDLAB_WRITE(name) output << #name "=" << s.name << "\n";
    CLOUDLAB_WRITE(noonHorizonR); CLOUDLAB_WRITE(noonHorizonG); CLOUDLAB_WRITE(noonHorizonB);
    CLOUDLAB_WRITE(noonZenithR); CLOUDLAB_WRITE(noonZenithG); CLOUDLAB_WRITE(noonZenithB);
    CLOUDLAB_WRITE(noonSkyBlend); CLOUDLAB_WRITE(skyOvercastStart); CLOUDLAB_WRITE(skyOvercastEnd);
    CLOUDLAB_WRITE(stormDarkening); CLOUDLAB_WRITE(interiorDarkening);
    CLOUDLAB_WRITE(atmosphereTint); CLOUDLAB_WRITE(atmosphereChromaticTint);
    CLOUDLAB_WRITE(directSunStrength); CLOUDLAB_WRITE(silverEdgeStrength); CLOUDLAB_WRITE(thinBrightStrength);
    CLOUDLAB_WRITE(multipleScatterMin); CLOUDLAB_WRITE(multipleScatterMax); CLOUDLAB_WRITE(multipleScatterTransmissionFloor);
    CLOUDLAB_WRITE(nightBrightness); CLOUDLAB_WRITE(noonReferenceBlend); CLOUDLAB_WRITE(finalNoonContrast);
    CLOUDLAB_WRITE(extinctionClear); CLOUDLAB_WRITE(extinctionStorm); CLOUDLAB_WRITE(bodyOpacityBoost);
    CLOUDLAB_WRITE(sunAbsorptionClear); CLOUDLAB_WRITE(sunAbsorptionStorm);
    CLOUDLAB_WRITE(noonShadowR); CLOUDLAB_WRITE(noonShadowG); CLOUDLAB_WRITE(noonShadowB);
    CLOUDLAB_WRITE(noonBodyR); CLOUDLAB_WRITE(noonBodyG); CLOUDLAB_WRITE(noonBodyB);
    CLOUDLAB_WRITE(noonHighlightR); CLOUDLAB_WRITE(noonHighlightG); CLOUDLAB_WRITE(noonHighlightB);
    CLOUDLAB_WRITE(macroScaleM); CLOUDLAB_WRITE(mediumScaleM); CLOUDLAB_WRITE(smallScaleM);
    CLOUDLAB_WRITE(detailScaleM); CLOUDLAB_WRITE(detailVerticalScaleM);
    CLOUDLAB_WRITE(thresholdClear); CLOUDLAB_WRITE(thresholdCovered); CLOUDLAB_WRITE(stormThresholdBias);
    CLOUDLAB_WRITE(erosionInterior); CLOUDLAB_WRITE(erosionBoundary);
    CLOUDLAB_WRITE(densitySoftLow); CLOUDLAB_WRITE(densitySoftHigh);
    CLOUDLAB_WRITE(cellularStrength); CLOUDLAB_WRITE(towerStrength); CLOUDLAB_WRITE(verticalRoundness);
#undef CLOUDLAB_WRITE
    output.flush();
    return output.good();
}

inline bool reloadCloudLabSettings()
{
    CloudLabSettings loaded{};
    if (!loadCloudLabSettingsFromDisk(loaded))
        return false;
    cloudLabSettings() = loaded;
    return true;
}

inline bool deleteCloudLabPreset()
{
    std::error_code ec;
    const bool existed = std::filesystem::exists(cloudLabPresetPath(), ec);
    if (ec)
        return false;
    if (!existed)
        return true;
    return std::filesystem::remove(cloudLabPresetPath(), ec) && !ec;
}

inline bool cloudLabPresetExists()
{
    std::error_code ec;
    return std::filesystem::exists(cloudLabPresetPath(), ec) && !ec;
}

} // namespace heritage::physics::weather
