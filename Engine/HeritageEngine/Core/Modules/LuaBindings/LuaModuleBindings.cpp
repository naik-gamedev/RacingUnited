#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../../Paths/Utf8Path.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <Windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")
#endif

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaCoreBindingHandlers::luaModuleId(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().id : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaCoreBindingHandlers::luaModuleName(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().name : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaCoreBindingHandlers::luaModuleVersion(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    const std::string value = (runtime && runtime->m_context)
        ? runtime->m_context->module().version : std::string{};
    if (runtime)
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    return runtime ? 1 : 0;
}

int LuaCoreBindingHandlers::luaModuleAssetPath(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveAssetPath(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaCoreBindingHandlers::luaModuleAssetExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::filesystem::path path = runtime->m_context->resolveAssetPath(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    std::error_code error;
    const bool exists = !path.empty()
        && std::filesystem::is_regular_file(path, error)
        && !error;
    runtime->m_api.lua_pushboolean(state, exists ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaModuleGetAssetIndexRevision(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_assetRegistry.revision()));
    return 1;
}

int LuaCoreBindingHandlers::luaModuleGetAssetCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string extension = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string directoryPrefix = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string fileNamePrefix = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_assetRegistry.count(
            extension, directoryPrefix, fileNamePrefix)));
    return 1;
}

int LuaCoreBindingHandlers::luaModuleGetAssetPath(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const double rawIndex = LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0);
    const std::size_t oneBasedIndex = rawIndex > 0.0
        ? static_cast<std::size_t>(rawIndex)
        : 0;
    const std::string extension = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string directoryPrefix = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    const std::string fileNamePrefix = LuaModuleRuntime::stringArgument(*runtime, state, 4);

    const auto record = runtime->m_assetRegistry.recordAt(
        oneBasedIndex, extension, directoryPrefix, fileNamePrefix);
    if (!record)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushlstring(
        state,
        record->relativePath.c_str(),
        record->relativePath.size());
    return 1;
}

int LuaCoreBindingHandlers::luaModuleGetLatestAsset(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string extension = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string directoryPrefix = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::string fileNamePrefix = LuaModuleRuntime::stringArgument(*runtime, state, 3);
    const auto record = runtime->m_assetRegistry.latest(
        extension, directoryPrefix, fileNamePrefix);
    if (!record)
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushlstring(
        state,
        record->relativePath.c_str(),
        record->relativePath.size());
    return 1;
}

int LuaCoreBindingHandlers::luaModuleRefreshAssetIndex(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;
    runtime->m_assetRegistry.forceRefresh();
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_assetRegistry.revision()));
    return 1;
}

int LuaCoreBindingHandlers::luaModuleSelectAssetFile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

#if defined(_WIN32)
    std::vector<wchar_t> selected(32768, L'\0');
    const std::wstring initialDirectory =
        runtime->m_context->assetRoot().wstring();
    const wchar_t filter[] =
        L"Vehicle assets (*.obj;*.glb)\0*.obj;*.glb\0glTF Binary (*.glb)\0*.glb\0Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = selected.data();
    dialog.nMaxFile = static_cast<DWORD>(selected.size());
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.lpstrTitle = L"Select a module-owned vehicle OBJ or GLB";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST
        | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT;

    if (!GetOpenFileNameW(&dialog))
    {
        runtime->m_api.lua_pushnil(state);
        const std::string message = CommDlgExtendedError() == 0
            ? "Asset selection cancelled."
            : "The Windows asset picker could not complete.";
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(
        runtime->m_context->assetRoot(), error);
    if (error)
    {
        const std::string message = "The module Assets directory is unavailable.";
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }
    const std::filesystem::path absolute = std::filesystem::weakly_canonical(
        std::filesystem::path(selected.data()), error);
    const std::filesystem::path relative = error
        ? std::filesystem::path{}
        : absolute.lexically_relative(root);
    bool safe = !relative.empty() && !relative.is_absolute();
    for (const std::filesystem::path& component : relative)
    {
        if (component == "..")
            safe = false;
    }
    std::string extension = absolute.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    safe = safe && (extension == ".obj" || extension == ".glb")
        && std::filesystem::is_regular_file(absolute, error) && !error;
    if (!safe)
    {
        const std::string message =
            "Vehicle assets must be .obj or .glb files already inside this module's Assets folder.";
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::string value = relative.generic_string();
    runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    runtime->m_api.lua_pushnil(state);
    return 2;
#else
    const std::string message =
        "The native asset picker is currently implemented for Windows only.";
    runtime->m_api.lua_pushnil(state);
    runtime->m_api.lua_pushlstring(state, message.c_str(), message.size());
    return 2;
#endif
}

int LuaCoreBindingHandlers::luaModuleDataPath(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveDataPath(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaCoreBindingHandlers::luaModuleSavePath(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const auto path = runtime->m_context->resolveSavePath(
        LuaModuleRuntime::stringArgument(*runtime, state, 1));
    if (path.empty())
        runtime->m_api.lua_pushnil(state);
    else
    {
        const std::string value = path.string();
        runtime->m_api.lua_pushlstring(state, value.c_str(), value.size());
    }
    return 1;
}

int LuaCoreBindingHandlers::luaModuleWriteSaveText(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativeText = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string contents = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::filesystem::path relative = heritage::paths::fromUtf8(relativeText);
    std::string extension = relative.extension().string();
    std::transform(
        extension.begin(), extension.end(), extension.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    const bool supportedExtension = extension == ".lua"
        || extension == ".json" || extension == ".txt";
    const std::filesystem::path path = runtime->m_context->resolveSavePath(relative);
    if (path.empty() || !supportedExtension || contents.size() > 2u * 1024u * 1024u)
    {
        const std::string message =
            "Module.WriteSaveText requires a safe .lua, .json or .txt save-relative path and at most 2 MiB of text.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        const std::string message =
            "Could not create the module save directory: " + error.message();
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output)
    {
        const std::string message = "Could not write the module save text file.";
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushlstring(
            state, message.c_str(), message.size());
        return 2;
    }

    const std::string result = path.string();
    runtime->m_api.lua_pushboolean(state, 1);
    runtime->m_api.lua_pushlstring(state, result.c_str(), result.size());
    return 2;
}

void LuaModuleRuntime::registerModuleBindings()
{
    registerFunction("Module", "Id", &LuaCoreBindingHandlers::luaModuleId);
    registerFunction("Module", "Name", &LuaCoreBindingHandlers::luaModuleName);
    registerFunction("Module", "Version", &LuaCoreBindingHandlers::luaModuleVersion);
    registerFunction("Module", "AssetPath", &LuaCoreBindingHandlers::luaModuleAssetPath);
    registerFunction("Module", "AssetExists", &LuaCoreBindingHandlers::luaModuleAssetExists);
    registerFunction("Module", "GetAssetIndexRevision", &LuaCoreBindingHandlers::luaModuleGetAssetIndexRevision);
    registerFunction("Module", "GetAssetCount", &LuaCoreBindingHandlers::luaModuleGetAssetCount);
    registerFunction("Module", "GetAssetPath", &LuaCoreBindingHandlers::luaModuleGetAssetPath);
    registerFunction("Module", "GetLatestAsset", &LuaCoreBindingHandlers::luaModuleGetLatestAsset);
    registerFunction("Module", "RefreshAssetIndex", &LuaCoreBindingHandlers::luaModuleRefreshAssetIndex);
    registerFunction("Module", "SelectAssetFile", &LuaCoreBindingHandlers::luaModuleSelectAssetFile);
    registerFunction("Module", "DataPath", &LuaCoreBindingHandlers::luaModuleDataPath);
    registerFunction("Module", "SavePath", &LuaCoreBindingHandlers::luaModuleSavePath);
    registerFunction("Module", "WriteSaveText", &LuaCoreBindingHandlers::luaModuleWriteSaveText);
}

} // namespace heritage::modules
