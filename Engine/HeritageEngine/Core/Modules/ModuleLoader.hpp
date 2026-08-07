#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "ModuleInfo.hpp"

namespace heritage::modules {

struct ModuleScanResult
{
    std::vector<ModuleInfo> modules;
    std::vector<std::string> warnings;
};

class ModuleLoader
{
public:
    static std::string requestedModuleId(
        int argc,
        char** argv,
        const std::string& fallbackId = "");

    static std::filesystem::path requestedProjectRoot(int argc, char** argv);
    static std::filesystem::path requestedModulePath(int argc, char** argv);

    // Accepts either the stable manifest id or the physical folder name.
    static ModuleInfo load(
        const std::filesystem::path& projectRoot,
        const std::string& moduleIdOrFolder);

    // Loads one exact module folder. This is the preferred launcher-to-engine
    // route because it cannot accidentally resolve a different module.
    static ModuleInfo loadFromPath(
        const std::filesystem::path& projectRoot,
        const std::filesystem::path& moduleRoot);

    static std::vector<ModuleInfo> scan(const std::filesystem::path& projectRoot);
    static ModuleScanResult scanDetailed(const std::filesystem::path& projectRoot);

    static std::filesystem::path userDataRoot(
        const std::filesystem::path& projectRoot,
        const ModuleInfo& module);

private:
    static bool isSafeModuleId(const std::string& moduleId);
};

} // namespace heritage::modules
