#include "InputSettingsPageInternal.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include <imgui.h>

namespace heritage::ui::settings::input_settings_internal {
namespace {

constexpr float kActionColumnWidth = 170.0f;
constexpr float kBindingColumnWidth = 165.0f;
constexpr float kResetColumnWidth = 68.0f;
constexpr int kTableColumnCount =
    1 + static_cast<int>(heritage::input::InputSystem::kMaxBindingsPerAction) + 1;

std::string& bindingsGroupSelection()
{
    static std::string group;
    return group;
}

void drawBindingCell(
    heritage::input::InputSystem& input,
    const heritage::input::InputActionInfo& action,
    std::size_t bindingIndex)
{
    if (bindingIndex >= action.bindings.size())
    {
        ImGui::TextDisabled("Add Binding");
        return;
    }

    const auto& binding = action.bindings[bindingIndex];
    const bool occupied = !binding.binding.empty();
    const bool capturing = input.isCapturingBinding()
        && input.captureAction() == action.name
        && input.captureBindingIndex() == bindingIndex;

    ImGui::PushID(static_cast<int>(bindingIndex));

    std::string buttonLabel;
    if (capturing)
        buttonLabel = "Press input...";
    else if (occupied)
        buttonLabel = binding.displayName;
    else
        buttonLabel = "Add Binding";
    buttonLabel += "##binding";

    const float width = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button(buttonLabel.c_str(), ImVec2(width, 0.0f)) && !capturing)
        input.beginBindingCapture(action.name, bindingIndex);

    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Binding %zu", bindingIndex + 1);
        if (!occupied)
        {
            ImGui::TextDisabled("This slot is empty.");
            ImGui::Separator();
            ImGui::TextDisabled("Click to add a binding directly to this column.");
        }
        else
        {
            ImGui::TextUnformatted(binding.displayName.c_str());
            ImGui::TextDisabled("Stored as: %s", binding.binding.c_str());
            if (binding.analog)
            {
                ImGui::Text("Raw %.3f  ->  Processed %.3f",
                    binding.rawValue,
                    binding.value);
                ImGui::TextDisabled("Response settings are in the Analogue tab.");
            }
            else if (binding.active)
            {
                ImGui::Text("Current value: %.3f", binding.value);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Click to replace. Right-click to clear only this slot.");
        }
        ImGui::EndTooltip();
    }

    if (occupied
        && !capturing
        && ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        input.removeBinding(action.name, bindingIndex);
        invalidateAnalogueEditorSelection();
    }

    ImGui::PopID();
}


} // namespace

void drawBindingsTab(heritage::input::InputSystem& input)
{
    ImGui::TextDisabled("Detected devices");
    ImGui::Separator();
    ImGui::Text("Keyboard and mouse");
    if (ImGui::Button("REFRESH CONTROLLERS / WHEELS"))
        input.refreshInputDevices();
    ImGui::SameLine();
    ImGui::TextDisabled("Manual to avoid gameplay hitching");

    const auto devices = input.gamepads();
    if (devices.empty())
    {
        ImGui::TextDisabled("No GLFW-standard gamepad detected.");
    }
    else
    {
        for (const auto& device : devices)
        {
            ImGui::Text("Standard gamepad %d: %s", device.ordinal + 1, device.name.c_str());
            if (!device.guid.empty())
                ImGui::TextDisabled("GUID: %s", device.guid.c_str());
        }
    }

    const auto directInputDevices = input.directInputDevices();
    if (!input.directInputAvailable())
    {
        ImGui::TextDisabled("Windows DirectInput backend unavailable.");
    }
    else if (directInputDevices.empty())
    {
        ImGui::TextDisabled("No additional DirectInput wheel, shifter, pedal set, or legacy controller detected.");
    }
    else
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Racing peripherals and legacy controllers (DirectInput)");
        for (const auto& device : directInputDevices)
        {
            ImGui::Text("%s%s",
                device.name.c_str(),
                device.connected ? "" : " (disconnected)");
            ImGui::TextDisabled(
                "%d axes, %d buttons, %d POV hats",
                device.axisCount,
                device.buttonCount,
                device.povCount);
            if (!device.instanceGuid.empty())
                ImGui::TextDisabled("Device ID: %s", device.instanceGuid.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Module action bindings");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Click an occupied cell to replace it, or click Add Binding in any empty cell. "
        "Right-click clears only that exact slot; later bindings never shift left.");

    const auto allActions = input.actions();
    if (allActions.empty())
    {
        ImGui::TextWrapped(
            "The active module has no registered input actions. Add declarations to "
            "Modules/<ModuleID>/Data/InputActions.ini or register them through Lua.");
        return;
    }

    std::string& selectedGroup = bindingsGroupSelection();
    drawActionGroupTabs(input, "BindingActionGroups", selectedGroup);
    const auto actions = actionsInGroup(allActions, selectedGroup);

    if (actions.empty())
    {
        ImGui::TextWrapped(
            "The module has reserved the '%s' input category, but it does not declare any actions there yet.",
            selectedGroup.c_str());
        return;
    }

    if (input.isCapturingBinding())
    {
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Press a keyboard key, mouse button, standard gamepad control, wheel button, "
            "pedal, shifter, handbrake, or other DirectInput control.");
        ImGui::Text("Action: %s", displayActionName(input.captureAction()).c_str());
        ImGui::TextDisabled(
            "The input will be written to Binding %zu without moving any other slot.",
            input.captureBindingIndex() + 1);
        if (ImGui::Button("CANCEL REBIND"))
            input.cancelBindingCapture();
        ImGui::Spacing();
    }

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Borders
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_ScrollX
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_SizingFixedFit;

    const float innerWidth = kActionColumnWidth
        + kBindingColumnWidth
            * static_cast<float>(heritage::input::InputSystem::kMaxBindingsPerAction)
        + kResetColumnWidth;

    const float tableHeight = std::clamp(
        ImGui::GetContentRegionAvail().y * 0.62f,
        225.0f,
        360.0f);

    ImGui::PushID(selectedGroup.c_str());
    if (ImGui::BeginTable(
        "ModuleInputBindingTable",
        kTableColumnCount,
        tableFlags,
        ImVec2(0.0f, tableHeight),
        innerWidth))
    {
        ImGui::TableSetupScrollFreeze(1, 1);
        ImGui::TableSetupColumn(
            "Action",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
            kActionColumnWidth);

        static constexpr std::array<const char*,
            heritage::input::InputSystem::kMaxBindingsPerAction> bindingHeaders = {
            "Binding 1", "Binding 2", "Binding 3", "Binding 4",
            "Binding 5", "Binding 6", "Binding 7", "Binding 8"
        };

        for (const char* header : bindingHeaders)
        {
            ImGui::TableSetupColumn(
                header,
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
                kBindingColumnWidth);
        }

        ImGui::TableSetupColumn(
            "Reset",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide,
            kResetColumnWidth);
        ImGui::TableHeadersRow();

        for (const auto& action : actions)
        {
            ImGui::PushID(action.name.c_str());
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(displayActionName(action.name).c_str());

            for (std::size_t bindingIndex = 0;
                bindingIndex < heritage::input::InputSystem::kMaxBindingsPerAction;
                ++bindingIndex)
            {
                ImGui::TableSetColumnIndex(1 + static_cast<int>(bindingIndex));
                drawBindingCell(input, action, bindingIndex);
            }

            ImGui::TableSetColumnIndex(
                1 + static_cast<int>(heritage::input::InputSystem::kMaxBindingsPerAction));
            if (ImGui::Button("Reset##defaults", ImVec2(-1.0f, 0.0f)))
            {
                input.resetBindings(action.name);
                invalidateAnalogueEditorSelection();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::TextDisabled("Bindings are saved per module:");
    ImGui::TextWrapped("%s", input.settingsPath().string().c_str());
    ImGui::TextDisabled(
        "Device-specific gamepad and DirectInput bindings remember their device IDs and reconnect automatically.");
}

ImVec2 graphPoint(const ImVec2& origin, const ImVec2& size, float x, float y)
{
    return ImVec2(
        origin.x + std::clamp(x, 0.0f, 1.0f) * size.x,
        origin.y + (1.0f - std::clamp(y, 0.0f, 1.0f)) * size.y);
}


} // namespace heritage::ui::settings::input_settings_internal
