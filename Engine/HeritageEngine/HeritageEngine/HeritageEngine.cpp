#include "HeritageEngine.hpp"

#include <glad/glad.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef _WIN32
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#include <dwmapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#include <unistd.h>
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "Runtime/EngineStartup.hpp"
#include "Runtime/EngineUiStyle.hpp"
#include "Runtime/EngineRuntimeState.hpp"
#include "Runtime/EngineFrame.hpp"
#include "Runtime/EngineHotkeys.hpp"
#include "Runtime/EngineRendering.hpp"
#include "Runtime/EngineSimulation.hpp"
#include "Display/DisplayModeController.hpp"
#include "../Core/Diagnostics/PerformanceOverlay.hpp"
#include "../Platform/Windows/BackbufferClipboard.hpp"
#include "../Core/Math/Math.hpp"
#include "../Camera/ChaseCamera.hpp"
#include "../Camera/VehicleCameraController.hpp"
#include "../Core/Diagnostics/BuildIdentity.hpp"
#include "../Core/Diagnostics/PerformanceMonitor.hpp"
#include "../Core/Settings/VideoSettings.hpp"
#include "../Core/Settings/VideoSettingsStorage.hpp"
#include "../Core/Settings/AudioSettings.hpp"
#include "../Core/Settings/AudioSettingsStorage.hpp"
#include "../Core/Modules/ModuleLoader.hpp"
#include "../Core/Modules/ModuleContext.hpp"
#include "../Core/Timing/FrameLimiter.hpp"
#include "../Audio/AudioSystem.hpp"
#include "../Input/InputSystem.hpp"
#include "../Physics/PhysicsWorld.hpp"
#include "../Core/Entities/EntityRegistry.hpp"
#include "../Graphics/AntiAliasing.hpp"
#include "../Graphics/DisplaySystem.hpp"
#include "../Graphics/EnvironmentSystem.hpp"
#include "../Graphics/VegetationSystem.hpp"
#include "../Graphics/WindowSystem.hpp"
#include "../Graphics/Framebuffer/PostFramebuffer.hpp"
#include "../Graphics/PostProcessing/PostProcessor.hpp"
#include "../Graphics/Renderer/EntityDebugRenderer.hpp"
#include "../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../Graphics/Renderer/SurfacePresentationRenderer.hpp"
#include "../Graphics/Renderer/WeatherPresentationRenderer.hpp"
#include "../Graphics/RenderScaler.hpp"
#include "../Core/Modules/ModuleRuntimeManager.hpp"
#include "../Core/Modules/ModuleRuntimeServices.hpp"
#include "../UI/PauseMenu.hpp"
#include "../UI/WeatherRadarOverlay.hpp"

namespace fs = std::filesystem;
using heritage::math::Mat4;
using heritage::math::Vec3;
using heritage::math::perspectiveReversedZ;
using heritage::camera::ChaseCamera;
using heritage::camera::ChaseCameraInput;
using heritage::camera::VehicleCameraController;
using heritage::camera::VehicleCameraFlyInput;
using heritage::camera::CameraFrame;
using heritage::diagnostics::PerformanceMonitor;
using heritage::diagnostics::PerformanceSection;
using heritage::diagnostics::RenderPerformanceSection;
using heritage::graphics::AntiAliasingSettings;
using heritage::graphics::resolveAntiAliasing;
using heritage::graphics::DisplaySystem;
using heritage::graphics::kDefaultNearClipMeters;
using heritage::graphics::kDefaultFarClipMeters;
using heritage::graphics::EnvironmentSystem;
using heritage::graphics::VegetationSystem;
using heritage::graphics::WindowSystem;
using heritage::graphics::WindowMode;
using heritage::graphics::PostFramebuffer;
using heritage::graphics::PostProcessor;
using heritage::graphics::EntityDebugRenderer;
using heritage::graphics::EntityMeshRenderer;
using heritage::graphics::SurfacePresentationRenderer;
using heritage::graphics::WeatherPresentationRenderer;
using heritage::graphics::RenderScaler;
using heritage::graphics::RenderSize;
using heritage::settings::VideoSettings;
using heritage::settings::VideoSettingsStorage;
using heritage::settings::kDefaultWindowHeight;
using heritage::settings::kDefaultWindowWidth;
using heritage::settings::kMinimumInteractiveWindowHeight;
using heritage::settings::kMinimumInteractiveWindowWidth;
using heritage::settings::AudioSettings;
using heritage::settings::AudioSettingsStorage;
using heritage::settings::selectedFpsCap;
using heritage::modules::ModuleInfo;
using heritage::modules::ModuleLoader;
using heritage::modules::ModuleContext;
using heritage::modules::ModuleRuntimeManager;
using heritage::modules::ModuleRuntimeServices;
using heritage::modules::ModuleRuntimeAction;
using heritage::modules::ModuleRuntimeActionType;
using heritage::timing::FrameLimiter;
using heritage::audio::AudioSystem;
using heritage::input::InputSystem;
using heritage::physics::PhysicsWorld;
using heritage::entities::EntityRegistry;
using heritage::ui::drawPauseMenu;

#ifdef _WIN32
#define RACING_GLSL_VERSION "#version 460 core\n"
#else
#define RACING_GLSL_VERSION "#version 330 core\n"
#endif

namespace heritage::engine {

namespace {

void saveAllSettings(
    EngineRuntimeState& state,
    GLFWwindow* window,
    DisplayModeController& displayModeController)
{
    if (!window)
        return;

    if (state.window.mode() == WindowMode::Windowed)
        state.window.saveCurrentRect(window);

    state.videoSettings.windowPlacementValid = true;
    state.videoSettings.windowX = state.window.savedX();
    state.videoSettings.windowY = state.window.savedY();
    state.videoSettings.windowWidth = state.window.savedW();
    state.videoSettings.windowHeight = state.window.savedH();

    displayModeController.syncVideoSettings(window);

    try
    {
        if (!VideoSettingsStorage::save(
                state.videoSettingsPath.string(),
                state.videoSettings))
        {
            std::cerr << "Could not save video settings.\n";
        }

        state.display.save(state.displaySettingsPath.string());

        if (!AudioSettingsStorage::save(
                state.audioSettingsPath.string(),
                state.audio.settings()))
        {
            std::cerr << "Could not save audio settings.\n";
        }
    }
    catch (...)
    {
        std::cerr << "Could not save engine settings.\n";
    }
}

} // namespace

HeritageEngine::HeritageEngine()
    : m_state(std::make_unique<EngineRuntimeState>())
{
}

HeritageEngine::~HeritageEngine() = default;

int HeritageEngine::run(int argc, char** argv)
{
    EngineRuntimeState& state = *m_state;
    if (!glfwInit()) { std::cerr << "GLFW init failed\n"; return -1; }

#ifdef _WIN32
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    const fs::path requestedProjectRoot = ModuleLoader::requestedProjectRoot(argc, argv);
    const fs::path projectRoot = requestedProjectRoot.empty()
        ? heritage::engine::startup::findProjectRoot()
        : requestedProjectRoot.lexically_normal();

    const fs::path requestedModulePath = ModuleLoader::requestedModulePath(argc, argv);
    const std::string requestedModuleId = ModuleLoader::requestedModuleId(argc, argv);

    ModuleInfo activeModule;
    if (!requestedModulePath.empty())
        activeModule = ModuleLoader::loadFromPath(projectRoot, requestedModulePath);

    // Compatibility path for direct launches and old shortcuts.
    if (!activeModule.valid && !requestedModuleId.empty())
        activeModule = ModuleLoader::load(projectRoot, requestedModuleId);

    if (!activeModule.valid)
    {
        std::string message = "Heritage Engine could not load the requested module.";
        if (!requestedModuleId.empty())
            message += "\nModule id: " + requestedModuleId;
        if (!requestedModulePath.empty())
            message += "\nModule path: " + requestedModulePath.string();
        message += "\nProject root: " + projectRoot.string();

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), "Heritage Engine - Module Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    if (!requestedModuleId.empty() && activeModule.id != requestedModuleId)
    {
        const std::string message =
            "The launcher requested module id '" + requestedModuleId
            + "', but the selected module folder declares id '" + activeModule.id
            + "'.\n\nLaunch was stopped instead of loading the wrong module.";

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, message.c_str(), "Heritage Engine - Module Identity Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    ModuleContext moduleContext(projectRoot, activeModule);
    std::string moduleContextError;
    if (!moduleContext.prepareUserDirectories(moduleContextError))
    {
        std::cerr << moduleContextError << '\n';
#ifdef _WIN32
        MessageBoxA(nullptr, moduleContextError.c_str(), "Heritage Engine - Module Error", MB_OK | MB_ICONERROR);
#endif
        glfwTerminate();
        return -1;
    }

    state.videoSettingsPath = moduleContext.resolveSettingsPath("settings_video.ini");
    state.displaySettingsPath = moduleContext.resolveSettingsPath("settings_engine.ini");
    state.audioSettingsPath = moduleContext.resolveSettingsPath("settings_audio.ini");
    state.inputSettingsPath = moduleContext.resolveSettingsPath("settings_input.ini");

    try
    {
        if (fs::exists(state.videoSettingsPath))
        {
            VideoSettingsStorage::load(
                state.videoSettingsPath.string(),
                state.videoSettings);
        }
        else
        {
            // One-time compatibility path for settings created before modules
            // had independent persistent data.
            const fs::path legacyVideoSettingsPath = projectRoot / "settings_video.ini";
            if (fs::exists(legacyVideoSettingsPath))
            {
                VideoSettingsStorage::load(
                    legacyVideoSettingsPath.string(),
                    state.videoSettings);
            }
        }
    }
    catch (...)
    {
        state.videoSettings = VideoSettings{};
    }

    try
    {
        if (fs::exists(state.audioSettingsPath))
            AudioSettingsStorage::load(state.audioSettingsPath.string(), state.audioSettings);
    }
    catch (...)
    {
        state.audioSettings = AudioSettings{};
    }

    std::cout << "Build identity: "
        << heritage::diagnostics::buildIdentity() << "\n";
    std::cout << "Project root: " << projectRoot.string() << "\n";
    std::cout << "Active module: " << activeModule.name
              << " [" << activeModule.id << "]\n";
    std::cout << "Module root: " << activeModule.rootPath.string() << "\n";
    std::cout << "Manifest entry_scene: "
              << (activeModule.scene.empty() ? "<empty>" : activeModule.scene)
              << "\n";

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* vid = glfwGetVideoMode(mon);
    if (!mon || !vid)
    {
        std::cerr << "Primary monitor information is unavailable.\n";
        glfwTerminate();
        return -1;
    }

    int workX = 0;
    int workY = 0;
    int workW = vid->width;
    int workH = vid->height;
    glfwGetMonitorWorkarea(mon, &workX, &workY, &workW, &workH);

    const int maximumWindowWidth = std::max(1, workW);
    const int maximumWindowHeight = std::max(1, workH);
    const int minimumWindowWidth = std::min(
        kMinimumInteractiveWindowWidth,
        maximumWindowWidth);
    const int minimumWindowHeight = std::min(
        kMinimumInteractiveWindowHeight,
        maximumWindowHeight);

    int startW = state.videoSettings.windowPlacementValid
        ? state.videoSettings.windowWidth
        : (state.videoSettings.resolutionWidth > 0
            ? state.videoSettings.resolutionWidth
            : kDefaultWindowWidth);
    int startH = state.videoSettings.windowPlacementValid
        ? state.videoSettings.windowHeight
        : (state.videoSettings.resolutionHeight > 0
            ? state.videoSettings.resolutionHeight
            : kDefaultWindowHeight);

    startW = std::clamp(startW, minimumWindowWidth, maximumWindowWidth);
    startH = std::clamp(startH, minimumWindowHeight, maximumWindowHeight);

    int startX = workX + std::max(0, (workW - startW) / 2);
    int startY = workY + std::max(0, (workH - startH) / 2);

    if (state.videoSettings.windowPlacementValid)
    {
        const int maximumX = workX + std::max(0, workW - startW);
        const int maximumY = workY + std::max(0, workH - startH);
        startX = std::clamp(state.videoSettings.windowX, workX, maximumX);
        startY = std::clamp(state.videoSettings.windowY, workY, maximumY);
    }

    const std::string sceneTitle = activeModule.scene.empty()
        ? "<empty>"
        : activeModule.scene;
    const std::string windowTitle = activeModule.name + " [" + activeModule.id
        + " | " + sceneTitle + "] - Heritage Engine";
    GLFWwindow* window = glfwCreateWindow(startW, startH, windowTitle.c_str(), nullptr, nullptr);
    if (!window) { std::cerr << "Window failed\n"; glfwTerminate(); return -1; }

    // Step 29I.1 safety: custom window chrome and the prototype lab become
    // unusable below this size. GLFW enforces the limit for manual resizing,
    // while the settings loader above repairs stale/tiny remembered rectangles.
    glfwSetWindowSizeLimits(
        window,
        minimumWindowWidth,
        minimumWindowHeight,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE);
    glfwSetWindowPos(window, startX, startY);
    std::cout << "Window safety floor: "
              << minimumWindowWidth << "x" << minimumWindowHeight << "\n";

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(window);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
#endif

    glfwMakeContextCurrent(window);
    glfwSwapInterval(state.videoSettings.vsyncEnabled ? 1 : 0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) { std::cerr << "GLAD failed\n"; return -1; }
    std::cout << "OpenGL " << glGetString(GL_VERSION) << "\n";

    // Systems
    state.window.initialize(window);
    state.display.initialize();
    DisplayModeController displayModeController(state.display, state.window, state.videoSettings);
    try {
        if (fs::exists(state.displaySettingsPath))
        {
            state.display.load(state.displaySettingsPath.string());
        }
        else
        {
            const fs::path legacyDisplaySettingsPath = projectRoot / "settings_engine.ini";
            if (fs::exists(legacyDisplaySettingsPath))
                state.display.load(legacyDisplaySettingsPath.string());
        }
    }
    catch (...) {}

    displayModeController.applyLoadedMode(window);

    std::string audioMessage;
    if (!state.audio.initialize(audioMessage))
    {
        std::cerr << "Audio disabled: " << audioMessage << '\n';
    }
    state.audio.applySettings(state.audioSettings);

    std::string inputMessage;
    if (!state.input.initialize(window, state.inputSettingsPath, inputMessage))
    {
        std::cerr << "Input initialization warning: " << inputMessage << '\n';
    }
    else if (!inputMessage.empty())
    {
        std::cerr << "Input settings warning: " << inputMessage << '\n';
    }

    // Module-owned action declarations are loaded natively before Lua starts.
    // This keeps the Input settings page functional even if a script has a
    // syntax error or chooses not to register actions dynamically.
    const fs::path inputDefinitionsPath =
        moduleContext.resolveDataPath("InputActions.ini");
    std::string inputDefinitionsMessage;
    if (!state.input.loadActionDefinitions(
            inputDefinitionsPath,
            inputDefinitionsMessage))
    {
        std::cerr << "Input action definition error: "
                  << inputDefinitionsMessage << '\n';
    }
    else if (!inputDefinitionsMessage.empty())
    {
        std::cout << inputDefinitionsMessage << '\n';
    }

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;

    const std::string fontPath = (projectRoot / "Assets/Fonts/Orbitron-SemiBold.ttf").string();
    state.fontSmall = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 13.0f);
    state.fontNormal = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
    state.fontLarge = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 22.0f);
    if (!state.fontSmall || !state.fontNormal || !state.fontLarge)
        state.fontSmall = state.fontNormal = state.fontLarge = io.Fonts->AddFontDefault();

    applyEngineUiStyle();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(RACING_GLSL_VERSION);

    // GFX08 long-range depth convention: reversed-Z. Near surfaces have larger
    // depth values, the far horizon approaches zero, and floating-point depth
    // keeps useful precision across the 100 km view range.
    glClearDepth(0.0);
    glDepthFunc(GL_GREATER);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);

    EntityRegistry entityRegistry;
    entityRegistry.resetForModule(activeModule.id);
    state.physics.reset();

    EnvironmentSystem environmentSystem;
    environmentSystem.reset();

    VegetationSystem vegetationSystem;
    vegetationSystem.reset();

    // CAMLAB01 is an engine-owned camera service so module Lua can edit the
    // live vehicle-mounted authoring pose without owning render internals.
    VehicleCameraController vehicleCamera;

    ModuleRuntimeManager moduleRuntime;
    ModuleRuntimeServices runtimeServices;
    runtimeServices.audio = &state.audio;
    runtimeServices.input = &state.input;
    runtimeServices.vehicleCamera = &vehicleCamera;
    runtimeServices.entities = &entityRegistry;
    runtimeServices.physics = &state.physics;
    runtimeServices.environment = &environmentSystem;
    runtimeServices.vegetation = &vegetationSystem;

    std::string runtimeMessage;
    if (!moduleRuntime.initialize(window, moduleContext, runtimeServices, runtimeMessage))
    {
        const std::string message = runtimeMessage.empty()
            ? "Heritage Engine could not start the selected module runtime."
            : runtimeMessage;

        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Module Runtime Error",
            MB_OK | MB_ICONERROR);
#endif
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.input.shutdown();
        state.audio.shutdown();
        state.display.shutdown();
        state.window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    if (!runtimeMessage.empty())
        std::cerr << runtimeMessage << '\n';

    EntityMeshRenderer entityMeshRenderer;
    if (!entityMeshRenderer.initialize(moduleContext.assetRoot(), &environmentSystem))
    {
        const std::string message = entityMeshRenderer.lastError().empty()
            ? "EntityMeshRenderer could not initialize."
            : entityMeshRenderer.lastError();
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Entity Mesh Renderer Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        entityRegistry.clear();
        state.input.shutdown();
        state.audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.display.shutdown();
        state.window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    EntityDebugRenderer entityDebugRenderer;
    if (!entityDebugRenderer.initialize())
    {
        const std::string message =
            "EntityDebugRenderer could not initialize its primitive meshes or shader.";
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Entity Renderer Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        entityMeshRenderer.shutdown();
        entityRegistry.clear();
        state.input.shutdown();
        state.audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.display.shutdown();
        state.window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    SurfacePresentationRenderer surfacePresentationRenderer;
    if (!surfacePresentationRenderer.initialize())
    {
        const std::string message =
            "SurfacePresentationRenderer could not initialize its dynamic buffers or shaders.";
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Surface Presentation Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        entityDebugRenderer.shutdown();
        entityMeshRenderer.shutdown();
        entityRegistry.clear();
        state.input.shutdown();
        state.audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.display.shutdown();
        state.window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    WeatherPresentationRenderer weatherPresentationRenderer;
    if (!weatherPresentationRenderer.initialize(moduleContext.assetRoot()))
    {
        const std::string message =
            "WeatherPresentationRenderer could not initialize its rain shader.";
        std::cerr << message << '\n';
#ifdef _WIN32
        MessageBoxA(
            nullptr,
            message.c_str(),
            "Heritage Engine - Weather Presentation Error",
            MB_OK | MB_ICONERROR);
#endif
        moduleRuntime.shutdown();
        surfacePresentationRenderer.shutdown();
        entityDebugRenderer.shutdown();
        entityMeshRenderer.shutdown();
        entityRegistry.clear();
        state.input.shutdown();
        state.audio.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        state.display.shutdown();
        state.window.shutdown();
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    ChaseCamera chaseCamera;
    CameraFrame entityCameraFrame;
    bool vehicleCameraCursorCaptured = false;

    std::cout << "Active runtime: " << moduleRuntime.runtimeId() << "\n";
    std::cout << "Active content: " << moduleRuntime.activeContentId() << "\n";
    const auto jobStartupStats = state.jobs.stats();
    std::cout << "Job system: " << jobStartupStats.workerThreadCount
              << " workers + caller across "
              << jobStartupStats.hardwareThreadCount
              << " logical processors\n";
    std::cout << "Registered input actions: "
              << state.input.actionCount() << "\n";
    heritage::engine::startup::writeLaunchDiagnostics(
        projectRoot,
        requestedModulePath,
        requestedModuleId,
        activeModule,
        moduleContext,
        moduleRuntime.runtimeId(),
        moduleRuntime.activeContentId());


    EngineRenderingState renderingState;
    if (!initializeEngineRendering(renderingState))
    {
        std::cerr << "PostProcessor initialization failed\n";
        return -1;
    }

    FrameLimiter frameLimiter;
    EngineFrameState frameState;
    frameState.appliedVSync = state.videoSettings.vsyncEnabled;
    frameState.prevTime = glfwGetTime();
    EngineHotkeyState hotkeyState;
    bool shouldClose = false;
    bool shouldMin = false;

    PerformanceMonitor performanceMonitor;
    performanceMonitor.reset();

    while (!glfwWindowShouldClose(window) && !shouldClose)
    {
        EngineFrameData frameData;
        beginEngineFrame(
            frameState,
            frameData,
            window,
            state.videoSettings,
            state.input,
            state.audio,
            frameLimiter,
            performanceMonitor,
            shouldMin);

        processEngineHotkeys(
            window,
            state.window,
            displayModeController,
            entityMeshRenderer,
            environmentSystem,
            hotkeyState);

        if (!completeEngineFrameSetup(
                frameData,
                window,
                state.window,
                displayModeController,
                state.videoSettings,
                frameLimiter))
        {
            continue;
        }

        EngineRenderFrame renderFrame = prepareEngineRendering(
            renderingState,
            state.display,
            state.videoSettings,
            frameData.framebufferWidth,
            frameData.framebufferHeight);

        finalizeEngineFrameTiming(
            frameState,
            frameData,
            performanceMonitor);
        renderFrame.now = frameData.now;

        const int fbW = frameData.framebufferWidth;
        const int fbH = frameData.framebufferHeight;
        const float dt = frameData.dt;
        const double now = frameData.now;

        ChaseCameraInput chaseCameraInput{};
        const bool interfaceOwnsMouse = ImGui::GetCurrentContext()
            && ImGui::GetIO().WantCaptureMouse;
        // CAM06 paused inspection: ESC freezes gameplay/weather, but the
        // chase camera remains an interactive presentation tool. Holding the
        // primary mouse button over the world (not over an ImGui control) may
        // orbit around the frozen vehicle exactly as it can during gameplay.
        chaseCameraInput.orbitDragActive = !interfaceOwnsMouse
            && state.input.mouseDown("Left");
        if (chaseCameraInput.orbitDragActive)
        {
            chaseCameraInput.pointerDeltaX = state.input.mouseDeltaX();
            chaseCameraInput.pointerDeltaY = state.input.mouseDeltaY();
        }

        // CAMLAB01 keeps its legacy Shift+Grave authoring shortcut for named
        // vehicle-mounted cameras. CAM07 adds a module-owned, rebindable
        // "Toggle Free Camera" action for the ordinary detached world camera.
        // Activation copies the CURRENT render frame, so the camera simply
        // lets go of the car instead of teleporting to another view.
        const bool cameraFlyAuthoringHotkey = state.input.keyPressed("Grave")
            && (state.input.keyDown("LeftShift")
                || state.input.keyDown("RightShift"));
        if (cameraFlyAuthoringHotkey && vehicleCamera.active())
        {
            vehicleCamera.setFlyEnabled(!vehicleCamera.flyEnabled());
        }
        else if (state.input.actionPressed("Toggle Free Camera"))
        {
            if (vehicleCamera.detachedActive())
            {
                vehicleCamera.deactivateDetached();
            }
            else
            {
                vehicleCamera.activateDetachedFromFrame(
                    entityCameraFrame,
                    state.physics.globalOrigin());
            }
        }

        // CAM10: ESC/pause and a visible module control panel temporarily own
        // the pointer without deactivating detached free flight. Closing the UI
        // restores GLFW_CURSOR_DISABLED and the same camera continues moving.
        const bool vehicleCameraNavigationActive = vehicleCamera.flyEnabled()
            && !hotkeyState.menuOpen
            && !vehicleCamera.uiInteractionActive();
        bool vehicleCameraCursorCaptureChanged = false;
        if (vehicleCameraNavigationActive != vehicleCameraCursorCaptured)
        {
            vehicleCameraCursorCaptured = vehicleCameraNavigationActive;
            vehicleCameraCursorCaptureChanged = true;
            glfwSetInputMode(
                window,
                GLFW_CURSOR,
                vehicleCameraCursorCaptured
                    ? GLFW_CURSOR_DISABLED
                    : GLFW_CURSOR_NORMAL);
        }

        VehicleCameraFlyInput vehicleCameraFlyInput{};
        if (vehicleCameraNavigationActive)
        {
            // These are normal InputSystem actions, so every navigation key is
            // visible and rebindable in Settings > Input > Camera. While fly
            // navigation owns input, Racing United suppresses vehicle controls.
            vehicleCameraFlyInput.moveForward =
                state.input.actionDown("Camera Forward");
            vehicleCameraFlyInput.moveBackward =
                state.input.actionDown("Camera Backward");
            vehicleCameraFlyInput.moveLeft =
                state.input.actionDown("Camera Left");
            vehicleCameraFlyInput.moveRight =
                state.input.actionDown("Camera Right");
            vehicleCameraFlyInput.moveDown =
                state.input.actionDown("Camera Down");
            vehicleCameraFlyInput.moveUp =
                state.input.actionDown("Camera Up");
            vehicleCameraFlyInput.fast =
                state.input.actionDown("Camera Fast");
            vehicleCameraFlyInput.slow =
                state.input.actionDown("Camera Slow");
            // GLFW may warp/renormalize its virtual pointer when switching
            // NORMAL <-> DISABLED. Ignore that one transition delta so closing
            // a GUI cannot snap the free camera by a large angle.
            if (!vehicleCameraCursorCaptureChanged)
            {
                vehicleCameraFlyInput.pointerDeltaX = state.input.mouseDeltaX();
                vehicleCameraFlyInput.pointerDeltaY = state.input.mouseDeltaY();
            }
            chaseCameraInput.orbitDragActive = false;
        }

        updateEngineSimulation(
            dt,
            now,
            hotkeyState.menuOpen,
            state.physics,
            moduleRuntime,
            environmentSystem,
            entityRegistry,
            chaseCamera,
            vehicleCamera,
            chaseCameraInput,
            vehicleCameraFlyInput,
            entityCameraFrame,
            performanceMonitor);

        const bool gpuTimerActiveThisFrame = renderEngineScene(
            renderingState,
            renderFrame,
            state.display,
            state.videoSettings,
            moduleRuntime,
            entityMeshRenderer,
            entityDebugRenderer,
            surfacePresentationRenderer,
            weatherPresentationRenderer,
            state.physics.surfaces(),
            entityRegistry,
            entityCameraFrame,
            performanceMonitor,
            hotkeyState.wireframeVisible);

        const double uiCpuStart = glfwGetTime();

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Module-owned overlay UI is drawn behind the permanent engine UI.
        moduleRuntime.drawUI(fbW, fbH);

        // Runtimes communicate with the engine through a deliberately small
        // action queue instead of reaching into main.cpp or WindowSystem.
        ModuleRuntimeAction moduleAction;
        while (moduleRuntime.pollAction(moduleAction))
        {
            if (moduleAction.type == ModuleRuntimeActionType::OpenEngineSettings)
            {
                hotkeyState.menuOpen = true;
                hotkeyState.menuShowSettings = true;
            }
            else if (moduleAction.type == ModuleRuntimeActionType::ExitApplication)
            {
                shouldClose = true;
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }

        // Display-mode confirmation/revert policy is owned by the display controller.
        displayModeController.drawChangeConfirmationPopup(
            window,
            fbW,
            fbH,
            [&]()
            {
                saveAllSettings(state, window, displayModeController);
            });

        // Titlebar + Resize (now clean)
        state.window.drawTitlebar(window, fbW, fbH, shouldClose, shouldMin);
        state.window.drawResizeHandles(window);

        // In-game pause menu
        drawPauseMenu(
            window,
            hotkeyState.menuOpen,
            hotkeyState.menuShowSettings,
            shouldClose,
            fbW,
            fbH,
            state.fontLarge,
            state.fontNormal,
            state.display,
            state.window,
            state.videoSettings,
            state.audio,
            state.input,
            vid->width,
            vid->height,
            [&](WindowMode newMode, int desiredW, int desiredH, int desiredRefresh)
            {
                displayModeController.initiateChange(window, newMode, desiredW, desiredH, desiredRefresh);
            });

        if (hotkeyState.weatherRadarVisible)
        {
            const heritage::math::Vec3 radarEyeLocal = entityCameraFrame.valid
                ? entityCameraFrame.eyeLocal
                : heritage::math::Vec3{ 0.0f, 3.4f, 8.5f };
            const double radarForwardX = entityCameraFrame.valid
                ? static_cast<double>(entityCameraFrame.targetLocal.x - entityCameraFrame.eyeLocal.x)
                : 0.0;
            const double radarForwardZ = entityCameraFrame.valid
                ? static_cast<double>(entityCameraFrame.targetLocal.z - entityCameraFrame.eyeLocal.z)
                : -1.0;
            const double radarHeadingRadians = std::atan2(radarForwardX, radarForwardZ);
            heritage::ui::drawWeatherRadarOverlay(
                state.physics.surfaces(),
                state.physics.surfaces().localToGlobal(radarEyeLocal),
                radarHeadingRadians);
        }

        if (hotkeyState.performanceOverlayVisible)
        {
            heritage::diagnostics::drawPerformanceOverlay(
                performanceMonitor.snapshot(),
                entityMeshRenderer.frameStats(),
                entityDebugRenderer.frameStats(),
                surfacePresentationRenderer.frameStats(),
                weatherPresentationRenderer.frameStats(),
                vegetationSystem.stats(),
                entityRegistry.count(),
                entityMeshRenderer.loadedAssetCount(),
                state.jobs.stats(),
                state.physics.lastWorldStepCount(),
                state.physics.overloadedLastFrame(),
                state.videoSettings.vsyncEnabled,
                heritage::settings::selectedFpsCap(state.videoSettings));
        }

        ImGui::Render();
        glDisable(GL_DEPTH_TEST);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        performanceMonitor.recordSection(
            PerformanceSection::UiCpu,
            (glfwGetTime() - uiCpuStart) * 1000.0);


        const double screenshotCaptureMsThisFrame = captureRequestedScreenshot(
            frameState,
            window,
            fbW,
            fbH,
            hotkeyState.screenshotClipboardRefreshFrames);

        endEngineGpuTimer(renderingState, gpuTimerActiveThisFrame);
        presentEngineFrame(
            frameData,
            window,
            state.videoSettings,
            frameLimiter,
            performanceMonitor,
            screenshotCaptureMsThisFrame);
    }

    // Cleanup
    // An unconfirmed display change must never become the saved startup mode.
    if (displayModeController.changePending())
        displayModeController.restorePendingChange(window);

    saveAllSettings(state, window, displayModeController);

    moduleRuntime.shutdown();
    weatherPresentationRenderer.shutdown();
    surfacePresentationRenderer.shutdown();
    entityDebugRenderer.shutdown();
    entityMeshRenderer.shutdown();
    entityRegistry.clear();
    vegetationSystem.reset();
    state.physics.reset();
    state.input.shutdown();
    state.audio.shutdown();
    state.display.shutdown();
    state.window.shutdown();
    shutdownEngineRendering(renderingState);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();


    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
} // namespace heritage::engine
