#include "LauncherState.hpp"

#include <fstream>
#include <string>

namespace racing::launcher {
namespace {

std::string trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

bool LauncherStateStorage::load(
    const std::filesystem::path& path,
    LauncherState& state)
{
    std::ifstream file(path);
    if (!file)
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        line = trim(line);
        if (line.empty() || line.front() == '#' || line.front() == ';')
            continue;

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;

        const std::string key = trim(line.substr(0, equals));
        const std::string value = trim(line.substr(equals + 1));

        if (key == "lastModuleId")
            state.lastModuleId = value;
        else if (key == "settingsVisible")
            state.settingsVisible = (value == "1" || value == "true" || value == "on");
    }

    return true;
}

bool LauncherStateStorage::save(
    const std::filesystem::path& path,
    const LauncherState& state)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);

    std::ofstream file(path, std::ios::trunc);
    if (!file)
        return false;

    file << "lastModuleId=" << state.lastModuleId << '\n';
    file << "settingsVisible=" << (state.settingsVisible ? 1 : 0) << '\n';
    return static_cast<bool>(file);
}

} // namespace racing::launcher
