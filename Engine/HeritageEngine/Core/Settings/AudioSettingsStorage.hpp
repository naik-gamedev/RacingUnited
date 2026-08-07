#pragma once

#include <string>

#include "AudioSettings.hpp"

namespace heritage::settings {

class AudioSettingsStorage
{
public:
    static bool load(const std::string& path, AudioSettings& settings);
    static bool save(const std::string& path, const AudioSettings& settings);
};

} // namespace heritage::settings
