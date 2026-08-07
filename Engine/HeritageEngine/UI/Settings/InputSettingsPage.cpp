#include "InputSettingsPage.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include <imgui.h>

#include "../../Input/InputSystem.hpp"

namespace heritage::ui::settings {
namespace {

constexpr float kActionColumnWidth = 170.0f;
constexpr float kBindingColumnWidth = 165.0f;
constexpr float kResetColumnWidth = 68.0f;
constexpr int kTableColumnCount =
    1 + static_cast<int>(heritage::input::InputSystem::kMaxBindingsPerAction) + 1;

struct AnalogEditorState
{
    std::string action;
    std::size_t bindingIndex = 0;
    std::string loadedKey;
    heritage::input::InputAnalogSettings draft;
    int draggedHandle = 0;
};

struct ProfileEditorState
{
    std::string selected;
    std::string pendingProfile;
    std::array<char, 64> newName{};
    std::array<char, 64> duplicateName{};
    std::array<char, 64> renameName{};
    std::string bufferProfile;
    std::string status;
    bool statusError = false;
};

ProfileEditorState& profileEditorState()
{
    static ProfileEditorState state;
    return state;
}

void setTextBuffer(std::array<char, 64>& buffer, const std::string& text)
{
    std::snprintf(buffer.data(), buffer.size(), "%s", text.c_str());
}

std::string trimUiText(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(),
        value.end(),
        [](unsigned char character) { return std::isspace(character) != 0; });
    if (first == value.end())
        return {};

    const auto last = std::find_if_not(
        value.rbegin(),
        value.rend(),
        [](unsigned char character) { return std::isspace(character) != 0; }).base();
    return std::string(first, last);
}

bool sameProfileName(const std::string& left, const std::string& right)
{
    if (left.size() != right.size())
        return false;
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        if (std::tolower(static_cast<unsigned char>(left[index]))
            != std::tolower(static_cast<unsigned char>(right[index])))
        {
            return false;
        }
    }
    return true;
}

void setProfileStatus(
    ProfileEditorState& state,
    const std::string& message,
    bool error = false)
{
    state.status = message;
    state.statusError = error;
}

AnalogEditorState& analogEditorState()
{
    static AnalogEditorState state;
    return state;
}

std::string& bindingsGroupSelection()
{
    static std::string group;
    return group;
}

std::string& analogueGroupSelection()
{
    static std::string group;
    return group;
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

void clampAnalogDraft(heritage::input::InputAnalogSettings& settings)
{
    settings.innerDeadzone = std::clamp(settings.innerDeadzone, 0.0f, 0.95f);
    settings.outerDeadzone = std::clamp(
        settings.outerDeadzone,
        0.0f,
        0.95f - settings.innerDeadzone);
    settings.sensitivity = std::clamp(settings.sensitivity, 0.1f, 3.0f);
    settings.bezierX1 = std::clamp(settings.bezierX1, 0.0f, 1.0f);
    settings.bezierY1 = std::clamp(settings.bezierY1, 0.0f, 1.0f);
    settings.bezierX2 = std::clamp(settings.bezierX2, 0.0f, 1.0f);
    settings.bezierY2 = std::clamp(settings.bezierY2, 0.0f, 1.0f);
}

std::string analogSelectionKey(
    const std::string& action,
    std::size_t bindingIndex,
    const std::string& binding)
{
    return action + "#" + std::to_string(bindingIndex) + "#" + binding;
}

const heritage::input::InputBindingInfo* findBinding(
    const std::vector<heritage::input::InputActionInfo>& actions,
    const std::string& action,
    std::size_t bindingIndex)
{
    for (const auto& candidate : actions)
    {
        if (candidate.name == action && bindingIndex < candidate.bindings.size())
            return &candidate.bindings[bindingIndex];
    }
    return nullptr;
}

bool selectFirstAnalogBinding(
    const std::vector<heritage::input::InputActionInfo>& actions,
    AnalogEditorState& editor)
{
    for (const auto& action : actions)
    {
        for (std::size_t index = 0; index < action.bindings.size(); ++index)
        {
            if (!action.bindings[index].analog)
                continue;
            editor.action = action.name;
            editor.bindingIndex = index;
            editor.loadedKey.clear();
            return true;
        }
    }
    return false;
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
        analogEditorState().loadedKey.clear();
    }

    ImGui::PopID();
}

void drawBindingsTab(heritage::input::InputSystem& input)
{
    ImGui::TextDisabled("Detected devices");
    ImGui::Separator();
    ImGui::Text("Keyboard and mouse");

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
        ImGui::Text("Action: %s", input.captureAction().c_str());
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
            ImGui::TextUnformatted(action.name.c_str());

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
                analogEditorState().loadedKey.clear();
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

void drawCurveGraph(
    heritage::input::InputSystem& input,
    AnalogEditorState& editor,
    float rawValue,
    float processedValue)
{
    const float width = std::clamp(ImGui::GetContentRegionAvail().x, 260.0f, 430.0f);
    const float height = std::clamp(width * 0.70f, 230.0f, 310.0f);
    const ImVec2 canvasSize(width, height);
    const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton(
        "AnalogueCurveCanvas",
        canvasSize,
        ImGuiButtonFlags_MouseButtonLeft);

    const bool hovered = ImGui::IsItemHovered();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        canvasOrigin,
        ImVec2(canvasOrigin.x + width, canvasOrigin.y + height),
        ImGui::GetColorU32(ImGuiCol_FrameBg));
    draw->AddRect(
        canvasOrigin,
        ImVec2(canvasOrigin.x + width, canvasOrigin.y + height),
        ImGui::GetColorU32(ImGuiCol_Border));

    for (int division = 1; division < 10; ++division)
    {
        const float fraction = static_cast<float>(division) / 10.0f;
        const ImU32 gridColor = ImGui::GetColorU32(ImGuiCol_Separator);
        draw->AddLine(
            graphPoint(canvasOrigin, canvasSize, fraction, 0.0f),
            graphPoint(canvasOrigin, canvasSize, fraction, 1.0f),
            gridColor);
        draw->AddLine(
            graphPoint(canvasOrigin, canvasSize, 0.0f, fraction),
            graphPoint(canvasOrigin, canvasSize, 1.0f, fraction),
            gridColor);
    }

    draw->AddLine(
        graphPoint(canvasOrigin, canvasSize, 0.0f, 0.0f),
        graphPoint(canvasOrigin, canvasSize, 1.0f, 1.0f),
        ImGui::GetColorU32(ImGuiCol_TextDisabled),
        1.0f);

    const float inner = editor.draft.innerDeadzone;
    const float outerStart = 1.0f - editor.draft.outerDeadzone;
    draw->AddLine(
        graphPoint(canvasOrigin, canvasSize, inner, 0.0f),
        graphPoint(canvasOrigin, canvasSize, inner, 1.0f),
        ImGui::GetColorU32(ImGuiCol_SliderGrab),
        1.0f);
    draw->AddLine(
        graphPoint(canvasOrigin, canvasSize, outerStart, 0.0f),
        graphPoint(canvasOrigin, canvasSize, outerStart, 1.0f),
        ImGui::GetColorU32(ImGuiCol_SliderGrab),
        1.0f);

    ImVec2 previous = graphPoint(canvasOrigin, canvasSize, 0.0f, 0.0f);
    for (int sample = 1; sample <= 96; ++sample)
    {
        const float x = static_cast<float>(sample) / 96.0f;
        const float y = heritage::input::InputSystem::applyAnalogProcessing(
            x,
            editor.draft);
        const ImVec2 current = graphPoint(canvasOrigin, canvasSize, x, y);
        draw->AddLine(
            previous,
            current,
            ImGui::GetColorU32(ImGuiCol_PlotLines),
            2.5f);
        previous = current;
    }

    const ImVec2 start = graphPoint(canvasOrigin, canvasSize, 0.0f, 0.0f);
    const ImVec2 end = graphPoint(canvasOrigin, canvasSize, 1.0f, 1.0f);
    const ImVec2 handle1 = graphPoint(
        canvasOrigin,
        canvasSize,
        editor.draft.bezierX1,
        editor.draft.bezierY1);
    const ImVec2 handle2 = graphPoint(
        canvasOrigin,
        canvasSize,
        editor.draft.bezierX2,
        editor.draft.bezierY2);

    draw->AddLine(start, handle1, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.5f);
    draw->AddLine(end, handle2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1.5f);
    draw->AddCircleFilled(handle1, 7.0f, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    draw->AddCircleFilled(handle2, 7.0f, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    draw->AddText(
        ImVec2(handle1.x + 9.0f, handle1.y - 8.0f),
        ImGui::GetColorU32(ImGuiCol_Text),
        "P1");
    draw->AddText(
        ImVec2(handle2.x + 9.0f, handle2.y - 8.0f),
        ImGui::GetColorU32(ImGuiCol_Text),
        "P2");

    const ImVec2 livePoint = graphPoint(
        canvasOrigin,
        canvasSize,
        rawValue,
        processedValue);
    draw->AddCircleFilled(livePoint, 5.0f, ImGui::GetColorU32(ImGuiCol_CheckMark));

    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const auto distanceSquared = [&mouse](const ImVec2& point) {
            const float x = mouse.x - point.x;
            const float y = mouse.y - point.y;
            return x * x + y * y;
        };
        const float firstDistance = distanceSquared(handle1);
        const float secondDistance = distanceSquared(handle2);
        const float radiusSquared = 18.0f * 18.0f;
        if ((std::min)(firstDistance, secondDistance) <= radiusSquared)
            editor.draggedHandle = firstDistance <= secondDistance ? 1 : 2;
    }

    if (editor.draggedHandle != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float x = std::clamp((mouse.x - canvasOrigin.x) / width, 0.0f, 1.0f);
        const float y = std::clamp(1.0f - (mouse.y - canvasOrigin.y) / height, 0.0f, 1.0f);
        if (editor.draggedHandle == 1)
        {
            editor.draft.bezierX1 = x;
            editor.draft.bezierY1 = y;
        }
        else
        {
            editor.draft.bezierX2 = x;
            editor.draft.bezierY2 = y;
        }
        clampAnalogDraft(editor.draft);
        input.setBindingAnalogSettings(
            editor.action,
            editor.bindingIndex,
            editor.draft,
            false);
    }

    if (editor.draggedHandle != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        editor.draggedHandle = 0;
        input.save();
    }

    if (hovered)
        ImGui::SetTooltip("Drag P1 or P2. The green marker shows live raw input and output.");
}

void applyPreset(
    heritage::input::InputSystem& input,
    AnalogEditorState& editor,
    float x1,
    float y1,
    float x2,
    float y2)
{
    editor.draft.bezierX1 = x1;
    editor.draft.bezierY1 = y1;
    editor.draft.bezierX2 = x2;
    editor.draft.bezierY2 = y2;
    clampAnalogDraft(editor.draft);
    input.setBindingAnalogSettings(
        editor.action,
        editor.bindingIndex,
        editor.draft,
        true);
}

void drawAnalogueTab(heritage::input::InputSystem& input)
{
    const auto allActions = input.actions();
    std::string& selectedGroup = analogueGroupSelection();
    drawActionGroupTabs(input, "AnalogueActionGroups", selectedGroup);
    auto actions = actionsInGroup(allActions, selectedGroup);
    AnalogEditorState& editor = analogEditorState();

    const heritage::input::InputBindingInfo* selected =
        findBinding(actions, editor.action, editor.bindingIndex);
    if (!selected || !selected->analog)
    {
        if (!selectFirstAnalogBinding(actions, editor))
        {
            ImGui::TextWrapped(
                "No analogue axis is currently bound. Add a gamepad stick, trigger, pedal, "
                "wheel, or another analogue control in the Bindings tab first.");
            return;
        }
        selected = findBinding(actions, editor.action, editor.bindingIndex);
    }

    std::string preview = editor.action
        + " / Binding " + std::to_string(editor.bindingIndex + 1);
    if (selected)
        preview += " / " + selected->displayName;

    if (ImGui::BeginCombo("Analogue binding", preview.c_str()))
    {
        for (const auto& action : actions)
        {
            for (std::size_t index = 0; index < action.bindings.size(); ++index)
            {
                const auto& binding = action.bindings[index];
                if (!binding.analog)
                    continue;

                const std::string label = action.name
                    + " / Binding " + std::to_string(index + 1)
                    + " / " + binding.displayName;
                const bool isSelected = editor.action == action.name
                    && editor.bindingIndex == index;
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    editor.action = action.name;
                    editor.bindingIndex = index;
                    editor.loadedKey.clear();
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    actions = actionsInGroup(input.actions(), selectedGroup);
    selected = findBinding(actions, editor.action, editor.bindingIndex);
    if (!selected || !selected->analog)
        return;

    const std::string key = analogSelectionKey(
        editor.action,
        editor.bindingIndex,
        selected->binding);
    if (editor.loadedKey != key)
    {
        editor.loadedKey = key;
        editor.draft = selected->analogSettings;
        editor.draggedHandle = 0;
    }

    ImGui::TextDisabled("Per-binding analogue processing");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Every analogue binding keeps its own calibration and response curve. A DualSense "
        "stick can therefore use a precision curve while a steering wheel remains linear.");

    const float rawValue = input.bindingRawValue(editor.action, editor.bindingIndex);
    const float processedValue = input.bindingValue(editor.action, editor.bindingIndex);
    drawCurveGraph(input, editor, rawValue, processedValue);

    ImGui::Text("Raw input: %.3f", rawValue);
    ImGui::ProgressBar(rawValue, ImVec2(-1.0f, 0.0f));
    ImGui::Text("Processed output: %.3f", processedValue);
    ImGui::ProgressBar(processedValue, ImVec2(-1.0f, 0.0f));

    bool changed = false;
    if (ImGui::Checkbox("Invert axis", &editor.draft.invert))
    {
        changed = true;
        input.setBindingAnalogSettings(
            editor.action,
            editor.bindingIndex,
            editor.draft,
            true);
    }

    if (ImGui::SliderFloat(
        "Inner deadzone",
        &editor.draft.innerDeadzone,
        0.0f,
        0.50f,
        "%.3f",
        ImGuiSliderFlags_AlwaysClamp))
    {
        clampAnalogDraft(editor.draft);
        input.setBindingAnalogSettings(
            editor.action,
            editor.bindingIndex,
            editor.draft,
            false);
        changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
        input.save();

    if (ImGui::SliderFloat(
        "Outer deadzone / saturation",
        &editor.draft.outerDeadzone,
        0.0f,
        0.50f,
        "%.3f",
        ImGuiSliderFlags_AlwaysClamp))
    {
        clampAnalogDraft(editor.draft);
        input.setBindingAnalogSettings(
            editor.action,
            editor.bindingIndex,
            editor.draft,
            false);
        changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
        input.save();

    if (ImGui::SliderFloat(
        "Sensitivity",
        &editor.draft.sensitivity,
        0.10f,
        3.00f,
        "%.3f",
        ImGuiSliderFlags_AlwaysClamp))
    {
        clampAnalogDraft(editor.draft);
        input.setBindingAnalogSettings(
            editor.action,
            editor.bindingIndex,
            editor.draft,
            false);
        changed = true;
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
        input.save();

    ImGui::Spacing();
    ImGui::TextDisabled("Cubic Bezier response curve");
    ImGui::TextWrapped(
        "Drag the P1/P2 handles above, or click a number and type an exact value.");

    if (ImGui::BeginTable("BezierNumericValues", 4, ImGuiTableFlags_SizingStretchSame))
    {
        float* values[4] = {
            &editor.draft.bezierX1,
            &editor.draft.bezierY1,
            &editor.draft.bezierX2,
            &editor.draft.bezierY2
        };
        const char* labels[4] = { "X1", "Y1", "X2", "Y2" };
        for (int index = 0; index < 4; ++index)
        {
            ImGui::TableNextColumn();
            ImGui::PushID(index);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputFloat(
                labels[index],
                values[index],
                0.01f,
                0.10f,
                "%.3f",
                ImGuiInputTextFlags_CharsDecimal))
            {
                clampAnalogDraft(editor.draft);
                input.setBindingAnalogSettings(
                    editor.action,
                    editor.bindingIndex,
                    editor.draft,
                    false);
                changed = true;
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
                input.save();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::TextDisabled("Presets");
    if (ImGui::Button("Linear"))
        applyPreset(input, editor, 0.0f, 0.0f, 1.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Precision"))
        applyPreset(input, editor, 0.70f, 0.28f, 1.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Ease In"))
        applyPreset(input, editor, 0.42f, 0.0f, 1.0f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("Ease Out"))
        applyPreset(input, editor, 0.0f, 0.0f, 0.58f, 1.0f);

    if (ImGui::Button("Ease In-Out"))
        applyPreset(input, editor, 0.42f, 0.0f, 0.58f, 1.0f);
    ImGui::SameLine();
    if (ImGui::Button("RESET THIS BINDING"))
    {
        input.resetBindingAnalogSettings(editor.action, editor.bindingIndex);
        editor.loadedKey.clear();
    }

    if (changed)
        ImGui::TextDisabled("Live preview updated. Values save when editing finishes.");

    ImGui::Spacing();
    ImGui::TextDisabled(
        "Processing order: raw input -> inversion -> deadzones -> sensitivity -> Bezier curve.");
}


void drawProfilesTab(heritage::input::InputSystem& input)
{
    ProfileEditorState& editor = profileEditorState();
    std::vector<heritage::input::InputProfileInfo> profiles = input.profiles();

    const auto selectedIterator = std::find_if(
        profiles.begin(),
        profiles.end(),
        [&editor](const heritage::input::InputProfileInfo& profile) {
            return sameProfileName(profile.name, editor.selected);
        });
    if (selectedIterator == profiles.end())
    {
        editor.selected = profiles.empty() ? std::string{} : profiles.front().name;
        editor.bufferProfile.clear();
    }

    if (editor.bufferProfile != editor.selected)
    {
        editor.bufferProfile = editor.selected;
        setTextBuffer(editor.renameName, editor.selected);
        setTextBuffer(
            editor.duplicateName,
            editor.selected.empty() ? std::string{} : editor.selected + " Copy");
    }

    ImGui::TextDisabled("Named input profiles");
    ImGui::Separator();
    ImGui::TextWrapped(
        "Your working bindings and analogue curves continue to autosave normally. "
        "A named profile is a separate snapshot and changes only when you deliberately "
        "create or update it.");

    if (profiles.empty())
    {
        ImGui::TextDisabled("No named profiles have been saved yet.");
    }
    else
    {
        const char* preview = editor.selected.empty()
            ? "Select profile"
            : editor.selected.c_str();
        if (ImGui::BeginCombo("Saved profile", preview))
        {
            for (const auto& profile : profiles)
            {
                const bool selected =
                    sameProfileName(profile.name, editor.selected);
                if (ImGui::Selectable(profile.name.c_str(), selected))
                {
                    editor.selected = profile.name;
                    editor.bufferProfile.clear();
                    editor.status.clear();
                }
                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    if (!input.lastAppliedProfile().empty())
    {
        ImGui::Text("Last saved/restored snapshot: %s",
            input.lastAppliedProfile().c_str());
        if (input.profileDirty())
        {
            ImGui::TextDisabled(
                "Working settings have changed since that snapshot. Apply it to restore, "
                "or Update Snapshot to keep the new settings.");
        }
        else
        {
            ImGui::TextDisabled(
                "The working settings still match that saved snapshot.");
        }
    }
    else
    {
        ImGui::TextDisabled(
            "No profile snapshot has been saved or restored during this configuration yet.");
    }

    const bool hasSelection = !editor.selected.empty();
    ImGui::BeginDisabled(!hasSelection);
    if (ImGui::Button("APPLY / RESTORE SELECTED"))
    {
        editor.pendingProfile = editor.selected;
        ImGui::OpenPopup("ApplyInputProfileConfirmation");
    }
    ImGui::SameLine();
    if (ImGui::Button("UPDATE SNAPSHOT"))
    {
        editor.pendingProfile = editor.selected;
        ImGui::OpenPopup("UpdateInputProfileConfirmation");
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal(
        "ApplyInputProfileConfirmation",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "Restore '%s'? The current working bindings and analogue settings will be "
            "replaced by this snapshot.",
            editor.pendingProfile.c_str());
        if (ImGui::Button("RESTORE PROFILE", ImVec2(180.0f, 0.0f)))
        {
            if (input.applyProfile(editor.pendingProfile))
            {
                editor.selected = input.lastAppliedProfile();
                editor.bufferProfile.clear();
                analogEditorState().loadedKey.clear();
                setProfileStatus(editor, "Profile restored successfully.");
            }
            else
            {
                setProfileStatus(editor, input.lastError(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal(
        "UpdateInputProfileConfirmation",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "Overwrite '%s' with the complete current setup? This updates all eight "
            "binding slots and every analogue curve stored in the snapshot.",
            editor.pendingProfile.c_str());
        if (ImGui::Button("UPDATE SNAPSHOT", ImVec2(180.0f, 0.0f)))
        {
            if (input.updateProfile(editor.pendingProfile))
            {
                editor.selected = input.lastAppliedProfile();
                editor.bufferProfile.clear();
                setProfileStatus(editor, "Profile snapshot updated.");
            }
            else
            {
                setProfileStatus(editor, input.lastError(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Create a snapshot from the current setup");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText(
        "##NewInputProfileName",
        editor.newName.data(),
        editor.newName.size());
    if (ImGui::Button("SAVE CURRENT AS NEW PROFILE"))
    {
        const std::string requested = trimUiText(editor.newName.data());
        if (input.createProfile(requested))
        {
            editor.selected = input.lastAppliedProfile();
            editor.bufferProfile.clear();
            editor.newName.fill('\0');
            setProfileStatus(editor, "New profile snapshot saved.");
        }
        else
        {
            setProfileStatus(editor, input.lastError(), true);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Profile management");

    ImGui::BeginDisabled(!hasSelection);
    ImGui::TextDisabled("Duplicate as");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText(
        "##DuplicateInputProfileName",
        editor.duplicateName.data(),
        editor.duplicateName.size());
    if (ImGui::Button("DUPLICATE SELECTED"))
    {
        const std::string requested = trimUiText(editor.duplicateName.data());
        if (input.duplicateProfile(editor.selected, requested))
        {
            editor.selected = requested;
            editor.bufferProfile.clear();
            setProfileStatus(editor, "Profile duplicated.");
        }
        else
        {
            setProfileStatus(editor, input.lastError(), true);
        }
    }

    ImGui::TextDisabled("Rename to");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText(
        "##RenameInputProfileName",
        editor.renameName.data(),
        editor.renameName.size());
    if (ImGui::Button("RENAME SELECTED"))
    {
        const std::string requested = trimUiText(editor.renameName.data());
        if (input.renameProfile(editor.selected, requested))
        {
            editor.selected = requested;
            editor.bufferProfile.clear();
            setProfileStatus(editor, "Profile renamed.");
        }
        else
        {
            setProfileStatus(editor, input.lastError(), true);
        }
    }

    if (ImGui::Button("DELETE SELECTED PROFILE"))
    {
        editor.pendingProfile = editor.selected;
        ImGui::OpenPopup("DeleteInputProfileConfirmation");
    }
    ImGui::EndDisabled();

    if (ImGui::BeginPopupModal(
        "DeleteInputProfileConfirmation",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped(
            "Permanently delete the profile snapshot '%s'? The current working settings "
            "will not be changed.",
            editor.pendingProfile.c_str());
        if (ImGui::Button("DELETE PROFILE", ImVec2(170.0f, 0.0f)))
        {
            if (input.deleteProfile(editor.pendingProfile))
            {
                editor.selected.clear();
                editor.bufferProfile.clear();
                setProfileStatus(editor, "Profile deleted.");
            }
            else
            {
                setProfileStatus(editor, input.lastError(), true);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("CANCEL", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (!editor.status.empty())
    {
        ImGui::Spacing();
        if (editor.statusError)
        {
            ImGui::TextColored(
                ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                "%s",
                editor.status.c_str());
        }
        else
        {
            ImGui::TextColored(
                ImGui::GetStyleColorVec4(ImGuiCol_CheckMark),
                "%s",
                editor.status.c_str());
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Profile snapshots are stored per module:");
    ImGui::TextWrapped("%s", input.profilesDirectory().string().c_str());
}

} // namespace

void drawInputSettingsPage(heritage::input::InputSystem& input)
{
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
