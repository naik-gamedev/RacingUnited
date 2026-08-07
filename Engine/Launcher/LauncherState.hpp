#pragma once

#include <filesystem>
#include <string>

namespace racing::launcher {

struct LauncherState
{
    std::string lastModuleId;
    bool settingsVisible = false;
};

class LauncherStateStorage
{
public:
    static bool load(const std::filesystem::path& path, LauncherState& state);
    static bool save(const std::filesystem::path& path, const LauncherState& state);
};

} // namespace racing::launcher
