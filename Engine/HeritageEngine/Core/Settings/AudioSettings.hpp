#pragma once

namespace heritage::settings {

// Module-specific audio configuration. The same values are used by the
// native audio system, the engine Audio tab, and the Lua Audio API.
struct AudioSettings
{
    float masterVolume = 1.0f;
    float musicVolume = 0.80f;
    float effectsVolume = 1.0f;
    float ambienceVolume = 0.80f;
    float uiVolume = 1.0f;
    float voiceVolume = 1.0f;
    bool muteWhenUnfocused = false;
};

} // namespace heritage::settings
