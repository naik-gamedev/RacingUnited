#include "ModuleLoader.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>

namespace heritage::modules {
namespace {

std::string trim(std::string value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::map<std::string, std::string> parseIni(const std::filesystem::path& path)
{
    std::map<std::string, std::string> values;
    std::ifstream file(path);
    if (!file)
        return values;

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
        if (!key.empty())
            values[key] = value;
    }

    return values;
}

std::string valueOr(
    const std::map<std::string, std::string>& values,
    const std::string& key,
    const std::string& fallback)
{
    const auto found = values.find(key);
    return found == values.end() ? fallback : found->second;
}

std::string commandLineValue(
    int argc,
    char** argv,
    const std::string& option)
{
    const std::string prefix = option + "=";

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index] ? argv[index] : "";

        if (argument == option && index + 1 < argc)
            return argv[index + 1] ? argv[index + 1] : "";

        if (argument.rfind(prefix, 0) == 0)
            return argument.substr(prefix.size());
    }

    return {};
}

bool isSafeId(const std::string& moduleId)
{
    if (moduleId.empty() || moduleId == "." || moduleId == "..")
        return false;

    return std::all_of(
        moduleId.begin(),
        moduleId.end(),
        [](unsigned char character)
        {
            return std::isalnum(character)
                || character == '_'
                || character == '-';
        });
}

std::filesystem::path normalizedAbsolute(
    const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path result = std::filesystem::weakly_canonical(path, error);
    if (!error)
        return result.lexically_normal();

    result = std::filesystem::absolute(path, error);
    return error ? path.lexically_normal() : result.lexically_normal();
}

ModuleInfo loadFromDirectory(
    const std::filesystem::path& moduleRoot,
    std::vector<std::string>* warnings)
{
    ModuleInfo module;
    module.folderName = moduleRoot.filename().string();
    module.rootPath = normalizedAbsolute(moduleRoot);

    const std::filesystem::path manifestPath = module.rootPath / "module.ini";
    if (!std::filesystem::is_regular_file(manifestPath))
    {
        if (warnings)
        {
            warnings->push_back(
                "Module folder '" + module.folderName + "' has no module.ini file.");
        }
        return module;
    }

    const auto values = parseIni(manifestPath);
    if (values.empty())
    {
        if (warnings)
        {
            warnings->push_back(
                "Could not read manifest: " + manifestPath.string());
        }
        return module;
    }

    const auto idFound = values.find("id");
    if (idFound == values.end() || trim(idFound->second).empty())
    {
        module.id = module.folderName;
        if (warnings)
        {
            warnings->push_back(
                "Module '" + module.folderName
                + "' has no stable id; the folder name is being used for compatibility.");
        }
    }
    else
    {
        module.id = trim(idFound->second);
    }

    if (!isSafeId(module.id))
    {
        if (warnings)
        {
            warnings->push_back(
                "Module '" + module.folderName + "' has an invalid id: " + module.id);
        }
        module.id.clear();
        return module;
    }

    module.name = valueOr(values, "name", module.folderName);
    module.version = valueOr(values, "version", "?");
    module.author = valueOr(values, "author", "Unknown");
    module.description = valueOr(values, "description", "");
    module.runtime = valueOr(values, "runtime", "builtin_scene");

    // Deliberately no logo scene fallback. A module without entry_scene loads
    // the engine's empty black scene so manifest mistakes stay visible.
    module.scene = valueOr(values, "entry_scene", valueOr(values, "scene", ""));
    module.entryScript = valueOr(values, "entry_script", "");
    module.entryUi = valueOr(values, "entry_ui", "");
    module.logoMesh = valueOr(values, "logo_mesh", "");

    if (module.runtime.empty())
        module.runtime = "builtin_scene";

    if (warnings)
    {
        if (module.runtime == "builtin_scene" && module.scene.empty())
        {
            warnings->push_back(
                "Module '" + module.id
                + "' uses builtin_scene but has no entry_scene; Heritage Engine will display an empty black scene.");
        }
        else if (module.runtime == "scripted_ui" && module.entryUi.empty())
        {
            warnings->push_back(
                "Module '" + module.id
                + "' uses scripted_ui but has no entry_ui.");
        }
        else if (module.runtime == "lua" && module.entryScript.empty())
        {
            warnings->push_back(
                "Module '" + module.id
                + "' uses lua but has no entry_script.");
        }
    }

    module.valid = true;
    return module;
}

} // namespace

std::string ModuleLoader::requestedModuleId(
    int argc,
    char** argv,
    const std::string& fallbackId)
{
    const std::string requested = commandLineValue(argc, argv, "--module");
    if (!requested.empty())
        return requested;

    // Compatibility with the original launcher, which passed only the folder name.
    if (argc >= 2 && argv[1])
    {
        const std::string firstArgument = argv[1];
        if (!firstArgument.empty() && firstArgument.front() != '-')
            return firstArgument;
    }

    return fallbackId;
}

std::filesystem::path ModuleLoader::requestedProjectRoot(int argc, char** argv)
{
    const std::string value = commandLineValue(argc, argv, "--project-root");
    return value.empty() ? std::filesystem::path{} : normalizedAbsolute(value);
}

std::filesystem::path ModuleLoader::requestedModulePath(int argc, char** argv)
{
    const std::string value = commandLineValue(argc, argv, "--module-path");
    return value.empty() ? std::filesystem::path{} : normalizedAbsolute(value);
}

bool ModuleLoader::isSafeModuleId(const std::string& moduleId)
{
    return isSafeId(moduleId);
}

ModuleInfo ModuleLoader::loadFromPath(
    const std::filesystem::path& projectRoot,
    const std::filesystem::path& moduleRoot)
{
    if (projectRoot.empty() || moduleRoot.empty())
        return {};

    const std::filesystem::path normalizedProjectRoot = normalizedAbsolute(projectRoot);
    const std::filesystem::path modulesRoot = normalizedAbsolute(normalizedProjectRoot / "Modules");
    const std::filesystem::path normalizedModuleRoot = normalizedAbsolute(moduleRoot);

    // Modules are direct children of RacingUnited/Modules. Requiring this exact
    // relationship prevents a launcher argument from escaping into arbitrary folders.
    if (normalizedModuleRoot.parent_path() != modulesRoot)
        return {};

    return loadFromDirectory(normalizedModuleRoot, nullptr);
}

ModuleInfo ModuleLoader::load(
    const std::filesystem::path& projectRoot,
    const std::string& moduleIdOrFolder)
{
    if (!isSafeModuleId(moduleIdOrFolder))
        return {};

    const std::filesystem::path modulesRoot = projectRoot / "Modules";

    // Fast path: the command-line value matches the folder name.
    const std::filesystem::path directRoot = modulesRoot / moduleIdOrFolder;
    if (std::filesystem::is_directory(directRoot))
    {
        ModuleInfo direct = loadFromDirectory(directRoot, nullptr);
        if (direct.valid
            && (direct.id == moduleIdOrFolder || direct.folderName == moduleIdOrFolder))
        {
            return direct;
        }
    }

    // Stable ids do not have to match folder names.
    const ModuleScanResult result = scanDetailed(projectRoot);
    const auto found = std::find_if(
        result.modules.begin(),
        result.modules.end(),
        [&](const ModuleInfo& module)
        {
            return module.id == moduleIdOrFolder
                || module.folderName == moduleIdOrFolder;
        });

    return found == result.modules.end() ? ModuleInfo{} : *found;
}

std::vector<ModuleInfo> ModuleLoader::scan(
    const std::filesystem::path& projectRoot)
{
    return scanDetailed(projectRoot).modules;
}

ModuleScanResult ModuleLoader::scanDetailed(
    const std::filesystem::path& projectRoot)
{
    ModuleScanResult result;
    const std::filesystem::path modulesRoot = projectRoot / "Modules";

    std::error_code error;
    if (!std::filesystem::is_directory(modulesRoot, error))
    {
        result.warnings.push_back(
            "Modules folder was not found: " + modulesRoot.string());
        return result;
    }

    std::set<std::string> ids;
    for (const auto& entry : std::filesystem::directory_iterator(modulesRoot, error))
    {
        if (error)
        {
            result.warnings.push_back(
                "Could not scan Modules folder: " + error.message());
            break;
        }
        if (!entry.is_directory())
            continue;

        ModuleInfo module = loadFromDirectory(entry.path(), &result.warnings);
        if (!module.valid)
            continue;

        if (!ids.insert(module.id).second)
        {
            result.warnings.push_back(
                "Duplicate module id '" + module.id + "' in folder '"
                + module.folderName + "'. The duplicate was ignored.");
            continue;
        }

        result.modules.push_back(std::move(module));
    }

    std::sort(
        result.modules.begin(),
        result.modules.end(),
        [](const ModuleInfo& left, const ModuleInfo& right)
        {
            if (left.name != right.name)
                return left.name < right.name;
            return left.id < right.id;
        });

    return result;
}

std::filesystem::path ModuleLoader::userDataRoot(
    const std::filesystem::path& projectRoot,
    const ModuleInfo& module)
{
    const std::string safeId = isSafeModuleId(module.id) ? module.id : "Default";
    return (projectRoot / "UserData" / "Modules" / safeId).lexically_normal();
}

} // namespace heritage::modules
