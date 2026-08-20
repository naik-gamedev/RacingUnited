#include "InputSettingsPageInternal.hpp"

#include <algorithm>
#include <string>

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
    if (internalName == "Select Neutral")
        return "Neutral";
    if (internalName == "Select Reverse")
        return "Reverse";
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

    // INPUT03A: InputSystem::actions() is name-sorted for deterministic generic
    // callers, which makes direct gears appear lexicographically (Gear 1,
    // Gear 10, Gear 11 ... Gear 2). The Gears settings category is a physical
    // control sequence, so present it in the deliberate driver-facing order:
    // Shift Up, Shift Down, Clutch, Neutral, Reverse, then Gear 1 through Gear 24.
    if (group == "Gears")
    {
        const auto gearRank = [](const std::string& name) {
            if (name == "Shift Up") return 0;
            if (name == "Shift Down") return 1;
            if (name == "Clutch") return 2;
            if (name == "Select Neutral") return 3;
            if (name == "Select Reverse") return 4;
            constexpr const char* prefix = "Gear ";
            if (name.rfind(prefix, 0) == 0)
            {
                try
                {
                    const int gear = std::stoi(name.substr(5));
                    if (gear >= 1 && gear <= 24)
                        return 4 + gear;
                }
                catch (...)
                {
                }
            }
            return 1000;
        };
        std::stable_sort(
            result.begin(), result.end(),
            [&](const auto& left, const auto& right) {
                const int leftRank = gearRank(left.name);
                const int rightRank = gearRank(right.name);
                if (leftRank != rightRank)
                    return leftRank < rightRank;
                return left.name < right.name;
            });
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

    if (ImGui::BeginTabBar(id, ImGuiTabBarFlags_FittingPolicyResizeDown))
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
