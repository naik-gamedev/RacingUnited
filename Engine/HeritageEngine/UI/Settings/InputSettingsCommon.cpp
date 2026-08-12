#include "InputSettingsPageInternal.hpp"

#include <algorithm>

#include <imgui.h>

namespace heritage::ui::settings::input_settings_internal {

std::string displayActionName(const std::string& internalName)
{
    // INPUT02: the current vehicle runtime's two steering action semantics are
    // physically opposite to their historical internal identifiers. Keep the
    // internal/save keys stable, but present the direction that the action
    // actually produces to the user in Settings.
    if (internalName == "Steer Left")
        return "Steer Right";
    if (internalName == "Steer Right")
        return "Steer Left";
    return internalName;
}

std::vector<heritage::input::InputActionInfo> actionsInGroup(
    const std::vector<heritage::input::InputActionInfo>& actions,
    const std::string& group)
{
    std::vector<heritage::input::InputActionInfo> result;
    for (const auto& action : actions)
    {
        if (action.group == group)
            result.push_back(action);
    }
    return result;
}

void drawActionGroupTabs(
    heritage::input::InputSystem& input,
    const char* id,
    std::string& selectedGroup)
{
    std::vector<std::string> groups = input.actionGroups();
    if (groups.empty())
        groups.push_back("Common");

    if (std::find(groups.begin(), groups.end(), selectedGroup) == groups.end())
        selectedGroup = groups.front();

    if (ImGui::BeginTabBar(id, ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (const std::string& group : groups)
        {
            if (ImGui::BeginTabItem(group.c_str()))
            {
                selectedGroup = group;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}

} // namespace heritage::ui::settings::input_settings_internal
