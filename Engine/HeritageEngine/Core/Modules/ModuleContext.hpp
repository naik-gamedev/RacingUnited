#pragma once

#include <filesystem>
#include <string>

#include "ModuleInfo.hpp"

namespace heritage::modules {

// Runtime-owned paths for one active module. Module content, settings and saves
// are resolved through this object so modules cannot silently borrow files from
// another module or write outside their own persistent-data directory.
class ModuleContext
{
public:
    ModuleContext(
        const std::filesystem::path& projectRoot,
        const ModuleInfo& module);

    const ModuleInfo& module() const { return m_module; }
    const std::filesystem::path& projectRoot() const { return m_projectRoot; }
    const std::filesystem::path& moduleRoot() const { return m_moduleRoot; }
    const std::filesystem::path& assetRoot() const { return m_assetRoot; }
    const std::filesystem::path& scriptsRoot() const { return m_scriptsRoot; }
    const std::filesystem::path& scenesRoot() const { return m_scenesRoot; }
    const std::filesystem::path& prefabsRoot() const { return m_prefabsRoot; }
    const std::filesystem::path& dataRoot() const { return m_dataRoot; }
    const std::filesystem::path& uiRoot() const { return m_uiRoot; }
    const std::filesystem::path& settingsRoot() const { return m_settingsRoot; }
    const std::filesystem::path& saveRoot() const { return m_saveRoot; }

    bool prepareUserDirectories(std::string& errorMessage) const;

    // For manifest paths such as "Assets/RacingUnited_3D_Logo.obj".
    std::filesystem::path resolveModulePath(
        const std::filesystem::path& relativePath) const;

    // For paths relative to their dedicated module folders.
    std::filesystem::path resolveAssetPath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolveScriptPath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolveScenePath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolvePrefabPath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolveDataPath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolveUiPath(
        const std::filesystem::path& relativePath) const;

    std::filesystem::path resolveSettingsPath(
        const std::filesystem::path& relativePath) const;
    std::filesystem::path resolveSavePath(
        const std::filesystem::path& relativePath) const;

private:
    static bool isSafeRelativePath(const std::filesystem::path& path);
    static std::filesystem::path safeJoin(
        const std::filesystem::path& root,
        const std::filesystem::path& relativePath);

    ModuleInfo m_module;
    std::filesystem::path m_projectRoot;
    std::filesystem::path m_moduleRoot;
    std::filesystem::path m_assetRoot;
    std::filesystem::path m_scriptsRoot;
    std::filesystem::path m_scenesRoot;
    std::filesystem::path m_prefabsRoot;
    std::filesystem::path m_dataRoot;
    std::filesystem::path m_uiRoot;
    std::filesystem::path m_settingsRoot;
    std::filesystem::path m_saveRoot;
};

} // namespace heritage::modules
