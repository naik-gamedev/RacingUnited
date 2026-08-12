#include "InputSettingsPageInternal.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>

#include <imgui.h>

namespace heritage::ui::settings::input_settings_internal {
namespace {

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

} // namespace

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
                invalidateAnalogueEditorSelection();
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

} // namespace heritage::ui::settings::input_settings_internal
