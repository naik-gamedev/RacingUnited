#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"
#include "../../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include "../../../Audio/AudioSystem.hpp"
#include "../../../Audio/Vehicles/VehicleAudioRuntime.hpp"
#include "../../../Audio/Weather/WeatherAudioRuntime.hpp"
#include "../../../Audio/Lab/EngineSoundCaptureLab.hpp"
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
namespace {

int absoluteIndex(const LuaApi& api, lua_State* state, int index)
{
    return index > 0 ? index : api.lua_gettop(state) + index + 1;
}

void popValue(const LuaApi& api, lua_State* state)
{
    api.lua_settop(state, -2);
}

double audioFieldNumber(
    const LuaApi& api, lua_State* state, int tableIndex,
    const char* field, double fallback)
{
    api.lua_getfield(state, absoluteIndex(api, state, tableIndex), field);
    int converted = 0;
    const LuaNumber value = api.lua_tonumberx(state, -1, &converted);
    popValue(api, state);
    return converted ? static_cast<double>(value) : fallback;
}

std::string audioFieldString(
    const LuaApi& api, lua_State* state, int tableIndex,
    const char* field, const std::string& fallback)
{
    api.lua_getfield(state, absoluteIndex(api, state, tableIndex), field);
    std::string value = fallback;
    if (api.lua_type(state, -1) == kLuaTypeString)
    {
        std::size_t length = 0;
        if (const char* text = api.lua_tolstring(state, -1, &length))
            value.assign(text, length);
    }
    popValue(api, state);
    return value;
}

heritage::audio::AudioVector3 audioFieldVector3(
    const LuaApi& api, lua_State* state, int tableIndex,
    const char* field, const heritage::audio::AudioVector3& fallback)
{
    api.lua_getfield(state, absoluteIndex(api, state, tableIndex), field);
    if (api.lua_type(state, -1) != kLuaTypeTable)
    {
        popValue(api, state);
        return fallback;
    }
    const int vectorIndex = api.lua_gettop(state);
    heritage::audio::AudioVector3 value = fallback;
    float* components[]{ &value.x, &value.y, &value.z };
    for (LuaInteger component = 1; component <= 3; ++component)
    {
        api.lua_rawgeti(state, vectorIndex, component);
        int converted = 0;
        const LuaNumber candidate = api.lua_tonumberx(state, -1, &converted);
        if (converted)
            *components[static_cast<std::size_t>(component - 1)] = static_cast<float>(candidate);
        popValue(api, state);
    }
    popValue(api, state);
    return value;
}

template <typename Callback>
void withAudioTable(
    const LuaApi& api, lua_State* state, int parent,
    const char* field, Callback callback)
{
    api.lua_getfield(state, absoluteIndex(api, state, parent), field);
    if (api.lua_type(state, -1) == kLuaTypeTable)
        callback(api.lua_gettop(state));
    popValue(api, state);
}

void pushNumberField(
    const LuaApi& api, lua_State* state,
    const char* name, double value)
{
    api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    api.lua_setfield(state, -2, name);
}

void pushBooleanField(
    const LuaApi& api, lua_State* state,
    const char* name, bool value)
{
    api.lua_pushboolean(state, value ? 1 : 0);
    api.lua_setfield(state, -2, name);
}

void pushStringField(
    const LuaApi& api, lua_State* state,
    const char* name, const char* value)
{
    api.lua_pushstring(state, value);
    api.lua_setfield(state, -2, name);
}

} // namespace

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

int LuaCoreBindingHandlers::luaAudioGetRuntimeStats(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::size_t activeVoices = runtime->m_audio
        ? runtime->m_audio->activeVoiceCount() : 0;
    const std::size_t cachedBytes = runtime->m_audio
        ? runtime->m_audio->cachedAudioBytes() : 0;
    runtime->m_api.lua_createtable(state, 0, 2);
    pushNumberField(runtime->m_api, state, "activeVoices",
        static_cast<double>(activeVoices));
    pushNumberField(runtime->m_api, state, "cachedAudioMiB",
        static_cast<double>(cachedBytes) / (1024.0 * 1024.0));
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

int LuaCoreBindingHandlers::luaAudioEngineLabGetState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    runtime->m_engineSoundLab->update();
    const auto value = runtime->m_engineSoundLab->status();
    runtime->m_api.lua_createtable(state, 0, 14);
    pushBooleanField(runtime->m_api, state, "available", value.available);
    pushBooleanField(runtime->m_api, state, "capturing", value.capturing);
    pushBooleanField(runtime->m_api, state, "previewPlaying", value.previewPlaying);
    pushNumberField(runtime->m_api, state, "progress", value.progress);
    pushNumberField(runtime->m_api, state, "requestedDurationSeconds", value.requestedDurationSeconds);
    pushNumberField(runtime->m_api, state, "capturedDurationSeconds", value.capturedDurationSeconds);
    pushNumberField(runtime->m_api, state, "peak", value.peak);
    pushNumberField(runtime->m_api, state, "rms", value.rms);
    pushNumberField(runtime->m_api, state, "sampleRate", value.sampleRate);
    const std::string rawPath = heritage::paths::toUtf8(value.lastRawPath);
    const std::string previewPath = heritage::paths::toUtf8(value.lastPreviewPath);
    runtime->m_api.lua_pushlstring(state, rawPath.c_str(), rawPath.size());
    runtime->m_api.lua_setfield(state, -2, "lastRawPath");
    runtime->m_api.lua_pushlstring(state, previewPath.c_str(), previewPath.size());
    runtime->m_api.lua_setfield(state, -2, "lastPreviewPath");
    runtime->m_api.lua_pushlstring(state, value.profileName.c_str(), value.profileName.size());
    runtime->m_api.lua_setfield(state, -2, "profileName");
    runtime->m_api.lua_pushlstring(state, value.lastError.c_str(), value.lastError.size());
    runtime->m_api.lua_setfield(state, -2, "lastError");
    const std::string root = heritage::paths::toUtf8(runtime->m_engineSoundLab->root());
    runtime->m_api.lua_pushlstring(state, root.c_str(), root.size());
    runtime->m_api.lua_setfield(state, -2, "root");
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabGetProfile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const auto& p = runtime->m_engineSoundLab->profile();
    runtime->m_api.lua_createtable(state, 0, 23);
#define HERITAGE_PUSH_PROFILE_FIELD(name) pushNumberField(runtime->m_api, state, #name, p.name)
    HERITAGE_PUSH_PROFILE_FIELD(inputGainDb);
    HERITAGE_PUSH_PROFILE_FIELD(highPassHz);
    HERITAGE_PUSH_PROFILE_FIELD(lowPassHz);
    HERITAGE_PUSH_PROFILE_FIELD(bodyGainDb);
    HERITAGE_PUSH_PROFILE_FIELD(bodyFrequencyHz);
    HERITAGE_PUSH_PROFILE_FIELD(bodyQ);
    HERITAGE_PUSH_PROFILE_FIELD(presenceCutDb);
    HERITAGE_PUSH_PROFILE_FIELD(presenceFrequencyHz);
    HERITAGE_PUSH_PROFILE_FIELD(presenceQ);
    HERITAGE_PUSH_PROFILE_FIELD(highShelfDb);
    HERITAGE_PUSH_PROFILE_FIELD(pulseSoftening);
    HERITAGE_PUSH_PROFILE_FIELD(saturation);
    HERITAGE_PUSH_PROFILE_FIELD(mechanicalPresence);
    HERITAGE_PUSH_PROFILE_FIELD(intakePresence);
    HERITAGE_PUSH_PROFILE_FIELD(intakeFrequencyHz);
    HERITAGE_PUSH_PROFILE_FIELD(exhaustMuffling);
    HERITAGE_PUSH_PROFILE_FIELD(exhaustBodyGainDb);
    HERITAGE_PUSH_PROFILE_FIELD(exhaustBodyFrequencyHz);
    HERITAGE_PUSH_PROFILE_FIELD(exhaustBodyQ);
    HERITAGE_PUSH_PROFILE_FIELD(cabinDamping);
    HERITAGE_PUSH_PROFILE_FIELD(cabinLowFrequencyLeak);
    HERITAGE_PUSH_PROFILE_FIELD(cabinResonance);
    HERITAGE_PUSH_PROFILE_FIELD(cabinResonanceHz);
    HERITAGE_PUSH_PROFILE_FIELD(reverbPreview);
    HERITAGE_PUSH_PROFILE_FIELD(occlusionPreview);
    HERITAGE_PUSH_PROFILE_FIELD(outputGainDb);
#undef HERITAGE_PUSH_PROFILE_FIELD
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabSetProfile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    if (runtime->m_api.lua_type(state, 1) != kLuaTypeTable)
    {
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }
    auto p = runtime->m_engineSoundLab->profile();
#define HERITAGE_READ_PROFILE_FIELD(name) p.name = static_cast<float>(audioFieldNumber(runtime->m_api, state, 1, #name, p.name))
    HERITAGE_READ_PROFILE_FIELD(inputGainDb);
    HERITAGE_READ_PROFILE_FIELD(highPassHz);
    HERITAGE_READ_PROFILE_FIELD(lowPassHz);
    HERITAGE_READ_PROFILE_FIELD(bodyGainDb);
    HERITAGE_READ_PROFILE_FIELD(bodyFrequencyHz);
    HERITAGE_READ_PROFILE_FIELD(bodyQ);
    HERITAGE_READ_PROFILE_FIELD(presenceCutDb);
    HERITAGE_READ_PROFILE_FIELD(presenceFrequencyHz);
    HERITAGE_READ_PROFILE_FIELD(presenceQ);
    HERITAGE_READ_PROFILE_FIELD(highShelfDb);
    HERITAGE_READ_PROFILE_FIELD(pulseSoftening);
    HERITAGE_READ_PROFILE_FIELD(saturation);
    HERITAGE_READ_PROFILE_FIELD(mechanicalPresence);
    HERITAGE_READ_PROFILE_FIELD(intakePresence);
    HERITAGE_READ_PROFILE_FIELD(intakeFrequencyHz);
    HERITAGE_READ_PROFILE_FIELD(exhaustMuffling);
    HERITAGE_READ_PROFILE_FIELD(exhaustBodyGainDb);
    HERITAGE_READ_PROFILE_FIELD(exhaustBodyFrequencyHz);
    HERITAGE_READ_PROFILE_FIELD(exhaustBodyQ);
    HERITAGE_READ_PROFILE_FIELD(cabinDamping);
    HERITAGE_READ_PROFILE_FIELD(cabinLowFrequencyLeak);
    HERITAGE_READ_PROFILE_FIELD(cabinResonance);
    HERITAGE_READ_PROFILE_FIELD(cabinResonanceHz);
    HERITAGE_READ_PROFILE_FIELD(reverbPreview);
    HERITAGE_READ_PROFILE_FIELD(occlusionPreview);
    HERITAGE_READ_PROFILE_FIELD(outputGainDb);
#undef HERITAGE_READ_PROFILE_FIELD
    runtime->m_engineSoundLab->setProfile(p);
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabStartCalibrationCapture(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const float duration = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, 6.0));
    const bool started = runtime->m_engineSoundLab->startCalibrationCapture(duration);
    runtime->m_api.lua_pushboolean(state, started ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabStartBankCapture(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const std::string vehicle = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Peugeot206RC");
    const std::string engine = LuaModuleRuntime::stringArgument(*runtime, state, 2, "EW10J4S");
    const int rpm = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 2000.0));
    const int throttle = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 50.0));
    const float duration = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 4.0));
    const bool started = runtime->m_engineSoundLab->startBankCapture(
        vehicle, engine, rpm, throttle, duration);
    runtime->m_api.lua_pushboolean(state, started ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabStopCapture(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    runtime->m_engineSoundLab->stopCapture();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabPlayPreview(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const std::string perspective = LuaModuleRuntime::stringArgument(*runtime, state, 1, "raw");
    const bool played = runtime->m_engineSoundLab->playPreview(
        heritage::audio::lab::parseEngineSoundPerspective(perspective.c_str()));
    runtime->m_api.lua_pushboolean(state, played ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabStopPreview(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    runtime->m_engineSoundLab->stopPreview();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabSaveProfile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const std::string name = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Peugeot206RC_EW10J4S");
    const bool saved = runtime->m_engineSoundLab->saveProfile(name);
    runtime->m_api.lua_pushboolean(state, saved ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioEngineLabLoadProfile(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_engineSoundLab)
        return 0;
    const std::string name = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Peugeot206RC_EW10J4S");
    const bool loaded = runtime->m_engineSoundLab->loadProfile(name);
    runtime->m_api.lua_pushboolean(state, loaded ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioCreateVehicleSound(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleAudio)
        return 0;
    const heritage::vehicles::VehicleHandle vehicle =
        LuaModuleRuntime::vehicleHandleArgument(*runtime, state, 1);
    if (runtime->m_api.lua_type(state, 2) != kLuaTypeTable)
    {
        runtime->m_lastAudioError = "Audio.CreateVehicleSound expects a definition table.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::audio::vehicles::VehicleAudioDefinition definition;
    definition.id = audioFieldString(runtime->m_api, state, 2, "id", definition.id);
    definition.category = audioFieldString(runtime->m_api, state, 2, "category", definition.category);
    definition.cylinderCount = static_cast<int>(audioFieldNumber(
        runtime->m_api, state, 2, "cylinders", definition.cylinderCount));
    definition.cycleRevolutions = static_cast<int>(audioFieldNumber(
        runtime->m_api, state, 2, "cycleRevolutions", definition.cycleRevolutions));
    definition.idleRpm = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "idleRpm", definition.idleRpm));
    definition.redlineRpm = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "redlineRpm", definition.redlineRpm));
    definition.referenceRpm = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "referenceRpm", definition.referenceRpm));
    definition.maximumTorqueNm = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "maximumTorqueNm", definition.maximumTorqueNm));
    runtime->m_api.lua_getfield(state, 2, "firingOrder");
    if (runtime->m_api.lua_type(state, -1) == kLuaTypeTable)
    {
        const int order = runtime->m_api.lua_gettop(state);
        const std::size_t count = (std::min)(
            runtime->m_api.lua_rawlen(state, order),
            static_cast<std::size_t>(24));
        definition.engineAcoustics.firingOrder.clear();
        for (std::size_t index = 0; index < count; ++index)
        {
            runtime->m_api.lua_rawgeti(
                state, order, static_cast<LuaInteger>(index + 1));
            int converted = 0;
            const LuaNumber value = runtime->m_api.lua_tonumberx(
                state, -1, &converted);
            if (converted)
                definition.engineAcoustics.firingOrder.push_back(
                    static_cast<int>(value));
            popValue(runtime->m_api, state);
        }
    }
    popValue(runtime->m_api, state);
    withAudioTable(runtime->m_api, state, 2, "engineAcoustics", [&](int acoustics)
    {
        auto& value = definition.engineAcoustics;
        value.displacementLiters = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "displacementLiters",
            value.displacementLiters));
        value.compressionRatio = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "compressionRatio",
            value.compressionRatio));
        value.exhaustPulseSharpness = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "exhaustPulseSharpness",
            value.exhaustPulseSharpness));
        value.intakePulseSharpness = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "intakePulseSharpness",
            value.intakePulseSharpness));
        value.exhaustHeaderImbalance = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "exhaustHeaderImbalance",
            value.exhaustHeaderImbalance));
        value.intakeResonanceOrder = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "intakeResonanceOrder",
            value.intakeResonanceOrder));
        value.mechanicalOrderGain = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "mechanicalOrderGain",
            value.mechanicalOrderGain));
        value.combustionVariation = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "combustionVariation",
            value.combustionVariation));
        value.variableIntakeTransitionRpm = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "variableIntakeTransitionRpm",
            value.variableIntakeTransitionRpm));
        value.variableIntakeTransitionWidthRpm = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "variableIntakeTransitionWidthRpm",
            value.variableIntakeTransitionWidthRpm));
        value.variableIntakeGain = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, acoustics, "variableIntakeGain",
            value.variableIntakeGain));
    });
    definition.cabinRadiusMeters = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "cabinRadiusMeters", definition.cabinRadiusMeters));
    definition.fullDetailDistanceMeters = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "fullDetailDistanceMeters", definition.fullDetailDistanceMeters));
    definition.reducedDetailDistanceMeters = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "reducedDetailDistanceMeters", definition.reducedDetailDistanceMeters));
    definition.maximumDistanceMeters = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 2, "maximumDistanceMeters", definition.maximumDistanceMeters));

    withAudioTable(runtime->m_api, state, 2, "gains", [&](int gains)
    {
        definition.gains.exhaust = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "exhaust", definition.gains.exhaust));
        definition.gains.intake = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "intake", definition.gains.intake));
        definition.gains.mechanical = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "mechanical", definition.gains.mechanical));
        definition.gains.transmission = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "transmission", definition.gains.transmission));
        definition.gains.tires = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "tires", definition.gains.tires));
        definition.gains.wind = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "wind", definition.gains.wind));
        definition.gains.chassis = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, gains, "chassis", definition.gains.chassis));
    });
    withAudioTable(runtime->m_api, state, 2, "emitters", [&](int emitters)
    {
        definition.engineEmitter = audioFieldVector3(
            runtime->m_api, state, emitters, "engine", definition.engineEmitter);
        definition.intakeEmitter = audioFieldVector3(
            runtime->m_api, state, emitters, "intake", definition.intakeEmitter);
        definition.exhaustEmitter = audioFieldVector3(
            runtime->m_api, state, emitters, "exhaust", definition.exhaustEmitter);
        definition.transmissionEmitter = audioFieldVector3(
            runtime->m_api, state, emitters, "transmission", definition.transmissionEmitter);
        definition.chassisEmitter = audioFieldVector3(
            runtime->m_api, state, emitters, "chassis", definition.chassisEmitter);
    });
    withAudioTable(runtime->m_api, state, 2, "samples", [&](int samples)
    {
        definition.samples.gain = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, samples, "gain", definition.samples.gain));
        definition.samples.proceduralGain = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, samples, "proceduralGain",
            definition.samples.proceduralGain));
        const std::string startup = audioFieldString(
            runtime->m_api, state, samples, "startup", {});
        if (!startup.empty() && runtime->m_context)
        {
            definition.samples.startupPath = runtime->m_context->resolveAssetPath(
                heritage::paths::fromUtf8(startup));
        }

        runtime->m_api.lua_getfield(state, samples, "engineLoops");
        if (runtime->m_api.lua_type(state, -1) == kLuaTypeTable)
        {
            const int loops = runtime->m_api.lua_gettop(state);
            const std::size_t count = (std::min)(
                runtime->m_api.lua_rawlen(state, loops),
                static_cast<std::size_t>(16));
            for (std::size_t index = 0; index < count; ++index)
            {
                runtime->m_api.lua_rawgeti(
                    state, loops, static_cast<LuaInteger>(index + 1));
                if (runtime->m_api.lua_type(state, -1) == kLuaTypeTable)
                {
                    const int loop = runtime->m_api.lua_gettop(state);
                    const std::string relativePath = audioFieldString(
                        runtime->m_api, state, loop, "path", {});
                    heritage::audio::vehicles::VehicleEngineSample sample;
                    sample.referenceRpm = static_cast<float>(audioFieldNumber(
                        runtime->m_api, state, loop, "rpm", sample.referenceRpm));
                    sample.gain = static_cast<float>(audioFieldNumber(
                        runtime->m_api, state, loop, "gain", sample.gain));
                    if (!relativePath.empty() && runtime->m_context)
                    {
                        sample.path = runtime->m_context->resolveAssetPath(
                            heritage::paths::fromUtf8(relativePath));
                    }
                    if (!sample.path.empty())
                        definition.samples.loops.push_back(std::move(sample));
                }
                popValue(runtime->m_api, state);
            }
        }
        popValue(runtime->m_api, state);
    });
    withAudioTable(runtime->m_api, state, 2, "events", [&](int events)
    {
        definition.events.gain = static_cast<float>(audioFieldNumber(
            runtime->m_api, state, events, "gain", definition.events.gain));
        definition.events.maximumVoices = static_cast<int>(audioFieldNumber(
            runtime->m_api, state, events, "maximumVoices",
            definition.events.maximumVoices));
        const auto readPaths = [&](
            const char* field,
            std::vector<std::filesystem::path>& destination)
        {
            runtime->m_api.lua_getfield(state, events, field);
            if (runtime->m_api.lua_type(state, -1) == kLuaTypeTable)
            {
                const int array = runtime->m_api.lua_gettop(state);
                const std::size_t count = (std::min)(
                    runtime->m_api.lua_rawlen(state, array),
                    static_cast<std::size_t>(16));
                for (std::size_t index = 0; index < count; ++index)
                {
                    runtime->m_api.lua_rawgeti(
                        state, array, static_cast<LuaInteger>(index + 1));
                    if (runtime->m_api.lua_type(state, -1) == kLuaTypeString)
                    {
                        std::size_t length = 0;
                        if (const char* text = runtime->m_api.lua_tolstring(
                            state, -1, &length); text && runtime->m_context)
                        {
                            const std::filesystem::path path =
                                runtime->m_context->resolveAssetPath(
                                    heritage::paths::fromUtf8(
                                        std::string(text, length)));
                            if (!path.empty())
                                destination.push_back(path);
                        }
                    }
                    popValue(runtime->m_api, state);
                }
            }
            popValue(runtime->m_api, state);
        };
        readPaths("gearShift", definition.events.gearShift);
        readPaths("suspensionLight", definition.events.suspensionLight);
        readPaths("suspensionHeavy", definition.events.suspensionHeavy);
    });

    const auto handle = runtime->m_vehicleAudio->create(vehicle, definition);
    if (handle == heritage::audio::vehicles::kInvalidVehicleSoundHandle)
        runtime->m_lastAudioError = "Could not create native vehicle sound: "
            + (runtime->m_audio ? runtime->m_audio->lastError() : std::string("audio unavailable"));
    else
        runtime->m_lastAudioError.clear();
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioDestroyVehicleSound(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const bool destroyed = valid && value > 0
        && runtime->m_vehicleAudio->destroy(static_cast<heritage::audio::vehicles::VehicleSoundHandle>(value));
    runtime->m_api.lua_pushboolean(state, destroyed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetVehicleSoundEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const bool enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 2, true);
    const bool changed = valid && value > 0
        && runtime->m_vehicleAudio->setEnabled(
            static_cast<heritage::audio::vehicles::VehicleSoundHandle>(value), enabled);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetVehicleSoundState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_vehicleAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    heritage::audio::vehicles::VehicleAudioTelemetry telemetry;
    if (!valid || value <= 0 || !runtime->m_vehicleAudio->telemetry(
        static_cast<heritage::audio::vehicles::VehicleSoundHandle>(value), telemetry))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_createtable(state, 0, 21);
    pushBooleanField(runtime->m_api, state, "valid", telemetry.valid);
    pushBooleanField(runtime->m_api, state, "interior", telemetry.interior);
    pushStringField(runtime->m_api, state, "detail",
        heritage::audio::vehicles::vehicleAudioDetailName(telemetry.detail));
    pushNumberField(runtime->m_api, state, "distanceMeters", telemetry.distanceMeters);
    pushNumberField(runtime->m_api, state, "engineRpm", telemetry.engineRpm);
    pushNumberField(runtime->m_api, state, "engineLoad", telemetry.engineLoad);
    pushNumberField(runtime->m_api, state, "speedMetersPerSecond", telemetry.speedMetersPerSecond);
    pushNumberField(runtime->m_api, state, "averageTireSlip", telemetry.averageTireSlip);
    pushNumberField(runtime->m_api, state, "suspensionActivity", telemetry.suspensionActivity);
    pushNumberField(runtime->m_api, state, "gear", telemetry.gear);
    pushNumberField(runtime->m_api, state, "activeLayerCount", telemetry.activeLayerCount);
    pushNumberField(runtime->m_api, state, "activeSampleVoices", telemetry.activeSampleVoices);
    pushNumberField(runtime->m_api, state, "activeTransientVoices", telemetry.activeTransientVoices);
    pushBooleanField(runtime->m_api, state, "acousticPathTraced",
        telemetry.acousticPathTraced);
    pushBooleanField(runtime->m_api, state, "directPathOccluded",
        telemetry.directPathOccluded);
    pushNumberField(runtime->m_api, state, "acousticRayCount",
        telemetry.acousticRayCount);
    pushNumberField(runtime->m_api, state, "reflectionPathCount",
        telemetry.reflectionPathCount);
    pushNumberField(runtime->m_api, state, "directPathGain",
        telemetry.directPathGain);
    pushNumberField(runtime->m_api, state, "reflectionGain",
        telemetry.reflectionGain);
    pushNumberField(runtime->m_api, state, "reflectionDelayMilliseconds",
        telemetry.reflectionDelayMilliseconds);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioCreateWeatherSound(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_weatherAudio)
        return 0;
    if (runtime->m_api.lua_type(state, 1) != kLuaTypeTable)
    {
        runtime->m_lastAudioError =
            "Audio.CreateWeatherSound expects a definition table.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::audio::weather::WeatherAudioDefinition definition;
    definition.rainGain = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 1, "rainGain", definition.rainGain));
    definition.windGain = static_cast<float>(audioFieldNumber(
        runtime->m_api, state, 1, "windGain", definition.windGain));
    const auto resolve = [&](const char* field)
    {
        if (!runtime->m_context)
            return std::filesystem::path{};
        const std::string relative = audioFieldString(
            runtime->m_api, state, 1, field, {});
        return relative.empty()
            ? std::filesystem::path{}
            : runtime->m_context->resolveAssetPath(
                heritage::paths::fromUtf8(relative));
    };
    definition.lightRainPath = resolve("lightRain");
    definition.mediumRainPath = resolve("mediumRain");
    definition.heavyRainPath = resolve("heavyRain");
    definition.stormRainPath = resolve("stormRain");
    definition.windPath = resolve("wind");

    const auto handle = runtime->m_weatherAudio->create(definition);
    if (handle == heritage::audio::weather::kInvalidWeatherSoundHandle)
    {
        runtime->m_lastAudioError = "Could not create native weather sound: "
            + (runtime->m_audio ? runtime->m_audio->lastError()
                : std::string("audio unavailable"));
    }
    else
    {
        runtime->m_lastAudioError.clear();
    }
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaCoreBindingHandlers::luaAudioDestroyWeatherSound(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_weatherAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const bool destroyed = valid && value > 0
        && runtime->m_weatherAudio->destroy(
            static_cast<heritage::audio::weather::WeatherSoundHandle>(value));
    runtime->m_api.lua_pushboolean(state, destroyed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioSetWeatherSoundEnabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_weatherAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    const bool enabled = LuaModuleRuntime::booleanArgument(*runtime, state, 2, true);
    const bool changed = valid && value > 0
        && runtime->m_weatherAudio->setEnabled(
            static_cast<heritage::audio::weather::WeatherSoundHandle>(value),
            enabled);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaAudioGetWeatherSoundState(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_weatherAudio)
        return 0;
    int valid = 0;
    const LuaInteger value = runtime->m_api.lua_tointegerx(state, 1, &valid);
    heritage::audio::weather::WeatherAudioTelemetry telemetry;
    if (!valid || value <= 0 || !runtime->m_weatherAudio->telemetry(
        static_cast<heritage::audio::weather::WeatherSoundHandle>(value),
        telemetry))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_createtable(state, 0, 9);
    pushBooleanField(runtime->m_api, state, "enabled", telemetry.enabled);
    pushNumberField(runtime->m_api, state, "rainMmPerHour", telemetry.rainMmPerHour);
    pushNumberField(runtime->m_api, state, "windMetersPerSecond", telemetry.windMetersPerSecond);
    pushNumberField(runtime->m_api, state, "lightRainGain", telemetry.lightRainGain);
    pushNumberField(runtime->m_api, state, "mediumRainGain", telemetry.mediumRainGain);
    pushNumberField(runtime->m_api, state, "heavyRainGain", telemetry.heavyRainGain);
    pushNumberField(runtime->m_api, state, "stormRainGain", telemetry.stormRainGain);
    pushNumberField(runtime->m_api, state, "windGain", telemetry.windGain);
    pushNumberField(runtime->m_api, state, "activeVoiceCount", telemetry.activeVoiceCount);
    return 1;
}

void LuaModuleRuntime::registerAudioBindings()
{
    registerFunction("Audio", "IsAvailable", &LuaCoreBindingHandlers::luaAudioIsAvailable);
    registerFunction("Audio", "GetBackend", &LuaCoreBindingHandlers::luaAudioGetBackend);
    registerFunction("Audio", "GetRuntimeStats", &LuaCoreBindingHandlers::luaAudioGetRuntimeStats);
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
    registerFunction("Audio", "EngineLabGetState", &LuaCoreBindingHandlers::luaAudioEngineLabGetState);
    registerFunction("Audio", "EngineLabGetProfile", &LuaCoreBindingHandlers::luaAudioEngineLabGetProfile);
    registerFunction("Audio", "EngineLabSetProfile", &LuaCoreBindingHandlers::luaAudioEngineLabSetProfile);
    registerFunction("Audio", "EngineLabStartCalibrationCapture", &LuaCoreBindingHandlers::luaAudioEngineLabStartCalibrationCapture);
    registerFunction("Audio", "EngineLabStartBankCapture", &LuaCoreBindingHandlers::luaAudioEngineLabStartBankCapture);
    registerFunction("Audio", "EngineLabStopCapture", &LuaCoreBindingHandlers::luaAudioEngineLabStopCapture);
    registerFunction("Audio", "EngineLabPlayPreview", &LuaCoreBindingHandlers::luaAudioEngineLabPlayPreview);
    registerFunction("Audio", "EngineLabStopPreview", &LuaCoreBindingHandlers::luaAudioEngineLabStopPreview);
    registerFunction("Audio", "EngineLabSaveProfile", &LuaCoreBindingHandlers::luaAudioEngineLabSaveProfile);
    registerFunction("Audio", "EngineLabLoadProfile", &LuaCoreBindingHandlers::luaAudioEngineLabLoadProfile);
    registerFunction("Audio", "CreateVehicleSound", &LuaCoreBindingHandlers::luaAudioCreateVehicleSound);
    registerFunction("Audio", "DestroyVehicleSound", &LuaCoreBindingHandlers::luaAudioDestroyVehicleSound);
    registerFunction("Audio", "SetVehicleSoundEnabled", &LuaCoreBindingHandlers::luaAudioSetVehicleSoundEnabled);
    registerFunction("Audio", "GetVehicleSoundState", &LuaCoreBindingHandlers::luaAudioGetVehicleSoundState);
    registerFunction("Audio", "CreateWeatherSound", &LuaCoreBindingHandlers::luaAudioCreateWeatherSound);
    registerFunction("Audio", "DestroyWeatherSound", &LuaCoreBindingHandlers::luaAudioDestroyWeatherSound);
    registerFunction("Audio", "SetWeatherSoundEnabled", &LuaCoreBindingHandlers::luaAudioSetWeatherSoundEnabled);
    registerFunction("Audio", "GetWeatherSoundState", &LuaCoreBindingHandlers::luaAudioGetWeatherSoundState);
}

} // namespace heritage::modules
