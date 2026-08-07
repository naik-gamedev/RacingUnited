#include "ScriptedUiRuntime.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <imgui.h>

namespace heritage::modules {
namespace {

bool startsWith(const std::string& value, const std::string& prefix)
{
    return value.size() >= prefix.size()
        && value.compare(0, prefix.size(), prefix) == 0;
}

std::pair<std::string, std::string> splitOnce(
    const std::string& value,
    char delimiter)
{
    const std::size_t position = value.find(delimiter);
    if (position == std::string::npos)
        return { value, {} };

    return {
        value.substr(0, position),
        value.substr(position + 1)
    };
}

} // namespace

bool ScriptedUiRuntime::onLoad(
    GLFWwindow*,
    const ModuleContext& context,
    const ModuleRuntimeServices&,
    std::string& message)
{
    onShutdown();

    m_moduleName = context.module().name.empty()
        ? context.module().id
        : context.module().name;

    if (context.module().entryUi.empty())
    {
        message = "Module '" + context.module().id
            + "' requested runtime 'scripted_ui' but has no entry_ui.";
        return false;
    }

    const auto uiPath = context.resolveUiPath(context.module().entryUi);
    if (uiPath.empty())
    {
        message = "Module '" + context.module().id
            + "' has an unsafe entry_ui path: " + context.module().entryUi;
        return false;
    }

    m_documentPath = uiPath.string();
    if (!loadDocument(m_documentPath, message))
        return false;

    m_loaded = true;
    m_started = false;
    message.clear();
    return true;
}

void ScriptedUiRuntime::onStart()
{
    if (!m_loaded)
        return;

    m_currentScreen = m_startScreen;
    m_started = true;
}

void ScriptedUiRuntime::onUpdate(float, bool allowInteraction)
{
    m_allowInteraction = allowInteraction;
}

heritage::math::Vec3 ScriptedUiRuntime::clearColor() const
{
    return m_clearColor;
}

void ScriptedUiRuntime::onRender(
    const heritage::math::Mat4&,
    const heritage::settings::VideoSettings&) const
{
    // This first scripted runtime owns UI and logic only. A later step will
    // allow a scripted module to select and control a live 3D background scene.
}

void ScriptedUiRuntime::onDrawUI(
    int framebufferWidth,
    int framebufferHeight)
{
    if (!m_started)
        return;

    const auto found = m_screens.find(m_currentScreen);
    if (found == m_screens.end())
        return;

    const ScreenDefinition& screen = found->second;

    const float availableWidth = static_cast<float>((std::max)(framebufferWidth, 1));
    const float availableHeight = static_cast<float>((std::max)(framebufferHeight, 1));
    const float panelWidth = (std::max)(160.0f,
        (std::min)(620.0f, availableWidth - 32.0f));
    const float panelHeight = (std::max)(120.0f,
        (std::min)(520.0f, availableHeight - 48.0f));

    ImGui::SetNextWindowPos(
        ImVec2(availableWidth * 0.5f, availableHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.040f, 0.048f, 0.96f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##heritage_module_ui", nullptr, flags))
    {
        ImGui::TextDisabled("%s", m_moduleName.c_str());
        ImGui::Separator();
        ImGui::Spacing();

        if (!screen.title.empty())
        {
            ImGui::SetWindowFontScale(1.35f);
            ImGui::TextUnformatted(screen.title.c_str());
            ImGui::SetWindowFontScale(1.0f);
        }

        if (!screen.subtitle.empty())
            ImGui::TextDisabled("%s", screen.subtitle.c_str());

        if (!screen.textLines.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            for (const std::string& line : screen.textLines)
                ImGui::TextWrapped("%s", line.c_str());
        }

        if (!screen.buttons.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float buttonWidth = (std::min)(360.0f, ImGui::GetContentRegionAvail().x);
            for (std::size_t i = 0; i < screen.buttons.size(); ++i)
            {
                const ButtonDefinition& button = screen.buttons[i];
                ImGui::PushID(static_cast<int>(i));

                const float cursorX = ImGui::GetCursorPosX()
                    + (ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f;
                ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), cursorX));

                ImGui::BeginDisabled(!m_allowInteraction);
                if (ImGui::Button(button.label.c_str(), ImVec2(buttonWidth, 38.0f)))
                    executeAction(button.action);
                ImGui::EndDisabled();

                ImGui::PopID();
            }
        }
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (m_openMessagePopup)
    {
        ImGui::OpenPopup("Module Message");
        m_openMessagePopup = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
        "Module Message",
        nullptr,
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
    {
        if (!m_messageTitle.empty())
        {
            ImGui::TextUnformatted(m_messageTitle.c_str());
            ImGui::Separator();
            ImGui::Spacing();
        }

        ImGui::TextWrapped("%s", m_messageText.c_str());
        ImGui::Spacing();
        if (ImGui::Button("OK", ImVec2(120.0f, 0.0f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void ScriptedUiRuntime::onShutdown()
{
    m_screens.clear();
    m_screenOrder.clear();
    m_actions.clear();
    m_moduleName.clear();
    m_documentPath.clear();
    m_startScreen.clear();
    m_currentScreen.clear();
    m_messageTitle.clear();
    m_messageText.clear();
    m_clearColor = { 0.005f, 0.007f, 0.010f };
    m_loaded = false;
    m_started = false;
    m_allowInteraction = true;
    m_openMessagePopup = false;
}

bool ScriptedUiRuntime::pollAction(ModuleRuntimeAction& action)
{
    if (m_actions.empty())
        return false;

    action = std::move(m_actions.front());
    m_actions.pop_front();
    return true;
}

std::string ScriptedUiRuntime::activeContentId() const
{
    if (m_documentPath.empty())
        return "<none>";

    return m_documentPath + "#" + m_currentScreen;
}

bool ScriptedUiRuntime::loadDocument(
    const std::string& path,
    std::string& errorMessage)
{
    std::ifstream file(path);
    if (!file)
    {
        errorMessage = "Could not open module UI document:\n" + path;
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseDocument(buffer.str(), errorMessage);
}

bool ScriptedUiRuntime::parseDocument(
    const std::string& source,
    std::string& errorMessage)
{
    std::istringstream input(source);
    std::string line;
    ScreenDefinition* currentScreen = nullptr;
    int lineNumber = 0;

    while (std::getline(input, line))
    {
        ++lineNumber;
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        if (line.front() == '[' && line.back() == ']')
        {
            const std::string section = trim(line.substr(1, line.size() - 2));
            const std::string prefix = "screen ";
            if (!startsWith(section, prefix))
            {
                errorMessage = "Unknown UI section on line "
                    + std::to_string(lineNumber) + ": " + section;
                return false;
            }

            const std::string screenId = trim(section.substr(prefix.size()));
            if (screenId.empty())
            {
                errorMessage = "Empty screen id on line " + std::to_string(lineNumber);
                return false;
            }

            ScreenDefinition screen;
            screen.id = screenId;
            auto inserted = m_screens.emplace(screenId, std::move(screen));
            if (!inserted.second)
            {
                errorMessage = "Duplicate screen '" + screenId + "'.";
                return false;
            }

            m_screenOrder.push_back(screenId);
            currentScreen = &inserted.first->second;
            continue;
        }

        const auto pair = splitOnce(line, '=');
        const std::string key = trim(pair.first);
        const std::string value = trim(pair.second);
        if (pair.second.empty() && line.find('=') == std::string::npos)
        {
            errorMessage = "Expected key = value on line "
                + std::to_string(lineNumber) + ".";
            return false;
        }

        if (!currentScreen)
        {
            if (key == "start_screen")
                m_startScreen = value;
            else if (key == "clear_color")
            {
                if (!parseColor(value, m_clearColor))
                {
                    errorMessage = "Invalid clear_color on line "
                        + std::to_string(lineNumber) + ". Use r,g,b from 0 to 1.";
                    return false;
                }
            }
            else
            {
                errorMessage = "Unknown document property on line "
                    + std::to_string(lineNumber) + ": " + key;
                return false;
            }
            continue;
        }

        if (key == "title")
            currentScreen->title = value;
        else if (key == "subtitle")
            currentScreen->subtitle = value;
        else if (key == "text")
            currentScreen->textLines.push_back(value);
        else if (key == "button")
        {
            const auto buttonPair = splitOnce(value, '|');
            ButtonDefinition button;
            button.label = trim(buttonPair.first);
            button.action = trim(buttonPair.second);
            if (button.label.empty() || button.action.empty())
            {
                errorMessage = "Invalid button on line "
                    + std::to_string(lineNumber)
                    + ". Use button = Label|action.";
                return false;
            }
            currentScreen->buttons.push_back(std::move(button));
        }
        else
        {
            errorMessage = "Unknown screen property on line "
                + std::to_string(lineNumber) + ": " + key;
            return false;
        }
    }

    if (m_screens.empty())
    {
        errorMessage = "The module UI document contains no [screen Name] sections.";
        return false;
    }

    if (m_startScreen.empty())
        m_startScreen = m_screenOrder.front();

    if (m_screens.find(m_startScreen) == m_screens.end())
    {
        errorMessage = "start_screen refers to unknown screen '"
            + m_startScreen + "'.";
        return false;
    }

    errorMessage.clear();
    return true;
}

void ScriptedUiRuntime::executeAction(const std::string& action)
{
    if (startsWith(action, "screen:"))
    {
        const std::string screenId = trim(action.substr(7));
        if (m_screens.find(screenId) != m_screens.end())
            m_currentScreen = screenId;
        else
        {
            m_messageTitle = "Module UI Error";
            m_messageText = "Unknown screen: " + screenId;
            m_openMessagePopup = true;
        }
        return;
    }

    if (startsWith(action, "message:"))
    {
        m_messageTitle = m_moduleName;
        m_messageText = trim(action.substr(8));
        m_openMessagePopup = true;
        return;
    }

    if (action == "engine_settings")
    {
        queueAction(ModuleRuntimeActionType::OpenEngineSettings);
        return;
    }

    if (action == "exit")
    {
        queueAction(ModuleRuntimeActionType::ExitApplication);
        return;
    }

    m_messageTitle = "Module UI Error";
    m_messageText = "Unknown action: " + action;
    m_openMessagePopup = true;
}

void ScriptedUiRuntime::queueAction(
    ModuleRuntimeActionType type,
    const std::string& payload)
{
    ModuleRuntimeAction action;
    action.type = type;
    action.payload = payload;
    m_actions.push_back(std::move(action));
}

std::string ScriptedUiRuntime::trim(const std::string& value)
{
    const auto notSpace = [](unsigned char character) {
        return !std::isspace(character);
    };

    auto begin = std::find_if(value.begin(), value.end(), notSpace);
    auto end = std::find_if(value.rbegin(), value.rend(), notSpace).base();
    if (begin >= end)
        return {};

    return std::string(begin, end);
}

bool ScriptedUiRuntime::parseColor(
    const std::string& value,
    heritage::math::Vec3& color)
{
    std::istringstream stream(value);
    std::string component;
    float values[3] = {};

    for (int index = 0; index < 3; ++index)
    {
        if (!std::getline(stream, component, ','))
            return false;

        try
        {
            values[index] = std::stof(trim(component));
        }
        catch (...)
        {
            return false;
        }

        if (values[index] < 0.0f || values[index] > 1.0f)
            return false;
    }

    if (std::getline(stream, component, ','))
        return false;

    color = { values[0], values[1], values[2] };
    return true;
}

} // namespace heritage::modules
