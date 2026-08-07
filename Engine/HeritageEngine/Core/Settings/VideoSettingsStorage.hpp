#pragma once

#include <string>

#include "VideoSettings.hpp"

namespace heritage::settings {

class VideoSettingsStorage
{
public:
    static bool load(const std::string& path, VideoSettings& settings);
    static bool save(const std::string& path, const VideoSettings& settings);
};

} // namespace heritage::settings
