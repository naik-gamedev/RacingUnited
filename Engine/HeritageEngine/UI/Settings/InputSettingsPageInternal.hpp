#pragma once

#include <string>
#include <vector>

#include "../../Input/InputSystem.hpp"

namespace heritage::ui::settings::input_settings_internal {

std::string displayActionName(const std::string& internalName);
std::vector<heritage::input::InputActionInfo> actionsInGroup(
    const std::vector<heritage::input::InputActionInfo>& actions,
    const std::string& group);
void drawActionGroupTabs(
    heritage::input::InputSystem& input,
    const char* id,
    std::string& selectedGroup);

void drawBindingsTab(heritage::input::InputSystem& input);
void drawAnalogueTab(heritage::input::InputSystem& input);
void invalidateAnalogueEditorSelection();
void drawProfilesTab(heritage::input::InputSystem& input);

} // namespace heritage::ui::settings::input_settings_internal
