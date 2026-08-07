#pragma once

#include <filesystem>
#include <string>

namespace heritage::modules {

struct ModuleInfo
{
    // Stable identifier used for command-line selection, settings and saves.
    std::string id;

    // Physical folder name under RacingUnited/Modules/.
    std::string folderName;

    std::string name;
    std::string version;
    std::string author;
    std::string description;

    // Runtime implementation requested by the module manifest.
    // Available now: "builtin_scene", "scripted_ui", and "lua".
    std::string runtime = "builtin_scene";

    std::string scene;
    std::string entryScript;
    std::string entryUi;
    std::string logoMesh;

    std::filesystem::path rootPath;
    bool valid = false;

    std::filesystem::path resolvePath(const std::filesystem::path& relativePath) const
    {
        return (rootPath / relativePath).lexically_normal();
    }
};

} // namespace heritage::modules
