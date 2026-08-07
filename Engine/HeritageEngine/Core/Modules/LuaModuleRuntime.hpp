#pragma once

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
#include "../../Audio/AudioSystem.hpp"
#include "../Entities/EntityRegistry.hpp"
#include "../Entities/EntityPrefabDocument.hpp"
#include "../../Input/InputSystem.hpp"
#include "../../Physics/PhysicsWorld.hpp"
#include "../../Scenes/SceneManager.hpp"
#include "../../UI/UiImageCache.hpp"

namespace heritage::modules {

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
    bool createState(std::string& errorMessage);
    bool loadEntryScript(std::string& errorMessage);
    bool reloadScript(std::string& errorMessage);
    void destroyState(bool callShutdownFunction);

    void registerBindings();
    void sandboxStandardLibraries();
    void registerFunction(
        const char* tableName,
        const char* functionName,
        LuaCFunction function);
    void writeLuaApiManifest() const;
    bool runSafetySmokeTests(std::string& summary, std::filesystem::path& reportPath);
    void clearImportedStaticBoxScene();
    void replaceGlobalWithNil(const char* name);

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
    static heritage::entities::EntityHandle entityHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static heritage::physics::BodyHandle bodyHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static heritage::physics::ColliderHandle colliderHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static heritage::physics::ConstraintHandle constraintHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);
    static heritage::vehicles::VehicleHandle vehicleHandleArgument(
        LuaModuleRuntime& runtime,
        lua_State* state,
        int index);

    static int luaPrint(lua_State* state);

    static int luaUiBeginPanel(lua_State* state);
    static int luaUiEndPanel(lua_State* state);
    static int luaUiBeginTabBar(lua_State* state);
    static int luaUiEndTabBar(lua_State* state);
    static int luaUiBeginTabItem(lua_State* state);
    static int luaUiEndTabItem(lua_State* state);
    static int luaUiModuleLabel(lua_State* state);
    static int luaUiTitle(lua_State* state);
    static int luaUiSubtitle(lua_State* state);
    static int luaUiText(lua_State* state);
    static int luaUiTextWrapped(lua_State* state);
    static int luaUiTextDisabled(lua_State* state);
    static int luaUiSeparator(lua_State* state);
    static int luaUiSpacing(lua_State* state);
    static int luaUiSameLine(lua_State* state);
    static int luaUiButton(lua_State* state);
    static int luaUiSliderFloat(lua_State* state);
    static int luaUiCheckbox(lua_State* state);
    static int luaUiInputInt(lua_State* state);
    static int luaUiImage(lua_State* state);
    static int luaUiImageButton(lua_State* state);
    static int luaUiGetImageSize(lua_State* state);
    static int luaUiUnloadImage(lua_State* state);
    static int luaUiGetLastError(lua_State* state);
    static int luaUiSetCursorPos(lua_State* state);
    static int luaUiGetCursorPos(lua_State* state);
    static int luaUiDummy(lua_State* state);
    static int luaUiTextColored(lua_State* state);
    static int luaUiProgressBar(lua_State* state);
    static int luaUiPlotLines(lua_State* state);

    static int luaEngineOpenSettings(lua_State* state);
    static int luaEngineExit(lua_State* state);
    static int luaEngineSetClearColor(lua_State* state);
    static int luaEngineLog(lua_State* state);
    static int luaEngineGetBuildIdentity(lua_State* state);
    static int luaEngineGetBuildStep(lua_State* state);
    static int luaEngineGetGitCommit(lua_State* state);
    static int luaEngineGetBuildConfiguration(lua_State* state);
    static int luaEngineGetLuaApiCount(lua_State* state);
    static int luaEngineGetLuaApiName(lua_State* state);
    static int luaEngineDumpLuaAPI(lua_State* state);
    static int luaEngineRunSafetySmokeTests(lua_State* state);
    static int luaEngineGetLastSafetyReport(lua_State* state);

    static int luaScriptInclude(lua_State* state);

    static int luaSceneLoad(lua_State* state);
    static int luaSceneReload(lua_State* state);
    static int luaSceneGetCurrent(lua_State* state);
    static int luaSceneExists(lua_State* state);
    static int luaSceneGetLastError(lua_State* state);

    static int luaSaveGetString(lua_State* state);
    static int luaSaveSetString(lua_State* state);
    static int luaSaveGetInt(lua_State* state);
    static int luaSaveSetInt(lua_State* state);
    static int luaSaveGetNumber(lua_State* state);
    static int luaSaveSetNumber(lua_State* state);
    static int luaSaveGetBool(lua_State* state);
    static int luaSaveSetBool(lua_State* state);
    static int luaSaveHas(lua_State* state);
    static int luaSaveRemove(lua_State* state);
    static int luaSaveClear(lua_State* state);
    static int luaSaveFlush(lua_State* state);
    static int luaSaveGetPath(lua_State* state);
    static int luaSaveGetLastError(lua_State* state);
    static int luaSaveIsDirty(lua_State* state);

    static int luaAudioIsAvailable(lua_State* state);
    static int luaAudioGetBackend(lua_State* state);
    static int luaAudioPlaySound(lua_State* state);
    static int luaAudioPlayLoop(lua_State* state);
    static int luaAudioStop(lua_State* state);
    static int luaAudioStopAll(lua_State* state);
    static int luaAudioIsPlaying(lua_State* state);
    static int luaAudioSetVolume(lua_State* state);
    static int luaAudioSetPitch(lua_State* state);
    static int luaAudioSetMasterVolume(lua_State* state);
    static int luaAudioGetMasterVolume(lua_State* state);
    static int luaAudioSetBusVolume(lua_State* state);
    static int luaAudioGetBusVolume(lua_State* state);
    static int luaAudioGetLastError(lua_State* state);

    static int luaInputIsAvailable(lua_State* state);
    static int luaInputRegisterAction(lua_State* state);
    static int luaInputDown(lua_State* state);
    static int luaInputPressed(lua_State* state);
    static int luaInputReleased(lua_State* state);
    static int luaInputValue(lua_State* state);
    static int luaInputGetBinding(lua_State* state);
    static int luaInputGetBindingCount(lua_State* state);
    static int luaInputGetBindingAt(lua_State* state);
    static int luaInputBind(lua_State* state);
    static int luaInputAddBinding(lua_State* state);
    static int luaInputRemoveBinding(lua_State* state);
    static int luaInputResetBinding(lua_State* state);
    static int luaInputResetBindings(lua_State* state);
    static int luaInputKeyDown(lua_State* state);
    static int luaInputKeyPressed(lua_State* state);
    static int luaInputKeyReleased(lua_State* state);
    static int luaInputMouseDown(lua_State* state);
    static int luaInputMousePressed(lua_State* state);
    static int luaInputMouseReleased(lua_State* state);
    static int luaInputMouseDelta(lua_State* state);
    static int luaInputGamepadConnected(lua_State* state);
    static int luaInputGetGamepadName(lua_State* state);
    static int luaInputGetLastError(lua_State* state);


    static int luaPhysicsIsAvailable(lua_State* state);
    static int luaPhysicsGetFixedDelta(lua_State* state);
    static int luaPhysicsGetTickRate(lua_State* state);
    static int luaPhysicsSetTickRate(lua_State* state);
    static int luaPhysicsGetGravity(lua_State* state);
    static int luaPhysicsSetGravity(lua_State* state);
    static int luaPhysicsIsPaused(lua_State* state);
    static int luaPhysicsSetPaused(lua_State* state);
    static int luaPhysicsRequestSingleStep(lua_State* state);
    static int luaPhysicsGetTimeScale(lua_State* state);
    static int luaPhysicsSetTimeScale(lua_State* state);
    static int luaPhysicsGetStepCount(lua_State* state);
    static int luaPhysicsGetSimulationTime(lua_State* state);
    static int luaPhysicsGetInterpolationAlpha(lua_State* state);
    static int luaPhysicsGetLastSubstepCount(lua_State* state);
    static int luaPhysicsGetMaximumWorldStepsPerFrame(lua_State* state);
    static int luaPhysicsGetPendingWorldStepCount(lua_State* state);
    static int luaPhysicsGetBacklogTime(lua_State* state);
    static int luaPhysicsGetPeakBacklogTime(lua_State* state);
    static int luaPhysicsWasOverloadedLastFrame(lua_State* state);
    static int luaPhysicsGetOverloadFrameCount(lua_State* state);
    static int luaPhysicsGetDroppedTime(lua_State* state);
    static int luaPhysicsGetClampedTime(lua_State* state);
    static int luaPhysicsResetClock(lua_State* state);

    static int luaPhysicsCreateBody(lua_State* state);
    static int luaPhysicsDestroyBody(lua_State* state);
    static int luaPhysicsBodyExists(lua_State* state);
    static int luaPhysicsGetBodyCount(lua_State* state);
    static int luaPhysicsGetSleepingBodyCount(lua_State* state);
    static int luaPhysicsGetActiveDynamicBodyCount(lua_State* state);
    static int luaPhysicsFindBodyByEntity(lua_State* state);
    static int luaPhysicsGetBodyEntity(lua_State* state);
    static int luaPhysicsGetBodyMotionType(lua_State* state);
    static int luaPhysicsSetBodyMotionType(lua_State* state);
    static int luaPhysicsGetBodyMass(lua_State* state);
    static int luaPhysicsSetBodyMass(lua_State* state);
    static int luaPhysicsGetBodyGravityFactor(lua_State* state);
    static int luaPhysicsSetBodyGravityFactor(lua_State* state);
    static int luaPhysicsGetBodyLinearDamping(lua_State* state);
    static int luaPhysicsSetBodyLinearDamping(lua_State* state);
    static int luaPhysicsGetBodyAngularDamping(lua_State* state);
    static int luaPhysicsSetBodyAngularDamping(lua_State* state);
    static int luaPhysicsGetBodyContinuousCollision(lua_State* state);
    static int luaPhysicsSetBodyContinuousCollision(lua_State* state);
    static int luaPhysicsGetBodyPosition(lua_State* state);
    static int luaPhysicsSetBodyPosition(lua_State* state);
    static int luaPhysicsGetBodyRotation(lua_State* state);
    static int luaPhysicsSetBodyRotation(lua_State* state);
    static int luaPhysicsGetBodyLinearVelocity(lua_State* state);
    static int luaPhysicsSetBodyLinearVelocity(lua_State* state);
    static int luaPhysicsGetBodyAngularVelocity(lua_State* state);
    static int luaPhysicsSetBodyAngularVelocity(lua_State* state);
    static int luaPhysicsApplyBodyForce(lua_State* state);
    static int luaPhysicsApplyBodyImpulse(lua_State* state);
    static int luaPhysicsApplyBodyImpulseAtPoint(lua_State* state);
    static int luaPhysicsApplyBodyAngularImpulse(lua_State* state);
    static int luaPhysicsClearBodyForces(lua_State* state);
    static int luaPhysicsIsBodySleeping(lua_State* state);
    static int luaPhysicsSetBodySleeping(lua_State* state);
    static int luaPhysicsGetBodyAllowSleep(lua_State* state);
    static int luaPhysicsSetBodyAllowSleep(lua_State* state);
    static int luaPhysicsWakeBody(lua_State* state);

    static int luaPhysicsCreateSphereCollider(lua_State* state);
    static int luaPhysicsCreateBoxCollider(lua_State* state);
    static int luaPhysicsLoadStaticBoxScene(lua_State* state);
    static int luaPhysicsUnloadStaticBoxScene(lua_State* state);
    static int luaPhysicsLoadStaticTriangleScene(lua_State* state);
    static int luaPhysicsUnloadStaticTriangleScene(lua_State* state);
    static int luaPhysicsGetStaticTriangleSceneCount(lua_State* state);
    static int luaPhysicsGetStaticBoxSceneCount(lua_State* state);
    static int luaPhysicsDestroyCollider(lua_State* state);
    static int luaPhysicsColliderExists(lua_State* state);
    static int luaPhysicsGetColliderCount(lua_State* state);
    static int luaPhysicsGetBodyColliderCount(lua_State* state);
    static int luaPhysicsGetColliderBody(lua_State* state);
    static int luaPhysicsGetColliderShape(lua_State* state);
    static int luaPhysicsSetColliderMaterial(lua_State* state);
    static int luaPhysicsSetColliderSurface(lua_State* state);
    static int luaPhysicsGetColliderSurface(lua_State* state);
    static int luaPhysicsSetColliderTrigger(lua_State* state);
    static int luaPhysicsSetColliderFilter(lua_State* state);
    static int luaPhysicsRaycast(lua_State* state);
    static int luaPhysicsRaycastAny(lua_State* state);
    static int luaPhysicsSphereCast(lua_State* state);
    static int luaPhysicsSphereCastAny(lua_State* state);
    static int luaPhysicsOverlapSphereCount(lua_State* state);
    static int luaPhysicsGetLastQueryCandidateCount(lua_State* state);
    static int luaPhysicsGetLastQueryExactTestCount(lua_State* state);
    static int luaPhysicsGetContactCount(lua_State* state);
    static int luaPhysicsGetBodyContactCount(lua_State* state);
    static int luaPhysicsIsBodyTouching(lua_State* state);
    static int luaPhysicsGetBroadphaseCandidateCount(lua_State* state);
    static int luaPhysicsGetNarrowphaseTestCount(lua_State* state);
    static int luaPhysicsGetResolvedContactCount(lua_State* state);
    static int luaPhysicsGetSimulationIslandCount(lua_State* state);
    static int luaPhysicsGetActiveIslandCount(lua_State* state);
    static int luaPhysicsGetSleepingIslandCount(lua_State* state);
    static int luaPhysicsGetWarmStartedContactCount(lua_State* state);
    static int luaPhysicsGetPersistentContactCount(lua_State* state);
    static int luaPhysicsGetContinuousCollisionBodyCount(lua_State* state);
    static int luaPhysicsGetContinuousCollisionSweepCount(lua_State* state);
    static int luaPhysicsGetContinuousCollisionHitCount(lua_State* state);
    static int luaPhysicsGetContinuousCollisionClampedBodyCount(lua_State* state);
    static int luaPhysicsGetContinuousCollisionUnsupportedBodyCount(lua_State* state);

    static int luaPhysicsCreateSpringConstraint(lua_State* state);
    static int luaPhysicsDestroyConstraint(lua_State* state);
    static int luaPhysicsConstraintExists(lua_State* state);
    static int luaPhysicsGetConstraintCount(lua_State* state);
    static int luaPhysicsGetEnabledConstraintCount(lua_State* state);
    static int luaPhysicsGetActiveConstraintCount(lua_State* state);
    static int luaPhysicsSetConstraintEnabled(lua_State* state);
    static int luaPhysicsGetConstraintEnabled(lua_State* state);
    static int luaPhysicsSetSpringConstraintProperties(lua_State* state);
    static int luaPhysicsGetSpringConstraintState(lua_State* state);
    static int luaPhysicsGetConstraintBodyA(lua_State* state);
    static int luaPhysicsGetConstraintBodyB(lua_State* state);

    static int luaPhysicsGetLastError(lua_State* state);

    static int luaVehicleIsAvailable(lua_State* state);
    static int luaVehicleCreate(lua_State* state);
    static int luaVehicleDestroy(lua_State* state);
    static int luaVehicleExists(lua_State* state);
    static int luaVehicleGetCount(lua_State* state);
    static int luaVehicleAddWheel(lua_State* state);
    static int luaVehicleGetWheelCount(lua_State* state);
    static int luaVehicleSetInputs(lua_State* state);
    static int luaVehicleSetWheelBrakeFactors(lua_State* state);
    static int luaVehicleSetDriverAids(lua_State* state);
    static int luaVehicleGetDriverAidState(lua_State* state);
    static int luaVehicleSetTuning(lua_State* state);
    static int luaVehicleSetTireModel(lua_State* state);
    static int luaVehicleSetWheelTireModel(lua_State* state);
    static int luaVehicleGetWheelTireModel(lua_State* state);
    static int luaVehicleSetSurfacePreset(lua_State* state);
    static int luaVehicleGetSurfacePreset(lua_State* state);
    static int luaVehicleSetHighRateHertz(lua_State* state);
    static int luaVehicleSetSteeringGeometry(lua_State* state);
    static int luaVehicleGetSteeringState(lua_State* state);
    static int luaVehicleSetPowertrain(lua_State* state);
    static int luaVehicleSetGearRatios(lua_State* state);
    static int luaVehicleSetDifferential(lua_State* state);
    static int luaVehicleSetGear(lua_State* state);
    static int luaVehicleShiftUp(lua_State* state);
    static int luaVehicleShiftDown(lua_State* state);
    static int luaVehicleGetDrivetrainState(lua_State* state);
    static int luaVehicleGetForwardGearCount(lua_State* state);
    static int luaVehicleGetHighRateHertz(lua_State* state);
    static int luaVehicleGetSpeed(lua_State* state);
    static int luaVehicleGetGroundedWheelCount(lua_State* state);
    static int luaVehicleGetLastHighRateStepCount(lua_State* state);
    static int luaVehicleGetTotalHighRateStepCount(lua_State* state);
    static int luaVehicleStartDynamicsLab(lua_State* state);
    static int luaVehicleStopDynamicsLab(lua_State* state);
    static int luaVehicleClearDynamicsLab(lua_State* state);
    static int luaVehicleGetDynamicsLabSummary(lua_State* state);
    static int luaVehicleGetDynamicsLabSeries(lua_State* state);
    static int luaVehicleExportDynamicsLabCsv(lua_State* state);
    static int luaVehicleGetWheelState(lua_State* state);
    static int luaVehicleGetLastError(lua_State* state);


    static int luaEntityIsAvailable(lua_State* state);
    static int luaEntityCreate(lua_State* state);
    static int luaEntityDestroy(lua_State* state);
    static int luaEntityExists(lua_State* state);
    static int luaEntityCount(lua_State* state);
    static int luaEntityGetPersistentId(lua_State* state);
    static int luaEntityFindByName(lua_State* state);
    static int luaEntitySetName(lua_State* state);
    static int luaEntityGetName(lua_State* state);
    static int luaEntityAddTag(lua_State* state);
    static int luaEntityRemoveTag(lua_State* state);
    static int luaEntityHasTag(lua_State* state);
    static int luaEntityFindFirstWithTag(lua_State* state);
    static int luaEntitySetParent(lua_State* state);
    static int luaEntityClearParent(lua_State* state);
    static int luaEntityGetParent(lua_State* state);
    static int luaEntityGetChildCount(lua_State* state);
    static int luaEntityGetChildAt(lua_State* state);
    static int luaEntityIsDescendantOf(lua_State* state);
    static int luaEntitySetPosition(lua_State* state);
    static int luaEntityGetPosition(lua_State* state);
    static int luaEntitySetWorldPosition(lua_State* state);
    static int luaEntityGetWorldPosition(lua_State* state);
    static int luaEntitySetRotation(lua_State* state);
    static int luaEntityGetRotation(lua_State* state);
    static int luaEntitySetWorldRotation(lua_State* state);
    static int luaEntityGetWorldRotation(lua_State* state);
    static int luaEntitySetScale(lua_State* state);
    static int luaEntityGetScale(lua_State* state);
    static int luaEntitySetWorldScale(lua_State* state);
    static int luaEntityGetWorldScale(lua_State* state);
    static int luaEntitySetDebugPrimitive(lua_State* state);
    static int luaEntityRemoveDebugPrimitive(lua_State* state);
    static int luaEntityHasDebugPrimitive(lua_State* state);
    static int luaEntitySetDebugVisible(lua_State* state);
    static int luaEntitySetDebugColor(lua_State* state);
    static int luaEntityGetDebugPrimitive(lua_State* state);
    static int luaEntitySetMesh(lua_State* state);
    static int luaEntityRemoveMesh(lua_State* state);
    static int luaEntityHasMesh(lua_State* state);
    static int luaEntitySetMeshVisible(lua_State* state);
    static int luaEntitySetMeshColor(lua_State* state);
    static int luaEntitySetMeshNormalize(lua_State* state);
    static int luaEntitySetMeshDoubleSided(lua_State* state);
    static int luaEntityGetMesh(lua_State* state);
    static int luaEntityGetLastError(lua_State* state);

    static int luaPrefabIsAvailable(lua_State* state);
    static int luaPrefabExists(lua_State* state);
    static int luaPrefabInstantiate(lua_State* state);
    static int luaPrefabGetLastError(lua_State* state);

    static int luaModuleId(lua_State* state);
    static int luaModuleName(lua_State* state);
    static int luaModuleVersion(lua_State* state);
    static int luaModuleAssetPath(lua_State* state);
    static int luaModuleDataPath(lua_State* state);
    static int luaModuleSavePath(lua_State* state);

    LuaApi m_api;
    lua_State* m_state = nullptr;
    GLFWwindow* m_window = nullptr;
    heritage::audio::AudioSystem* m_audio = nullptr;
    heritage::input::InputSystem* m_input = nullptr;
    heritage::entities::EntityRegistry* m_entities = nullptr;
    heritage::physics::PhysicsWorld* m_physics = nullptr;
    std::unordered_set<heritage::audio::AudioHandle> m_audioHandles;
    std::string m_lastAudioError;
    std::optional<ModuleContext> m_context;
    std::filesystem::path m_scriptPath;
    std::unordered_map<std::string, std::filesystem::file_time_type>
        m_scriptWriteTimes;

    std::deque<ModuleRuntimeAction> m_actions;
    heritage::scenes::SceneManager m_sceneManager;
    ModuleSaveStore m_saveStore;
    heritage::ui::UiImageCache m_uiImages;
    std::string m_lastUiError;
    std::string m_lastPrefabError;
    std::string m_lastPhysicsError;
    std::vector<heritage::physics::BodyHandle> m_importedStaticSceneBodies;
    std::vector<heritage::entities::EntityHandle> m_importedStaticSceneEntities;
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

    int m_framebufferWidth = 1;
    int m_framebufferHeight = 1;
    bool m_panelOpen = false;
    bool m_panelVisible = false;
    std::string m_numericSliderInputLabel;
    bool m_numericSliderFocusRequested = false;
    std::vector<UiScopeType> m_uiScopes;
};

} // namespace heritage::modules
