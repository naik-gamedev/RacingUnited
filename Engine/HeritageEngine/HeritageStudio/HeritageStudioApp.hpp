#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "../Audio/AudioSystem.hpp"
#include "../Audio/Lab/EngineSoundCaptureLab.hpp"
#include "Authoring/StudioAuthoringData.hpp"
#include "StudioScenePreview.hpp"

struct GLFWwindow;

namespace heritage::studio {

enum class Workspace
{
    Scene,
    Race,
    Traffic,
    Gameplay,
    Weather,
    Vehicle,
    Audio,
    Assets
};

struct EngineCaptureTarget
{
    int rpm = 0;
    int throttlePercent = 0;
    bool idle = false;
};

struct StudioViewportFlyState
{
    bool active = false;
    float speedMps = 12.0f;
    float startYawDeg = 0.0f;
    float startPitchDeg = 0.0f;
    float startDistanceM = 10.0f;
    authoring::Vec3 startTarget{};
    bool startOrthographic = false;
};

class HeritageStudioApp
{
public:
    HeritageStudioApp();
    ~HeritageStudioApp();

    HeritageStudioApp(const HeritageStudioApp&) = delete;
    HeritageStudioApp& operator=(const HeritageStudioApp&) = delete;

    int run();

private:
    bool initialize();
    void shutdown();
    void frame();
    void drawStudioWindowChrome(int framebufferWidth, int framebufferHeight);
    void drawStudioResizeHandles();
    void setStudioMaximized(bool maximized);
    void drawMainMenu();
    void drawWorkspaceRail();
    void drawWorkspace();
    void drawSceneWorkspace();
    void drawSceneViewportInteractive();
    void drawRaceWorkspace();
    void drawRaceViewportInteractive();
    void drawTrafficWorkspace();
    void drawTrafficViewportInteractive();
    void drawGameplayWorkspace();
    void drawGameplayViewportInteractive();
    void drawWeatherWorkspace();
    void drawVehicleWorkspace();
    void drawAudioWorkspace();
    void drawAssetsWorkspace();
    void drawAudioCapturePanel();
    void drawAudioAssistantPanel();
    void drawAudioCharacterPanel();
    void drawAudioShapePanel();
    void drawAudioPerspectivePanel();
    void drawStatusBar();
    void ensureScenePreviewInitialized();
    void drawEditorScaffold(const char* title, const char* description, const std::vector<const char*>& features);

    void buildPeugeotCaptureGrid();
    void updateCaptureAutoAdvance();
    void applyAudioPreset(audio::lab::EngineSoundPreset preset);
    void autoTuneAudio(audio::lab::EngineSoundPreset seedPreset);
    bool saveSceneAuthoring();
    bool saveAllAuthoring();
    bool loadSceneAuthoring();
    bool saveRuntimeVehicleSpawn(std::string& message) const;
    bool loadRuntimeVehicleSpawn(std::string& message);
    bool saveRuntimeGameplay(std::string& message) const;
    std::string validateAuthoring() const;
    int countCompletedCaptureTargets() const;
    void jumpToNextMissingCaptureTarget();
    bool lastCaptureQualityGood() const;
    void openPathInShell(const std::filesystem::path& path) const;
    static std::filesystem::path findRepositoryRoot();
    static std::filesystem::path executableDirectory();

    GLFWwindow* m_window = nullptr;
    Workspace m_workspace = Workspace::Audio;
    bool m_initialized = false;

    // STUDIO26: HeritageEngine-parity borderless dark chrome. Heritage Engine
    // does not rely on the OS caption; it creates an undecorated GLFW window
    // and owns its ImGui title bar, drag region and resize affordances.
    bool m_studioWindowMaximized = false;
    bool m_studioPendingRestore = false;
    bool m_studioWindowDragging = false;
    bool m_studioWindowResizing = false;
    int m_studioResizeDirection = 0; // bit 1 left, 2 right, 4 top, 8 bottom.
    int m_studioSavedX = 80;
    int m_studioSavedY = 80;
    int m_studioSavedW = 1500;
    int m_studioSavedH = 920;
    int m_studioDragStartWindowX = 0;
    int m_studioDragStartWindowY = 0;
    double m_studioDragStartCursorX = 0.0;
    double m_studioDragStartCursorY = 0.0;
    int m_studioResizeStartX = 0;
    int m_studioResizeStartY = 0;
    int m_studioResizeStartW = 0;
    int m_studioResizeStartH = 0;
    double m_studioResizeStartCursorX = 0.0;
    double m_studioResizeStartCursorY = 0.0;

    std::filesystem::path m_repositoryRoot;
    std::filesystem::path m_moduleRoot;
    std::filesystem::path m_moduleUserRoot;
    std::filesystem::path m_engineScriptPath;
    std::filesystem::path m_engineResearchPath;
    std::filesystem::path m_assetBrowserPath;
    std::filesystem::path m_selectedAssetPath;
    std::filesystem::path m_runtimeScenePath;

    audio::AudioSystem m_audio;
    std::unique_ptr<audio::lab::EngineSoundCaptureLab> m_soundLab;
    std::unique_ptr<StudioScenePreview> m_scenePreview;
    bool m_scenePreviewInitialized = false;
    std::string m_audioBackendMessage;

    std::vector<EngineCaptureTarget> m_captureTargets;
    int m_captureTargetIndex = 0;
    float m_captureDurationSeconds = 4.0f;
    bool m_autoAdvanceCapture = true;
    bool m_captureQualityGate = true;
    bool m_skipCompletedCaptureTargets = true;
    bool m_captureWasRunning = false;
    std::filesystem::path m_lastCompletedRawPath;
    char m_profileName[96] = "Peugeot206RC_EW10J4S_Stock";
    audio::lab::EngineSoundAcousticProfile m_profileUndo{};
    bool m_hasProfileUndo = false;
    std::string m_audioAssistantMessage = "Capture a representative Engine Simulator sample, then use AUTO PEUGEOT STOCK.";

    // Lightweight editable authoring-state scaffolds. These deliberately live
    // in Studio, not Racing United runtime, and will become serialized assets.
    authoring::StudioAuthoringData m_authoring;
    std::filesystem::path m_studioProjectRoot;
    std::string m_studioMessage = "Studio authoring data is ready.";
    int m_sceneSelectedObject = 0;
    int m_raceSelectedMarker = 0;
    int m_raceTab = 0; // 0 markers/grid, 1 routes, 2 layouts, 3 sessions, 4 race control, 5 broadcast cameras, 6 cone courses.
    int m_raceSelectedRoute = 0;
    int m_raceSelectedRouteNode = 0;
    int m_raceSelectedLayout = 0;
    int m_raceSelectedSession = 0;
    int m_raceSelectedSupportPoint = 0;
    int m_raceSelectedCameraPath = 0;
    int m_raceSelectedCameraNode = 0;
    int m_raceConeSelectionKind = 0; // 0 physical/traffic cone, 1 invisible course gate.
    int m_raceSelectedCone = 0;
    int m_raceSelectedConeGate = 0;
    std::uint32_t m_raceConeAuthorEventId = 0; // 0 = persistent free-roam overlay; new cones/gates inherit this.
    bool m_racePlaceRouteNode = false;
    int m_raceSupportPlacementType = -1;
    int m_raceConePlacementRole = -1;
    int m_raceConeGatePlacementType = -1;
    int m_trafficSelectedNode = 0;
    int m_trafficSelectedLink = 0;
    int m_trafficLinkTargetNode = 0;
    int m_trafficTab = 0; // 0 graph, 1 roads, 2 junctions/signals, 3 parking/population/nav, 4 operations/routing, 5 traffic agents, 6 portals/density/incidents.
    int m_trafficSelectedRoad = 0;
    int m_trafficSelectedRoadNode = 0;
    int m_trafficSelectedIntersection = 0;
    int m_trafficSelectedConnector = 0;
    int m_trafficSelectedSignalPhase = 0;
    int m_trafficSelectedParking = 0;
    int m_trafficSelectedRestriction = 0;
    int m_trafficRestrictionTypeToAdd = 0;
    int m_trafficSelectedAgentProfile = 0;
    int m_trafficAgentClassToAdd = 1;
    int m_trafficSelectedPortal = 0;
    int m_trafficSelectedDensityRegion = 0;
    int m_trafficSelectedIncident = 0;
    int m_trafficIncidentTypeToAdd = 0;
    bool m_trafficPlacePortal = false;
    bool m_trafficPlaceDensityRegion = false;
    bool m_trafficPlaceIncident = false;
    int m_trafficRoadClassToAdd = 3;
    int m_trafficConnectorFromRoad = 0;
    int m_trafficConnectorToRoad = 0;
    bool m_trafficPlaceRoadNode = false;
    bool m_trafficPlaceIntersection = false;
    bool m_trafficPlaceParking = false;
    int m_gameplaySelectedEvent = 0;
    int m_gameplaySelectedWorldPoint = 0;
    int m_gameplayTab = 0; // 0 events, 1 world points, 2 police / underground, 3 event runtime / practice, 4 competitors / series.
    int m_gameplayCompetitionSelectionKind = 0; // 0 class, 1 entrant, 2 championship, 3 round.
    int m_gameplayCompetitionSelectedIndex = 0;
    int m_gameplayPoliceSelectionKind = 0; // 0 patrol, 1 roadblock, 2 escape, 3 meet.
    int m_gameplayPoliceSelectedIndex = 0;

    // STUDIO06 shared interactive 3D authoring viewport state. Scene, race and
    // traffic workspaces deliberately share the same camera so switching tools
    // does not lose spatial context.
    float m_viewYawDeg = 42.0f;
    float m_viewPitchDeg = 32.0f;
    float m_viewDistanceM = 55.0f;
    authoring::Vec3 m_viewTarget{};
    bool m_viewGridVisible = true;
    bool m_viewSnapEnabled = true;
    float m_viewSnapM = 0.5f;
    bool m_viewOrthographic = false;
    bool m_sceneGeometryVisible = true;
    bool m_sceneGeometryWireframe = false;
    float m_sceneGeometryExposure = 1.0f;
    bool m_viewToolbarVisible = true;
    bool m_viewSidebarVisible = true;
    int m_viewTool = 0; // Blender-style: 0 select, 1 move, 2 rotate, 3 scale.
    int m_viewGizmoAxis = -1;
    float m_viewGizmoDragAccumulator = 0.0f;
    StudioViewportFlyState m_viewFly{};

    // STUDIO07 Blender 5.2-compatible modal transform state. G/R/S starts a
    // transform, X/Y/Z constrains it, LMB/Enter confirms, RMB/Esc cancels.
    int m_viewTransformMode = 0; // 0 none, 1 grab, 2 rotate, 3 scale.
    int m_viewTransformAxis = -1;
    float m_viewTransformStartMouseX = 0.0f;
    float m_viewTransformStartMouseY = 0.0f;
    authoring::Vec3 m_viewTransformStartPosition{};
    authoring::Vec3 m_viewTransformStartRotation{};
    authoring::Vec3 m_viewTransformStartScale{ 1.0f, 1.0f, 1.0f };
    int m_scenePlacementType = -1;
    int m_racePlacementType = -1;
    int m_trafficPlacementType = -1;
    int m_gameplayPlacementType = -1;
    int m_gameplayPolicePlacementKind = -1;

    int m_raceLaps = 3;
    int m_gridSlots = 24;
    float m_pitSpeedKmh = 60.0f;
};

} // namespace heritage::studio
