#include "VideoSettingsStorage.hpp"

#include <algorithm>
#include <fstream>
#include <string>

namespace heritage::settings {
namespace {

bool parseInt(const std::string& value, int& output)
{
    try
    {
        std::size_t consumed = 0;
        const int parsed = std::stoi(value, &consumed);
        if (consumed != value.size())
            return false;
        output = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool parseFloat(const std::string& value, float& output)
{
    try
    {
        std::size_t consumed = 0;
        const float parsed = std::stof(value, &consumed);
        if (consumed != value.size())
            return false;
        output = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void sanitize(VideoSettings& settings)
{
    settings.antiAliasingIndex = std::clamp(settings.antiAliasingIndex, 0, 6);
    settings.textureFilterIndex = std::clamp(settings.textureFilterIndex, 0, 6);
    settings.shadowQualityIndex = std::clamp(settings.shadowQualityIndex, 0, 3);
    settings.shadowFilterIndex = std::clamp(settings.shadowFilterIndex, 0, 2);
    settings.scaleModeIndex = std::clamp(settings.scaleModeIndex, 0, 5);
    settings.fpsCapIndex = std::clamp(
        settings.fpsCapIndex,
        0,
        static_cast<int>(kFpsCapValues.size()) - 1);
    settings.renderApiIndex = 0; // OpenGL is currently the only backend.
    settings.windowModeIndex = std::clamp(settings.windowModeIndex, 0, 2);

    settings.resolutionWidth = std::max(settings.resolutionWidth, 0);
    settings.resolutionHeight = std::max(settings.resolutionHeight, 0);
    settings.refreshRate = std::max(settings.refreshRate, 0);

    settings.gamma = std::clamp(settings.gamma, 1.00f, 3.00f);
    settings.brightness = std::clamp(settings.brightness, -0.50f, 0.50f);
    settings.contrast = std::clamp(settings.contrast, 0.50f, 1.50f);
    settings.saturation = std::clamp(settings.saturation, 0.00f, 2.00f);

    // A custom-framed Heritage Engine window this small is effectively unusable:
    // the title bar and debug UI can consume most of the client area. Treat a
    // tiny remembered rectangle as damaged/stale placement and recover to the
    // normal centered startup size instead of faithfully reopening a postage stamp.
    if (settings.windowWidth < kMinimumInteractiveWindowWidth
        || settings.windowHeight < kMinimumInteractiveWindowHeight)
    {
        settings.windowPlacementValid = false;
        settings.windowWidth = kDefaultWindowWidth;
        settings.windowHeight = kDefaultWindowHeight;
    }
}

} // namespace

bool VideoSettingsStorage::load(const std::string& path, VideoSettings& settings)
{
    std::ifstream file(path);
    if (!file)
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = line.substr(0, equals);
        const std::string value = line.substr(equals + 1);

        if (key == "antiAliasingIndex") parseInt(value, settings.antiAliasingIndex);
        else if (key == "textureFilterIndex") parseInt(value, settings.textureFilterIndex);
        else if (key == "shadowQualityIndex") parseInt(value, settings.shadowQualityIndex);
        else if (key == "shadowFilterIndex") parseInt(value, settings.shadowFilterIndex);
        else if (key == "scaleModeIndex") parseInt(value, settings.scaleModeIndex);
        else if (key == "fpsCapIndex") parseInt(value, settings.fpsCapIndex);
        else if (key == "vsyncEnabled") settings.vsyncEnabled = (value == "1" || value == "true");
        else if (key == "renderApiIndex") parseInt(value, settings.renderApiIndex);
        else if (key == "windowModeIndex") parseInt(value, settings.windowModeIndex);
        else if (key == "resolutionWidth") parseInt(value, settings.resolutionWidth);
        else if (key == "resolutionHeight") parseInt(value, settings.resolutionHeight);
        else if (key == "refreshRate") parseInt(value, settings.refreshRate);
        else if (key == "gamma") parseFloat(value, settings.gamma);
        else if (key == "brightness") parseFloat(value, settings.brightness);
        else if (key == "contrast") parseFloat(value, settings.contrast);
        else if (key == "saturation") parseFloat(value, settings.saturation);
        else if (key == "windowPlacementValid") settings.windowPlacementValid = (value == "1");
        else if (key == "windowX") parseInt(value, settings.windowX);
        else if (key == "windowY") parseInt(value, settings.windowY);
        else if (key == "windowWidth") parseInt(value, settings.windowWidth);
        else if (key == "windowHeight") parseInt(value, settings.windowHeight);
    }

    sanitize(settings);
    return true;
}

bool VideoSettingsStorage::save(const std::string& path, const VideoSettings& settings)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
        return false;

    file << "version=5\n";
    file << "antiAliasingIndex=" << settings.antiAliasingIndex << '\n';
    file << "textureFilterIndex=" << settings.textureFilterIndex << '\n';
    file << "shadowQualityIndex=" << settings.shadowQualityIndex << '\n';
    file << "shadowFilterIndex=" << settings.shadowFilterIndex << '\n';
    file << "scaleModeIndex=" << settings.scaleModeIndex << '\n';
    file << "fpsCapIndex=" << settings.fpsCapIndex << '\n';
    file << "vsyncEnabled=" << (settings.vsyncEnabled ? 1 : 0) << '\n';
    file << "renderApiIndex=" << settings.renderApiIndex << '\n';
    file << "windowModeIndex=" << settings.windowModeIndex << '\n';
    file << "resolutionWidth=" << settings.resolutionWidth << '\n';
    file << "resolutionHeight=" << settings.resolutionHeight << '\n';
    file << "refreshRate=" << settings.refreshRate << '\n';
    file << "gamma=" << settings.gamma << '\n';
    file << "brightness=" << settings.brightness << '\n';
    file << "contrast=" << settings.contrast << '\n';
    file << "saturation=" << settings.saturation << '\n';
    file << "windowPlacementValid=" << (settings.windowPlacementValid ? 1 : 0) << '\n';
    file << "windowX=" << settings.windowX << '\n';
    file << "windowY=" << settings.windowY << '\n';
    file << "windowWidth=" << settings.windowWidth << '\n';
    file << "windowHeight=" << settings.windowHeight << '\n';

    return static_cast<bool>(file);
}

} // namespace heritage::settings
