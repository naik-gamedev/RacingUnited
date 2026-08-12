#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"
#include "../../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include "../../../Audio/AudioSystem.hpp"
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaCoreBindingHandlers::luaAudioIsAvailable(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const bool available = runtime->m_audio && runtime->m_audio->isAvailable();
    runtime->m_api.lua_pushboolean(state, available ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetBackend(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string backend = runtime->m_audio
        ? runtime->m_audio->backendName()
        : "No audio service";
    runtime->m_api.lua_pushlstring(state, backend.c_str(), backend.size());
    return 1;
}

int LuaCoreBindingHandlers::luaAudioPlaySound(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_context || !runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string busName = LuaModuleRuntime::stringArgument(*runtime, state, 2, "Effects");
    const auto bus = heritage::audio::parseAudioBus(busName);
    const float volume = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));
    const float pitch = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0));
    const std::filesystem::path path = runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));

    if (path.empty())
    {
        runtime->m_lastAudioError = "Unsafe module audio path: " + relativePath;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::audio::AudioHandle handle = runtime->m_audio->playOneShot(
        path, *bus, volume, pitch);
    if (handle == heritage::audio::kInvalidAudioHandle)
    {
        runtime->m_lastAudioError = runtime->m_audio->lastError();
    }
    else
    {
        runtime->m_audioHandles.insert(handle);
        runtime->m_lastAudioError.clear();
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioPlayLoop(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_context || !runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string busName = LuaModuleRuntime::stringArgument(*runtime, state, 2, "Ambience");
    const auto bus = heritage::audio::parseAudioBus(busName);
    const float volume = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));
    const float pitch = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0));
    const std::filesystem::path path = runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));

    if (path.empty())
    {
        runtime->m_lastAudioError = "Unsafe module audio path: " + relativePath;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::audio::AudioHandle handle = runtime->m_audio->playLoop(
        path, *bus, volume, pitch);
    if (handle == heritage::audio::kInvalidAudioHandle)
    {
        runtime->m_lastAudioError = runtime->m_audio->lastError();
    }
    else
    {
        runtime->m_audioHandles.insert(handle);
        runtime->m_lastAudioError.clear();
    }

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioStop(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;

    const bool owned = runtime->m_audioHandles.contains(handle);
    const bool stopped = owned && runtime->m_audio->stop(handle);
    if (owned)
        runtime->m_audioHandles.erase(handle);

    runtime->m_api.lua_pushboolean(state, stopped ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioStopAll(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    for (const heritage::audio::AudioHandle handle : runtime->m_audioHandles)
        runtime->m_audio->stop(handle);
    runtime->m_audioHandles.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioIsPlaying(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const bool playing = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->isPlaying(handle);
    runtime->m_api.lua_pushboolean(state, playing ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const float volume = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const bool changed = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->setHandleVolume(handle, volume);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetPitch(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const heritage::audio::AudioHandle handle = valid && value > 0
        ? static_cast<heritage::audio::AudioHandle>(value)
        : heritage::audio::kInvalidAudioHandle;
    const float pitch = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const bool changed = runtime->m_audioHandles.contains(handle)
        && runtime->m_audio->setHandlePitch(handle, pitch);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetMasterVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_audio->setMasterVolume(
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 1.0)));
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetMasterVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(runtime->m_audio->masterVolume()));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetBusVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const std::string busName = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const auto bus = heritage::audio::parseAudioBus(busName);
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_audio->setBusVolume(
        *bus,
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_lastAudioError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetBusVolume(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (!runtime->m_audio)
    {
        runtime->m_lastAudioError = "Audio service is unavailable.";
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    const std::string busName = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const auto bus = heritage::audio::parseAudioBus(busName);
    if (!bus)
    {
        runtime->m_lastAudioError = "Unknown audio bus: " + busName;
        runtime->m_api.lua_pushnumber(state, 0.0);
        return 1;
    }

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(runtime->m_audio->busVolume(*bus)));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    std::string error = runtime->m_lastAudioError;
    if (error.empty() && runtime->m_audio)
        error = runtime->m_audio->lastError();
    runtime->m_api.lua_pushlstring(state, error.c_str(), error.size());
    return 1;
}

void LuaModuleRuntime::registerAudioBindings()
{
    registerFunction("Audio", "IsAvailable", &LuaCoreBindingHandlers::luaAudioIsAvailable);
    registerFunction("Audio", "GetBackend", &LuaCoreBindingHandlers::luaAudioGetBackend);
    registerFunction("Audio", "PlaySound", &LuaCoreBindingHandlers::luaAudioPlaySound);
    registerFunction("Audio", "PlayLoop", &LuaCoreBindingHandlers::luaAudioPlayLoop);
    registerFunction("Audio", "Stop", &LuaCoreBindingHandlers::luaAudioStop);
    registerFunction("Audio", "StopAll", &LuaCoreBindingHandlers::luaAudioStopAll);
    registerFunction("Audio", "IsPlaying", &LuaCoreBindingHandlers::luaAudioIsPlaying);
    registerFunction("Audio", "SetVolume", &LuaCoreBindingHandlers::luaAudioSetVolume);
    registerFunction("Audio", "SetPitch", &LuaCoreBindingHandlers::luaAudioSetPitch);
    registerFunction("Audio", "SetMasterVolume", &LuaCoreBindingHandlers::luaAudioSetMasterVolume);
    registerFunction("Audio", "GetMasterVolume", &LuaCoreBindingHandlers::luaAudioGetMasterVolume);
    registerFunction("Audio", "SetBusVolume", &LuaCoreBindingHandlers::luaAudioSetBusVolume);
    registerFunction("Audio", "GetBusVolume", &LuaCoreBindingHandlers::luaAudioGetBusVolume);
    registerFunction("Audio", "GetLastError", &LuaCoreBindingHandlers::luaAudioGetLastError);
}

} // namespace heritage::modules
