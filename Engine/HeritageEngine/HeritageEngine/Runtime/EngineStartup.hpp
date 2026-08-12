#pragma once

#include <filesystem>
#include <string>

namespace heritage::modules {
struct ModuleInfo;
class ModuleContext;
}

namespace heritage::engine::startup {

std::filesystem::path findProjectRoot();

void writeLaunchDiagnostics(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& requestedModulePath,
    const std::string& requestedModuleId,
    const heritage::modules::ModuleInfo& activeModule,
    const heritage::modules::ModuleContext& moduleContext,
    const std::string& activeRuntimeId,
    const std::string& activeContentId);

} // namespace heritage::engine::startup
