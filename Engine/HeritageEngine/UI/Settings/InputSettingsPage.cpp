#include "InputSettingsPage.hpp"
#include "InputSettingsPageInternal.hpp"

#include <imgui.h>

#include "../../Input/InputSystem.hpp"

namespace heritage::ui::settings {

void drawInputSettingsPage(heritage::input::InputSystem& input)
{
    using namespace input_settings_internal;
    ImGui::Spacing();
    ImGui::TextDisabled("Multi-device input and analogue processing");
    ImGui::Separator();
    ImGui::Text("Input system: %s", input.isAvailable() ? "Available" : "Unavailable");
    ImGui::Text("Registered module actions: %d",
        static_cast<int>(input.actionCount()));
    ImGui::TextWrapped(
        "Each action has up to eight equal binding slots. Analogue bindings additionally "
        "store independent deadzones, sensitivity and cubic Bezier response curves. "
        "Named profiles preserve complete restorable snapshots.");

    ImGui::Spacing();
    if (ImGui::BeginTabBar("InputConfigurationTabs"))
    {
        if (ImGui::BeginTabItem("Bindings"))
        {
            drawBindingsTab(input);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Analogue"))
        {
            drawAnalogueTab(input);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Profiles"))
        {
            drawProfilesTab(input);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (!input.lastError().empty())
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
            "%s",
            input.lastError().c_str());
    }
}

} // namespace heritage::ui::settings
