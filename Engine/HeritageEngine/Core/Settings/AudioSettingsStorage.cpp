#include "AudioSettingsStorage.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

namespace heritage::settings {
namespace {

std::string trim(const std::string& value)
{
    const std::size_t begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
        return {};

    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

float clampVolume(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool parseBool(const std::string& value)
{
    return value == "1" || value == "true" || value == "True" || value == "on";
}

} // namespace

bool AudioSettingsStorage::load(
    const std::string& path,
    AudioSettings& settings)
{
    std::ifstream file(path);
    if (!file)
        return false;

    AudioSettings loaded = settings;
    std::string line;
    while (std::getline(file, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, separator));
        const std::string value = trim(line.substr(separator + 1));

        try
        {
            if (key == "master") loaded.masterVolume = clampVolume(std::stof(value));
            else if (key == "music") loaded.musicVolume = clampVolume(std::stof(value));
            else if (key == "effects") loaded.effectsVolume = clampVolume(std::stof(value));
            else if (key == "ambience") loaded.ambienceVolume = clampVolume(std::stof(value));
            else if (key == "ui") loaded.uiVolume = clampVolume(std::stof(value));
            else if (key == "voice") loaded.voiceVolume = clampVolume(std::stof(value));
            else if (key == "mute_when_unfocused") loaded.muteWhenUnfocused = parseBool(value);
        }
        catch (...)
        {
            // Ignore malformed individual entries and preserve the previous
            // valid value instead of rejecting the entire settings file.
        }
    }

    settings = loaded;
    return true;
}

bool AudioSettingsStorage::save(
    const std::string& path,
    const AudioSettings& settings)
{
    try
    {
        const std::filesystem::path filePath(path);
        if (!filePath.parent_path().empty())
            std::filesystem::create_directories(filePath.parent_path());

        std::ofstream file(filePath, std::ios::trunc);
        if (!file)
            return false;

        file << std::fixed << std::setprecision(4);
        file << "master=" << clampVolume(settings.masterVolume) << '\n';
        file << "music=" << clampVolume(settings.musicVolume) << '\n';
        file << "effects=" << clampVolume(settings.effectsVolume) << '\n';
        file << "ambience=" << clampVolume(settings.ambienceVolume) << '\n';
        file << "ui=" << clampVolume(settings.uiVolume) << '\n';
        file << "voice=" << clampVolume(settings.voiceVolume) << '\n';
        file << "mute_when_unfocused=" << (settings.muteWhenUnfocused ? 1 : 0) << '\n';
        return static_cast<bool>(file);
    }
    catch (...)
    {
        return false;
    }
}

} // namespace heritage::settings
