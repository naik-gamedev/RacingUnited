#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "ModuleRuntime.hpp"

namespace heritage::modules {

// Lightweight module-owned UI runtime.
//
// The module describes screens in a .hui text file. Heritage Engine renders
// the controls through its own UI layer (currently backed by ImGui), so module
// files never call raw ImGui functions and can survive a future UI backend
// replacement.
class ScriptedUiRuntime final : public ModuleRuntime
{
public:
    bool onLoad(
        GLFWwindow* window,
        const ModuleContext& context,
        const ModuleRuntimeServices& services,
        std::string& message) override;

    void onStart() override;
    void onUpdate(float deltaTime, bool allowInteraction) override;
    heritage::math::Vec3 clearColor() const override;

    void onRender(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const override;

    void onDrawUI(int framebufferWidth, int framebufferHeight) override;
    void onShutdown() override;

    bool pollAction(ModuleRuntimeAction& action) override;

    const char* runtimeId() const override { return "scripted_ui"; }
    std::string activeContentId() const override;

private:
    struct ButtonDefinition
    {
        std::string label;
        std::string action;
    };

    struct ScreenDefinition
    {
        std::string id;
        std::string title;
        std::string subtitle;
        std::vector<std::string> textLines;
        std::vector<ButtonDefinition> buttons;
    };

    bool loadDocument(const std::string& path, std::string& errorMessage);
    bool parseDocument(const std::string& source, std::string& errorMessage);
    void executeAction(const std::string& action);
    void queueAction(ModuleRuntimeActionType type, const std::string& payload = {});

    static std::string trim(const std::string& value);
    static bool parseColor(const std::string& value, heritage::math::Vec3& color);

    std::unordered_map<std::string, ScreenDefinition> m_screens;
    std::vector<std::string> m_screenOrder;
    std::deque<ModuleRuntimeAction> m_actions;

    std::string m_moduleName;
    std::string m_documentPath;
    std::string m_startScreen;
    std::string m_currentScreen;
    std::string m_messageTitle;
    std::string m_messageText;

    heritage::math::Vec3 m_clearColor{ 0.005f, 0.007f, 0.010f };
    bool m_loaded = false;
    bool m_started = false;
    bool m_allowInteraction = true;
    bool m_openMessagePopup = false;
};

} // namespace heritage::modules
