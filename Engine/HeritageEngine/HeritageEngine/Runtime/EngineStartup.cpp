#include "EngineStartup.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <fstream>

#include "../../Core/Diagnostics/BuildIdentity.hpp"
#include "../../Core/Modules/ModuleContext.hpp"
#include "../../Core/Modules/ModuleInfo.hpp"

namespace fs = std::filesystem;

namespace heritage::engine::startup {

fs::path findProjectRoot()
{
#ifdef _WIN32
    char modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    fs::path location = length ? fs::path(modulePath).parent_path() : fs::current_path();
#else
    char modulePath[4096] = {};
    ssize_t length = readlink("/proc/self/exe", modulePath, sizeof(modulePath) - 1);
    fs::path location = length > 0 ? fs::path(std::string(modulePath, length)).parent_path() : fs::current_path();
#endif
    for (fs::path candidate = location; !candidate.empty(); candidate = candidate.parent_path())
    {
        if (fs::exists(candidate / "Modules") && fs::exists(candidate / "Assets"))
            return candidate.lexically_normal();
        if (candidate == candidate.parent_path()) break;
    }
    return fs::current_path().lexically_normal();
}

void writeLaunchDiagnostics(
    const fs::path& projectRoot,
    const fs::path& requestedModulePath,
    const std::string& requestedModuleId,
    const heritage::modules::ModuleInfo& activeModule,
    const heritage::modules::ModuleContext& moduleContext,
    const std::string& activeRuntimeId,
    const std::string& activeContentId)
{
    try
    {
        const fs::path diagnosticRoot = projectRoot / "UserData";
        fs::create_directories(diagnosticRoot);

        std::ofstream file(diagnosticRoot / "last_launch.txt", std::ios::trunc);
        if (!file)
            return;

        file << "build_identity="
            << heritage::diagnostics::buildIdentity() << '\n';
        file << "build_step="
            << heritage::diagnostics::generated::kMilestone << '\n';
        file << "build_configuration="
            << heritage::diagnostics::compiledConfiguration() << '\n';
        file << "git_commit="
            << heritage::diagnostics::generated::kGitCommit << '\n';
        file << "git_dirty="
            << heritage::diagnostics::generated::kGitDirty << '\n';
        file << "build_identity_generated_utc="
            << heritage::diagnostics::generated::kGeneratedUtc << '\n';
        file << "project_root=" << projectRoot.string() << '\n';
        file << "requested_module_id=" << requestedModuleId << '\n';
        file << "requested_module_path=" << requestedModulePath.string() << '\n';
        file << "loaded_module_id=" << activeModule.id << '\n';
        file << "loaded_module_folder=" << activeModule.folderName << '\n';
        file << "loaded_module_root=" << activeModule.rootPath.string() << '\n';
        file << "manifest_runtime=" << activeModule.runtime << '\n';
        file << "manifest_entry_scene=" << activeModule.scene << '\n';
        file << "manifest_entry_script=" << activeModule.entryScript << '\n';
        file << "manifest_entry_ui=" << activeModule.entryUi << '\n';
        file << "active_runtime=" << activeRuntimeId << '\n';
        file << "active_content=" << activeContentId << '\n';
        file << "module_assets_root=" << moduleContext.assetRoot().string() << '\n';
        file << "module_scripts_root=" << moduleContext.scriptsRoot().string() << '\n';
        file << "module_scenes_root=" << moduleContext.scenesRoot().string() << '\n';
        file << "module_data_root=" << moduleContext.dataRoot().string() << '\n';
        file << "module_ui_root=" << moduleContext.uiRoot().string() << '\n';
        file << "module_settings_root=" << moduleContext.settingsRoot().string() << '\n';
        file << "module_save_root=" << moduleContext.saveRoot().string() << '\n';
    }
    catch (...)
    {
    }
}

} // namespace heritage::engine::startup
