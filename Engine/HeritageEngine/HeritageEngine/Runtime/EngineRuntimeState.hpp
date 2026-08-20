#pragma once

#include <filesystem>

#include "../../Audio/AudioSystem.hpp"
#include "../../Core/Jobs/JobSystem.hpp"
#include "../../Core/Settings/AudioSettings.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../../Graphics/DisplaySystem.hpp"
#include "../../Graphics/WindowSystem.hpp"
#include "../../Input/InputSystem.hpp"
#include "../../Physics/PhysicsWorld.hpp"

struct ImFont;

namespace heritage::engine {

// Long-lived engine services and persistent settings owned by HeritageEngine.
//
// This is deliberately NOT a per-frame scratch/context object. Frame-local
// values remain in the runtime phase that owns them, while process-lifetime
// services no longer live as anonymous globals in HeritageEngine.cpp.
struct EngineRuntimeState final
{
    EngineRuntimeState()
    {
        physics.setJobSystem(&jobs);
    }

    ImFont* fontSmall = nullptr;
    ImFont* fontNormal = nullptr;
    ImFont* fontLarge = nullptr;

    heritage::settings::VideoSettings videoSettings;
    heritage::settings::AudioSettings audioSettings;

    heritage::graphics::DisplaySystem display;
    heritage::graphics::WindowSystem window;
    heritage::audio::AudioSystem audio;
    heritage::input::InputSystem input;
    // JOB01: declared before PhysicsWorld so worker threads outlive every
    // subsystem that may submit work during PhysicsWorld teardown/reset.
    heritage::jobs::JobSystem jobs;
    heritage::physics::PhysicsWorld physics;

    std::filesystem::path videoSettingsPath;
    std::filesystem::path displaySettingsPath;
    std::filesystem::path audioSettingsPath;
    std::filesystem::path inputSettingsPath;
};

} // namespace heritage::engine
