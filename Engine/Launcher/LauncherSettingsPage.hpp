#pragma once

#include <glad/glad.h>

#include "../HeritageEngine/Core/Settings/VideoSettings.hpp"
#include "../HeritageEngine/Graphics/DisplaySystem.hpp"

namespace racing::launcher {

// Draws the selected module's settings without applying them to the launcher
// window. Both data objects are the same ones used by Heritage Engine.
// Returns true when persistent settings changed.
bool drawSettingsTabs(
    heritage::settings::VideoSettings& videoSettings,
    heritage::graphics::DisplaySystem& display);

} // namespace racing::launcher
