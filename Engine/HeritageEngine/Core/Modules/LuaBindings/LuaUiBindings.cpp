#include "../LuaModuleRuntime.hpp"
#include "LuaCoreBindingHandlers.hpp"
#include "LuaBindingInternals.hpp"
#include "../../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <cfloat>
#include <cstring>
#include <GLFW/glfw3.h>
#include <imgui.h>

namespace heritage::modules {
using namespace lua_binding_detail;

namespace {

enum class UiAlignment
{
    Left,
    Center,
    Right
};

UiAlignment parseAlignment(const std::string& text)
{
    std::string normalized = text;
    std::transform(
        normalized.begin(),
        normalized.end(),
        normalized.begin(),
        [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });

    if (normalized == "center" || normalized == "centre")
        return UiAlignment::Center;
    if (normalized == "right")
        return UiAlignment::Right;
    return UiAlignment::Left;
}

ImVec2 calculateImageSize(
    int sourceWidth,
    int sourceHeight,
    float requestedWidth,
    float requestedHeight,
    float maximumWidth)
{
    const float safeSourceWidth = static_cast<float>((std::max)(sourceWidth, 1));
    const float safeSourceHeight = static_cast<float>((std::max)(sourceHeight, 1));
    const float aspect = safeSourceWidth / safeSourceHeight;

    float width = requestedWidth;
    float height = requestedHeight;

    if (width <= 0.0f && height <= 0.0f)
    {
        width = safeSourceWidth;
        height = safeSourceHeight;
    }
    else if (width > 0.0f && height <= 0.0f)
    {
        height = width / aspect;
    }
    else if (width <= 0.0f && height > 0.0f)
    {
        width = height * aspect;
    }

    width = (std::max)(1.0f, width);
    height = (std::max)(1.0f, height);

    const float safeMaximumWidth = (std::max)(1.0f, maximumWidth);
    if (width > safeMaximumWidth)
    {
        const float scale = safeMaximumWidth / width;
        width *= scale;
        height *= scale;
    }

    return ImVec2(width, height);
}

void alignNextItem(UiAlignment alignment, float itemWidth)
{
    if (alignment == UiAlignment::Left)
        return;

    const float available = ImGui::GetContentRegionAvail().x;
    const float offset = alignment == UiAlignment::Center
        ? (available - itemWidth) * 0.5f
        : available - itemWidth;

    if (offset > 0.0f)
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
}

} // namespace

int LuaCoreBindingHandlers::luaUiBeginPanel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->closeOpenPanel();

    const std::string panelId = LuaModuleRuntime::stringArgument(
        *runtime, state, 1, "LuaModulePanel");
    const float availableWidth = static_cast<float>(runtime->m_framebufferWidth);
    const float availableHeight = static_cast<float>(runtime->m_framebufferHeight);
    const float requestedWidth = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 620.0));
    const float requestedHeight = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 520.0));

    const float maximumPanelWidth = (std::max)(120.0f, availableWidth - 32.0f);
    const float maximumPanelHeight = (std::max)(100.0f, availableHeight - 48.0f);
    const float panelWidth = (std::min)((std::max)(180.0f, requestedWidth), maximumPanelWidth);
    const float panelHeight = (std::min)((std::max)(140.0f, requestedHeight), maximumPanelHeight);

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

    const std::string windowName = "##lua_panel_" + panelId;
    runtime->m_panelVisible = ImGui::Begin(windowName.c_str(), nullptr, flags);
    runtime->m_panelOpen = true;
    runtime->m_api.lua_pushboolean(state, runtime->m_panelVisible ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaUiEndPanel(lua_State* state)
{
    if (LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state))
        runtime->closeOpenPanel();
    return 0;
}

int LuaCoreBindingHandlers::luaUiBeginTabBar(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string id = LuaModuleRuntime::stringArgument(*runtime, state, 1, "LuaTabs");
    const bool open = ImGui::BeginTabBar(id.c_str());
    if (open)
        runtime->m_uiScopes.push_back(LuaModuleRuntime::UiScopeType::TabBar);
    runtime->m_api.lua_pushboolean(state, open ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaUiEndTabBar(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_uiScopes.empty()
        && runtime->m_uiScopes.back() == LuaModuleRuntime::UiScopeType::TabBar)
    {
        ImGui::EndTabBar();
        runtime->m_uiScopes.pop_back();
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiBeginTabItem(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Tab");
    const bool open = ImGui::BeginTabItem(label.c_str());
    if (open)
        runtime->m_uiScopes.push_back(LuaModuleRuntime::UiScopeType::TabItem);
    runtime->m_api.lua_pushboolean(state, open ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaUiEndTabItem(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    if (!runtime->m_uiScopes.empty()
        && runtime->m_uiScopes.back() == LuaModuleRuntime::UiScopeType::TabItem)
    {
        ImGui::EndTabItem();
        runtime->m_uiScopes.pop_back();
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiModuleLabel(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime && runtime->m_context)
    {
        const auto& module = runtime->m_context->module();
        const std::string label = module.name.empty() ? module.id : module.name;
        ImGui::TextDisabled("%s", label.c_str());
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiTitle(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    ImGui::SetWindowFontScale(1.35f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::SetWindowFontScale(1.0f);
    return 0;
}

int LuaCoreBindingHandlers::luaUiSubtitle(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime)
    {
        const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
        ImGui::TextDisabled("%s", text.c_str());
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiText(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime)
        ImGui::TextUnformatted(LuaModuleRuntime::stringArgument(*runtime, state, 1).c_str());
    return 0;
}

int LuaCoreBindingHandlers::luaUiTextWrapped(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime)
    {
        const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
        ImGui::TextWrapped("%s", text.c_str());
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiTextDisabled(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (runtime)
    {
        const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
        ImGui::TextDisabled("%s", text.c_str());
    }
    return 0;
}

int LuaCoreBindingHandlers::luaUiSeparator(lua_State*)
{
    ImGui::Separator();
    return 0;
}

int LuaCoreBindingHandlers::luaUiSpacing(lua_State*)
{
    ImGui::Spacing();
    return 0;
}

int LuaCoreBindingHandlers::luaUiSameLine(lua_State*)
{
    ImGui::SameLine();
    return 0;
}

int LuaCoreBindingHandlers::luaUiGetAvailableWidth(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>((std::max)(0.0f, ImGui::GetContentRegionAvail().x)));
    return 1;
}

int LuaCoreBindingHandlers::luaUiButton(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "BUTTON");
    const float defaultWidth = (std::min)(
        360.0f,
        (std::max)(100.0f, ImGui::GetContentRegionAvail().x));
    const float availableWidth = (std::max)(80.0f, ImGui::GetContentRegionAvail().x);
    const float requestedWidth = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, defaultWidth));
    const float width = (std::min)((std::max)(80.0f, requestedWidth), availableWidth);
    const float height = (std::max)(22.0f, static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 38.0)));
    const bool centered = LuaModuleRuntime::booleanArgument(*runtime, state, 4, true);

    if (centered)
    {
        const float cursorX = ImGui::GetCursorPosX()
            + (ImGui::GetContentRegionAvail().x - width) * 0.5f;
        ImGui::SetCursorPosX((std::max)(ImGui::GetCursorPosX(), cursorX));
    }

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool clicked = ImGui::Button(label.c_str(), ImVec2(width, height));
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, clicked ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaUiSliderFloat(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Value");
    float value = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0));
    const float minimum = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0));
    const float maximum = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0));
    const std::string format = LuaModuleRuntime::stringArgument(*runtime, state, 5, "%.2f");

    const float originalValue = value;
    bool changed = false;

    ImGui::BeginDisabled(!runtime->m_allowInteraction);

    const bool numericInputMode = runtime->m_numericSliderInputLabel == label;
    if (numericInputMode)
    {
        if (runtime->m_numericSliderFocusRequested)
        {
            ImGui::SetKeyboardFocusHere();
            runtime->m_numericSliderFocusRequested = false;
        }

        const bool submitted = ImGui::InputFloat(
            label.c_str(),
            &value,
            0.0f,
            0.0f,
            format.c_str(),
            ImGuiInputTextFlags_EnterReturnsTrue
                | ImGuiInputTextFlags_AutoSelectAll);
        value = clampFloat(value, minimum, maximum);
        changed = std::abs(value - originalValue) > 0.0000001f;

        // Enter commits immediately. Clicking elsewhere leaves text-input mode
        // after the field loses focus. A double-click on any slider activates
        // this mode without requiring Ctrl or another modifier key.
        if (submitted || (!ImGui::IsItemActive() && !ImGui::IsItemHovered()))
            runtime->m_numericSliderInputLabel.clear();
    }
    else
    {
        changed = ImGui::SliderFloat(
            label.c_str(), &value, minimum, maximum, format.c_str());

        if (runtime->m_allowInteraction
            && ImGui::IsItemHovered()
            && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            runtime->m_numericSliderInputLabel = label;
            runtime->m_numericSliderFocusRequested = true;
        }
    }

    ImGui::EndDisabled();

    runtime->m_api.lua_pushnumber(state, value);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaCoreBindingHandlers::luaUiInputFloat(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Value");
    float value = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0));
    float minimum = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, -1000000.0));
    float maximum = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1000000.0));
    const float step = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.01));
    const float stepFast = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.1));
    const std::string format = LuaModuleRuntime::stringArgument(*runtime, state, 7, "%.3f");

    if (minimum > maximum)
        std::swap(minimum, maximum);

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::InputFloat(
        label.c_str(), &value, step, stepFast, format.c_str());
    ImGui::EndDisabled();

    value = clampFloat(value, minimum, maximum);
    runtime->m_api.lua_pushnumber(state, value);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaCoreBindingHandlers::luaUiCheckbox(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Enabled");
    bool value = LuaModuleRuntime::booleanArgument(*runtime, state, 2, false);

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::Checkbox(label.c_str(), &value);
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaCoreBindingHandlers::luaUiInputInt(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Value");
    int validInteger = 0;
    int value = static_cast<int>(runtime->m_api.lua_tointegerx(state, 2, &validInteger));
    if (!validInteger)
        value = 0;
    const int step = static_cast<int>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::InputInt(label.c_str(), &value, step);
    ImGui::EndDisabled();

    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(value));
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaCoreBindingHandlers::luaUiInputText(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Text");
    const std::string current = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const std::size_t requestedCapacity = static_cast<std::size_t>((std::max)(
        32.0,
        (std::min)(4096.0, LuaModuleRuntime::numberArgument(*runtime, state, 3, 512.0))));
    std::vector<char> buffer(requestedCapacity + 1, '\0');
    const std::size_t copied = (std::min)(current.size(), requestedCapacity);
    std::memcpy(buffer.data(), current.data(), copied);

    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool changed = ImGui::InputText(
        label.c_str(), buffer.data(), buffer.size());
    ImGui::EndDisabled();

    const std::size_t length = std::strlen(buffer.data());
    runtime->m_api.lua_pushlstring(state, buffer.data(), length);
    runtime->m_api.lua_pushboolean(state, changed ? 1 : 0);
    return 2;
}

int LuaCoreBindingHandlers::luaUiImage(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const float requestedWidth = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0));
    const float requestedHeight = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0));
    const UiAlignment alignment = parseAlignment(
        LuaModuleRuntime::stringArgument(*runtime, state, 4, "left"));
    const float alpha = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.0)), 0.0f, 1.0f);

    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
            "UI IMAGE ERROR: %s",
            runtime->m_lastUiError.c_str());
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    runtime->m_lastUiError.clear();
    const ImVec2 size = calculateImageSize(
        image->width,
        image->height,
        requestedWidth,
        requestedHeight,
        ImGui::GetContentRegionAvail().x);
    alignNextItem(alignment, size.x);

    const ImTextureRef texture(
        static_cast<ImTextureID>(image->textureId));
    ImGui::ImageWithBg(
        texture,
        size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f),
        ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
        ImVec4(1.0f, 1.0f, 1.0f, alpha));

    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaCoreBindingHandlers::luaUiImageButton(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string itemId = LuaModuleRuntime::stringArgument(
        *runtime, state, 1, "LuaImageButton");
    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const float requestedWidth = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0));
    const float requestedHeight = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0));
    const UiAlignment alignment = parseAlignment(
        LuaModuleRuntime::stringArgument(*runtime, state, 5, "left"));

    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        ImGui::TextColored(
            ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
            "UI IMAGE ERROR: %s",
            runtime->m_lastUiError.c_str());
        runtime->m_api.lua_pushboolean(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        return 2;
    }

    runtime->m_lastUiError.clear();
    const ImVec2 size = calculateImageSize(
        image->width,
        image->height,
        requestedWidth,
        requestedHeight,
        ImGui::GetContentRegionAvail().x);
    alignNextItem(alignment, size.x);

    const ImTextureRef texture(
        static_cast<ImTextureID>(image->textureId));
    ImGui::BeginDisabled(!runtime->m_allowInteraction);
    const bool clicked = ImGui::ImageButton(
        itemId.c_str(),
        texture,
        size,
        ImVec2(0.0f, 1.0f),
        ImVec2(1.0f, 0.0f));
    ImGui::EndDisabled();

    runtime->m_api.lua_pushboolean(state, clicked ? 1 : 0);
    runtime->m_api.lua_pushboolean(state, 1);
    return 2;
}

int LuaCoreBindingHandlers::luaUiGetImageSize(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    const heritage::ui::UiImage* image = runtime->m_uiImages.load(
        absolutePath,
        runtime->m_lastUiError);

    if (!image)
    {
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushinteger(state, 0);
        runtime->m_api.lua_pushboolean(state, 0);
        return 3;
    }

    runtime->m_lastUiError.clear();
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(image->width));
    runtime->m_api.lua_pushinteger(
        state, static_cast<LuaInteger>(image->height));
    runtime->m_api.lua_pushboolean(state, 1);
    return 3;
}

int LuaCoreBindingHandlers::luaUiUnloadImage(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime || !runtime->m_context)
        return 0;

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::filesystem::path absolutePath =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    const bool unloaded = runtime->m_uiImages.unload(absolutePath);
    runtime->m_api.lua_pushboolean(state, unloaded ? 1 : 0);
    return 1;
}

int LuaCoreBindingHandlers::luaUiGetLastError(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushlstring(
        state,
        runtime->m_lastUiError.c_str(),
        runtime->m_lastUiError.size());
    return 1;
}

int LuaCoreBindingHandlers::luaUiSetCursorPos(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const float x = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0));
    const float y = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0));
    ImGui::SetCursorPos(ImVec2((std::max)(0.0f, x), (std::max)(0.0f, y)));
    return 0;
}

int LuaCoreBindingHandlers::luaUiGetCursorPos(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const ImVec2 position = ImGui::GetCursorPos();
    runtime->m_api.lua_pushnumber(state, position.x);
    runtime->m_api.lua_pushnumber(state, position.y);
    return 2;
}

int LuaCoreBindingHandlers::luaUiDummy(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const float width = (std::max)(0.0f, static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)));
    const float height = (std::max)(0.0f, static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)));
    ImGui::Dummy(ImVec2(width, height));
    return 0;
}

int LuaCoreBindingHandlers::luaUiTextColored(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const float red = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)), 0.0f, 1.0f);
    const float green = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0)), 0.0f, 1.0f);
    const float blue = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0)), 0.0f, 1.0f);
    const float alpha = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 5, 1.0)), 0.0f, 1.0f);

    ImGui::TextColored(
        ImVec4(red, green, blue, alpha),
        "%s",
        text.c_str());
    return 0;
}

int LuaCoreBindingHandlers::luaUiProgressBar(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const float fraction = clampFloat(static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 1, 0.0)), 0.0f, 1.0f);
    const float requestedWidth = static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, -1.0));
    const float height = (std::max)(1.0f, static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)));
    const std::string overlay = LuaModuleRuntime::stringArgument(*runtime, state, 4);

    const float width = requestedWidth <= 0.0f
        ? ImGui::GetContentRegionAvail().x
        : (std::min)(requestedWidth, ImGui::GetContentRegionAvail().x);
    ImGui::ProgressBar(
        fraction,
        ImVec2(width, height),
        overlay.empty() ? nullptr : overlay.c_str());
    return 0;
}

int LuaCoreBindingHandlers::luaUiPlotLines(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    const std::string label = LuaModuleRuntime::stringArgument(*runtime, state, 1, "Plot");
    const float height = (std::max)(40.0f, static_cast<float>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 90.0)));
    const int argumentCount = runtime->m_api.lua_gettop(state);
    const int valueCount = (std::min)(512, (std::max)(0, argumentCount - 2));
    if (valueCount == 0)
    {
        ImGui::TextDisabled("%s: no captured samples", label.c_str());
        return 0;
    }

    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(valueCount));
    for (int argument = 3; argument < 3 + valueCount; ++argument)
    {
        values.push_back(static_cast<float>(
            LuaModuleRuntime::numberArgument(*runtime, state, argument, 0.0)));
    }

    ImGui::PlotLines(
        label.c_str(),
        values.data(),
        valueCount,
        0,
        nullptr,
        FLT_MAX,
        FLT_MAX,
        ImVec2(-1.0f, height));
    return 0;
}

void LuaModuleRuntime::registerUiBindings()
{
    registerFunction("UI", "BeginPanel", &LuaCoreBindingHandlers::luaUiBeginPanel);
    registerFunction("UI", "EndPanel", &LuaCoreBindingHandlers::luaUiEndPanel);
    registerFunction("UI", "BeginTabBar", &LuaCoreBindingHandlers::luaUiBeginTabBar);
    registerFunction("UI", "EndTabBar", &LuaCoreBindingHandlers::luaUiEndTabBar);
    registerFunction("UI", "BeginTabItem", &LuaCoreBindingHandlers::luaUiBeginTabItem);
    registerFunction("UI", "EndTabItem", &LuaCoreBindingHandlers::luaUiEndTabItem);
    registerFunction("UI", "ModuleLabel", &LuaCoreBindingHandlers::luaUiModuleLabel);
    registerFunction("UI", "Title", &LuaCoreBindingHandlers::luaUiTitle);
    registerFunction("UI", "Subtitle", &LuaCoreBindingHandlers::luaUiSubtitle);
    registerFunction("UI", "Text", &LuaCoreBindingHandlers::luaUiText);
    registerFunction("UI", "TextWrapped", &LuaCoreBindingHandlers::luaUiTextWrapped);
    registerFunction("UI", "TextDisabled", &LuaCoreBindingHandlers::luaUiTextDisabled);
    registerFunction("UI", "Separator", &LuaCoreBindingHandlers::luaUiSeparator);
    registerFunction("UI", "Spacing", &LuaCoreBindingHandlers::luaUiSpacing);
    registerFunction("UI", "SameLine", &LuaCoreBindingHandlers::luaUiSameLine);
    registerFunction("UI", "GetAvailableWidth", &LuaCoreBindingHandlers::luaUiGetAvailableWidth);
    registerFunction("UI", "Button", &LuaCoreBindingHandlers::luaUiButton);
    registerFunction("UI", "SliderFloat", &LuaCoreBindingHandlers::luaUiSliderFloat);
    registerFunction("UI", "InputFloat", &LuaCoreBindingHandlers::luaUiInputFloat);
    registerFunction("UI", "Checkbox", &LuaCoreBindingHandlers::luaUiCheckbox);
    registerFunction("UI", "InputInt", &LuaCoreBindingHandlers::luaUiInputInt);
    registerFunction("UI", "InputText", &LuaCoreBindingHandlers::luaUiInputText);
    registerFunction("UI", "Image", &LuaCoreBindingHandlers::luaUiImage);
    registerFunction("UI", "ImageButton", &LuaCoreBindingHandlers::luaUiImageButton);
    registerFunction("UI", "GetImageSize", &LuaCoreBindingHandlers::luaUiGetImageSize);
    registerFunction("UI", "UnloadImage", &LuaCoreBindingHandlers::luaUiUnloadImage);
    registerFunction("UI", "GetLastError", &LuaCoreBindingHandlers::luaUiGetLastError);
    registerFunction("UI", "SetCursorPos", &LuaCoreBindingHandlers::luaUiSetCursorPos);
    registerFunction("UI", "GetCursorPos", &LuaCoreBindingHandlers::luaUiGetCursorPos);
    registerFunction("UI", "Dummy", &LuaCoreBindingHandlers::luaUiDummy);
    registerFunction("UI", "TextColored", &LuaCoreBindingHandlers::luaUiTextColored);
    registerFunction("UI", "ProgressBar", &LuaCoreBindingHandlers::luaUiProgressBar);
    registerFunction("UI", "PlotLines", &LuaCoreBindingHandlers::luaUiPlotLines);
}

} // namespace heritage::modules
