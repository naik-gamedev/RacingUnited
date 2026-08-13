#pragma once

#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "LuaApi.hpp"
#include "ModuleRuntime.hpp"
#include "ModuleSaveStore.hpp"
#include "ModuleAssetRegistry.hpp"
#include "../../Scenes/SceneManager.hpp"
#include "../../UI/UiImageCache.hpp"

namespace heritage::modules {

struct LuaCoreBindingHandlers;
struct LuaPhysicsBindingHandlers;
struct LuaVehicleBindingHandlers;
struct LuaEntityBindingHandlers;

// Lua-backed module runtime.
//
// A module enters through Scripts/Main.lua and may safely include additional
// module-owned Lua files through Script.Include. The engine exposes a
// deliberately small set of safe UI and engine services.
class LuaModuleRuntime final : public ModuleRuntime
{
public:
    bool onLoad(
        GLFWwindow* window,
        const ModuleContext& context,
        const ModuleRuntimeServices& services,
        std::string& message) override;

    void onStart() override;
    void onFixedUpdate(float fixedDeltaTime) override;
    void onUpdate(float deltaTime, bool allowInteraction) override;
    heritage::math::Vec3 clearColor() const override;

    void onRender(
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings) const override;

    void onDrawUI(int framebufferWidth, int framebufferHeight) override;
    void onShutdown() override;

    bool pollAction(ModuleRuntimeAction& action) override;

    const char* runtimeId() const override { return "lua"; }
    std::string activeContentId() const override;

private:
    // CLEAN12: binding implementation domains may access runtime-owned services,
    // but their 410 Lua C-handler declarations no longer pollute this header.
    friend struct LuaCoreBindingHandlers;
    friend struct LuaPhysicsBindingHandlers;
    friend struct LuaVehicleBindingHandlers;
    friend struct LuaEntityBindingHandlers;

    bool createState(std::string& errorMessage);
    bool loadEntryScript(std::string& errorMessage);
    bool reloadScript(std::string& errorMessage);
    void destroyState(bool callShutdownFunction);

    void registerBindings();
    void registerUiBindings();
    void registerEngineBindings();
    void registerEnvironmentBindings();
    void registerVegetationBindings();
    void registerScriptBindings();
    void registerSceneBindings();
    void registerSaveBindings();
    void registerAudioBindings();
    void registerInputBindings();
    void registerPhysicsBindings();
    void registerVehicleBindings();
    void registerEntityBindings();
    void registerPrefabBindings();
    void registerModuleBindings();
    void sandboxStandardLibraries();
    void registerFunction(
        const char* tableName,
        const char* functionName,
        LuaCFunction function);
    void writeLuaApiManifest() const;
    bool runSafetySmokeTests(std::string& summary, std::filesystem::path& reportPath);
    void clearImportedStaticBoxScene();
    void replaceGlobalWithNil(const char* name);

    // Runtime-owned replacement for Lua print(); not part of a domain API table.
    static int luaPrint(lua_State* state);

    bool callOptionalNoArgs(const char* functionName);
    bool callOptionalNumber(const char* functionName, LuaNumber value);
    bool callOptionalTwoIntegers(
        const char* functionName,
        LuaInteger first,
        LuaInteger second);
    bool callOptionalString(
        const char* functionName,
        const std::string& value);
    bool callOptionalTwoStrings(
        const char* functionName,
        const std::string& first,
        const std::string& second);
    bool protectedCall(int argumentCount, int resultCount, const char* contextName);

    void setScriptError(const std::string& message);
    void clearScriptError();
    std::string stackString(int index) const;
    void pop(int count);

    void drawRuntimeError(int framebufferWidth, int framebufferHeight);
    void closeOpenPanel();
    void queueAction(
        ModuleRuntimeActionType type,
        const std::string& payload = {});

    bool checkForScriptChange(float deltaTime);
    std::unordered_map<std::string, std::filesystem::file_time_type>
        captureScriptWriteTimes() const;
    void refreshScriptWatchSnapshot();

    void requestSceneLoad(const std::string& sceneId, bool forceReload);
    void processPendingSceneTransition();

    static LuaModuleRuntime* runtimeFrom(lua_State* state);
    static std::string stringArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index,
        const std::string& fallback = {});
    static double numberArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index,
        double fallback);
    static bool booleanArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index,
        bool fallback);
    static std::uint64_t entityHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static std::uint64_t bodyHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static std::uint64_t colliderHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static std::uint64_t constraintHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static std::uint64_t vehicleHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    LuaApi m_api;
    lua_State* m_state = nullptr;
    GLFWwindow* m_window = nullptr;
    heritage::audio::AudioSystem* m_audio = nullptr;
    heritage::input::InputSystem* m_input = nullptr;
    heritage::entities::EntityRegistry* m_entities = nullptr;
    heritage::physics::PhysicsWorld* m_physics = nullptr;
    heritage::graphics::EnvironmentSystem* m_environment = nullptr;
    heritage::graphics::VegetationSystem* m_vegetation = nullptr;
    std::unordered_set<std::uint64_t> m_audioHandles;
    std::string m_lastAudioError;
    std::optional<ModuleContext> m_context;
    std::filesystem::path m_scriptPath;
    std::unordered_map<std::string, std::filesystem::file_time_type>
        m_scriptWriteTimes;

    std::deque<ModuleRuntimeAction> m_actions;
    heritage::scenes::SceneManager m_sceneManager;
    ModuleSaveStore m_saveStore;
    ModuleAssetRegistry m_assetRegistry;
    heritage::ui::UiImageCache m_uiImages;
    std::string m_lastUiError;
    std::string m_lastPrefabError;
    std::string m_lastPhysicsError;
    std::vector<std::uint64_t> m_importedStaticSceneBodies;
    std::vector<std::uint64_t> m_importedStaticSceneEntities;
    std::optional<std::string> m_pendingSceneId;
    bool m_pendingSceneReload = false;
    std::string m_lastSceneError;
    heritage::math::Vec3 m_clearColor{ 0.003f, 0.005f, 0.008f };

    std::string m_scriptError;
    std::string m_lastReloadMessage;
    std::vector<std::string> m_registeredLuaFunctions;
    std::string m_lastSafetyReport;
    std::filesystem::path m_lastSafetyReportPath;

    float m_reloadCheckTimer = 0.0f;
    float m_saveFlushTimer = 0.0f;
    bool m_loaded = false;
    bool m_started = false;
    bool m_allowInteraction = true;
    bool m_f5WasDown = false;

    enum class UiScopeType
    {
        TabBar,
        TabItem
    };

    struct UiPanelPlacement
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    int m_framebufferWidth = 1;
    int m_framebufferHeight = 1;
    bool m_panelOpen = false;
    bool m_panelVisible = false;
    bool m_uiLayoutEditing = false;
    std::string m_activePanelId;
    std::unordered_map<std::string, UiPanelPlacement> m_uiPanelPlacements;
    std::string m_numericSliderInputLabel;
    bool m_numericSliderFocusRequested = false;
    std::vector<UiScopeType> m_uiScopes;
};

} // namespace heritage::modules
