#include "ModuleContext.hpp"

#include "ModuleLoader.hpp"

#include <system_error>

namespace heritage::modules {

ModuleContext::ModuleContext(
    const std::filesystem::path& projectRoot,
    const ModuleInfo& module)
    : m_module(module),
      m_projectRoot(projectRoot.lexically_normal()),
      m_moduleRoot(module.rootPath.lexically_normal()),
      m_assetRoot((module.rootPath / "Assets").lexically_normal()),
      m_scriptsRoot((module.rootPath / "Scripts").lexically_normal()),
      m_scenesRoot((module.rootPath / "Scenes").lexically_normal()),
      m_prefabsRoot((module.rootPath / "Prefabs").lexically_normal()),
      m_dataRoot((module.rootPath / "Data").lexically_normal()),
      m_uiRoot((module.rootPath / "UI").lexically_normal()),
      m_settingsRoot(ModuleLoader::userDataRoot(projectRoot, module)),
      m_saveRoot((m_settingsRoot / "Saves").lexically_normal())
{
}

bool ModuleContext::prepareUserDirectories(std::string& errorMessage) const
{
    std::error_code error;
    std::filesystem::create_directories(m_settingsRoot, error);
    if (error)
    {
        errorMessage = "Could not create module settings directory: "
            + m_settingsRoot.string() + " (" + error.message() + ")";
        return false;
    }

    std::filesystem::create_directories(m_saveRoot, error);
    if (error)
    {
        errorMessage = "Could not create module save directory: "
            + m_saveRoot.string() + " (" + error.message() + ")";
        return false;
    }

    errorMessage.clear();
    return true;
}

std::filesystem::path ModuleContext::resolveModulePath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_moduleRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveAssetPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_assetRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveScriptPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_scriptsRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveScenePath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_scenesRoot, relativePath);
}

std::filesystem::path ModuleContext::resolvePrefabPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_prefabsRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveDataPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_dataRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveUiPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_uiRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveSettingsPath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_settingsRoot, relativePath);
}

std::filesystem::path ModuleContext::resolveSavePath(
    const std::filesystem::path& relativePath) const
{
    return safeJoin(m_saveRoot, relativePath);
}

bool ModuleContext::isSafeRelativePath(const std::filesystem::path& path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name())
        return false;

    const std::filesystem::path normalized = path.lexically_normal();
    for (const auto& component : normalized)
    {
        if (component == "..")
            return false;
    }

    return true;
}

std::filesystem::path ModuleContext::safeJoin(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath)
{
    if (!isSafeRelativePath(relativePath))
        return {};

    return (root / relativePath.lexically_normal()).lexically_normal();
}

} // namespace heritage::modules
