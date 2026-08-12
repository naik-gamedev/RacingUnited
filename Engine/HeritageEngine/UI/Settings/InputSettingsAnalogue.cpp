#include "InputSettingsPageInternal.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include <imgui.h>

namespace heritage::ui::settings::input_settings_internal {
namespace {

struct AnalogEditorState
{
    std::string action;
    std::size_t bindingIndex = 0;
    std::string loadedKey;
    heritage::input::InputAnalogSettings draft;
    int draggedHandle = 0;
};

AnalogEditorState& analogEditorState()
{
    static AnalogEditorState state;
    return state;
}

std::string& analogueGroupSelection()
{
    static std::string group;
    return group;
}

ImVec2 graphPoint(const ImVec2& origin, const ImVec2& size, float x, float y)
{
    return ImVec2(
        origin.x + std::clamp(x, 0.0f, 1.0f) * size.x,
        origin.y + (1.0f - std::clamp(y, 0.0f, 1.0f)) * size.y);
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


} // namespace

void invalidateAnalogueEditorSelection()
{
    analogEditorState().loadedKey.clear();
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

    std::string preview = displayActionName(editor.action)
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

                const std::string label = displayActionName(action.name)
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



} // namespace heritage::ui::settings::input_settings_internal
