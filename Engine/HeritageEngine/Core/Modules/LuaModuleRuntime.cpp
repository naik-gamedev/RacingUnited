#include "LuaModuleRuntime.hpp"
#include "LuaBindings/LuaBindingInternals.hpp"
#include "../../Audio/AudioSystem.hpp"
#include "../Entities/EntityRegistry.hpp"
#include "../../Input/InputSystem.hpp"
#include "../../Physics/PhysicsWorld.hpp"
#include "../../Graphics/EnvironmentSystem.hpp"
#include "../../Graphics/VegetationSystem.hpp"
#include "../Diagnostics/BuildIdentity.hpp"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace heritage::modules {
using namespace lua_binding_detail;
namespace {

std::unordered_map<lua_State*, LuaModuleRuntime*> g_runtimeByState;

std::string jsonEscape(const std::string& value)
{
    std::ostringstream output;
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '\\': output << "\\\\"; break;
        case '"': output << "\\\""; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (character < 0x20)
            {
                output << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<int>(character)
                    << std::dec << std::setfill(' ');
            }
            else
            {
                output << static_cast<char>(character);
            }
            break;
        }
    }
    return output.str();
}

} // namespace

bool LuaModuleRuntime::onLoad(
    GLFWwindow* window,
    const ModuleContext& context,
    const ModuleRuntimeServices& services,
    std::string& message)
{
    onShutdown();

    m_window = window;
    m_audio = services.audio;
    m_input = services.input;
    m_vehicleCamera = services.vehicleCamera;
    m_entities = services.entities;
    m_physics = services.physics;
    m_environment = services.environment;
    m_vegetation = services.vegetation;
    m_context.emplace(context);
    m_assetRegistry.reset(context.assetRoot());
    // AS01A: do not walk the filesystem during module startup. The registry
    // performs its first failure-isolated scan from onUpdate after the window
    // has been alive for roughly one second.
    m_loaded = true;

    std::string saveWarning;
    if (!m_saveStore.initialize(context, saveWarning))
    {
        std::cerr << "[Save:" << context.module().id << "] "
            << saveWarning << '\n';
    }
    else if (!saveWarning.empty())
    {
        std::cerr << "[Save:" << context.module().id << "] "
            << saveWarning << " Starting with an empty save store.\n";
    }

    if (context.module().entryScript.empty())
    {
        setScriptError(
            "Module '" + context.module().id
            + "' requested runtime 'lua' but has no entry_script.");
        message = m_scriptError;
        return true;
    }

    m_scriptPath = context.resolveScriptPath(context.module().entryScript);
    if (m_scriptPath.empty())
    {
        setScriptError(
            "Module '" + context.module().id
            + "' has an unsafe entry_script path: "
            + context.module().entryScript);
        message = m_scriptError;
        return true;
    }

    std::string loadError;
    if (!createState(loadError) || !loadEntryScript(loadError))
    {
        setScriptError(loadError);
        message = m_scriptError;
        return true;
    }

    message.clear();
    return true;
}

void LuaModuleRuntime::onStart()
{
    if (!m_loaded)
        return;

    m_started = true;
    if (m_state && m_scriptError.empty())
        callOptionalNoArgs("OnStart");

    // Lua may choose a scene in OnStart. If it does not, the optional
    // manifest entry_scene becomes the initial scene.
    if (!m_pendingSceneId
        && m_sceneManager.activeSceneId().empty()
        && m_context
        && !m_context->module().scene.empty())
    {
        requestSceneLoad(m_context->module().scene, false);
    }

    processPendingSceneTransition();
}

void LuaModuleRuntime::onFixedUpdate(float fixedDeltaTime)
{
    if (m_started && m_state && m_scriptError.empty())
        callOptionalNumber("OnFixedUpdate", static_cast<LuaNumber>(fixedDeltaTime));
}

void LuaModuleRuntime::onUpdate(float deltaTime, bool allowInteraction)
{
    m_allowInteraction = allowInteraction;
    m_assetRegistry.update(deltaTime);

    const bool f5Down = m_window
        && glfwGetKey(m_window, GLFW_KEY_F5) == GLFW_PRESS;
    const bool requestedManualReload = f5Down && !m_f5WasDown;
    m_f5WasDown = f5Down;

    // Development reload is explicit. The old 0.5 s recursive Scripts scan was
    // convenient but could hitch the main thread at a regular cadence. F5 now
    // refreshes both the asset index and Lua in one intentional authoring hitch.
    const bool changedOnDisk = false;
    if (requestedManualReload || changedOnDisk)
    {
        if (requestedManualReload)
            m_assetRegistry.forceRefresh();

        std::string reloadError;
        if (reloadScript(reloadError))
        {
            m_lastReloadMessage = "Lua module scripts reloaded from: " + m_scriptPath.parent_path().string();
            std::cout << m_lastReloadMessage << '\n';
        }
        else
        {
            setScriptError(reloadError);
            std::cerr << m_scriptError << '\n';
        }
    }

    processPendingSceneTransition();

    if (m_started)
        m_sceneManager.update(deltaTime, allowInteraction);

    if (m_started && m_state && m_scriptError.empty())
        callOptionalNumber("OnUpdate", static_cast<LuaNumber>(deltaTime));

    // Scene.Load() is queued. Applying it here prevents a script callback from
    // destroying the active scene while that scene is updating or drawing.
    processPendingSceneTransition();

    if (m_audio)
    {
        for (auto iterator = m_audioHandles.begin(); iterator != m_audioHandles.end(); )
        {
            if (!m_audio->isPlaying(*iterator))
                iterator = m_audioHandles.erase(iterator);
            else
                ++iterator;
        }
    }

    m_saveFlushTimer += (std::max)(0.0f, deltaTime);
    if (m_saveStore.isDirty() && m_saveFlushTimer >= 1.0f)
    {
        m_saveFlushTimer = 0.0f;
        if (!m_saveStore.flush())
        {
            std::cerr << "[Save:"
                << (m_context ? m_context->module().id : "?")
                << "] " << m_saveStore.lastError() << '\n';
        }
    }
}

heritage::math::Vec3 LuaModuleRuntime::clearColor() const
{
    return m_sceneManager.activeSceneId().empty()
        ? m_clearColor
        : m_sceneManager.clearColor();
}

void LuaModuleRuntime::onRender(
    const heritage::math::Mat4& projection,
    const heritage::settings::VideoSettings& videoSettings) const
{
    if (m_started)
        m_sceneManager.draw(projection, videoSettings);
}

void LuaModuleRuntime::onDrawUI(
    int framebufferWidth,
    int framebufferHeight)
{
    m_framebufferWidth = (std::max)(framebufferWidth, 1);
    m_framebufferHeight = (std::max)(framebufferHeight, 1);

    if (!m_scriptError.empty() || !m_state)
    {
        drawRuntimeError(m_framebufferWidth, m_framebufferHeight);
        return;
    }

    // Scene overlays are drawn first so module Lua UI remains the top layer.
    m_sceneManager.drawOverlay(m_framebufferWidth, m_framebufferHeight);

    m_panelOpen = false;
    m_panelVisible = false;
    callOptionalTwoIntegers(
        "OnDrawUI",
        static_cast<LuaInteger>(m_framebufferWidth),
        static_cast<LuaInteger>(m_framebufferHeight));

    // A broken module script must not corrupt ImGui's Begin/End stack.
    closeOpenPanel();
}

void LuaModuleRuntime::onShutdown()
{
    destroyState(true);
    clearImportedStaticBoxScene();
    if (m_physics)
        m_physics->collisions().clearStaticSceneTriangles();

    if (m_audio)
    {
        for (const heritage::audio::AudioHandle handle : m_audioHandles)
            m_audio->stop(handle);
    }
    m_audioHandles.clear();
    m_lastAudioError.clear();
    m_uiImages.clear();
    m_lastUiError.clear();
    m_lastPrefabError.clear();
    m_lastPhysicsError.clear();

    if (m_saveStore.isDirty() && !m_saveStore.flush())
    {
        std::cerr << "[Save:"
            << (m_context ? m_context->module().id : "?")
            << "] " << m_saveStore.lastError() << '\n';
    }

    m_sceneManager.shutdown();
    m_api.unload();

    m_actions.clear();
    m_pendingSceneId.reset();
    m_pendingSceneReload = false;
    m_lastSceneError.clear();
    m_saveStore.reset();
    m_assetRegistry.reset();
    m_context.reset();
    m_window = nullptr;
    m_audio = nullptr;
    m_input = nullptr;
    m_entities = nullptr;
    m_physics = nullptr;
    m_environment = nullptr;
    m_scriptPath.clear();
    m_scriptError.clear();
    m_lastReloadMessage.clear();
    m_registeredLuaFunctions.clear();
    m_lastSafetyReport.clear();
    m_lastSafetyReportPath.clear();
    m_clearColor = { 0.003f, 0.005f, 0.008f };
    m_reloadCheckTimer = 0.0f;
    m_saveFlushTimer = 0.0f;
    m_loaded = false;
    m_started = false;
    m_allowInteraction = true;
    m_f5WasDown = false;
    m_framebufferWidth = 1;
    m_framebufferHeight = 1;
    m_panelOpen = false;
    m_panelVisible = false;
    m_uiLayoutEditing = false;
    m_activePanelId.clear();
    m_uiPanelPlacements.clear();
    m_numericSliderInputLabel.clear();
    m_numericSliderFocusRequested = false;
}

void LuaModuleRuntime::clearImportedStaticBoxScene()
{
    if (m_physics)
    {
        for (const heritage::physics::BodyHandle body : m_importedStaticSceneBodies)
        {
            if (body != heritage::physics::InvalidBody
                && m_physics->rigidBodies().exists(body))
            {
                m_physics->destroyBody(body);
            }
        }
    }

    if (m_entities)
    {
        for (const heritage::entities::EntityHandle entity : m_importedStaticSceneEntities)
        {
            if (entity != heritage::entities::InvalidEntity
                && m_entities->exists(entity))
            {
                m_entities->destroy(entity);
            }
        }
    }

    m_importedStaticSceneBodies.clear();
    m_importedStaticSceneEntities.clear();
}

bool LuaModuleRuntime::pollAction(ModuleRuntimeAction& action)
{
    if (m_actions.empty())
        return false;

    action = std::move(m_actions.front());
    m_actions.pop_front();
    return true;
}

std::string LuaModuleRuntime::activeContentId() const
{
    const std::string script = m_scriptPath.empty()
        ? "<none>"
        : m_scriptPath.string();
    const std::string scene = m_sceneManager.activeSceneId().empty()
        ? "<none>"
        : m_sceneManager.activeSceneId();
    return script + " | scene=" + scene;
}

bool LuaModuleRuntime::createState(std::string& errorMessage)
{
    if (!m_context)
    {
        errorMessage = "Lua runtime has no ModuleContext.";
        return false;
    }

    if (!m_api.load(m_context->projectRoot(), errorMessage))
        return false;

    m_state = m_api.luaL_newstate();
    if (!m_state)
    {
        errorMessage = "Lua could not allocate a new interpreter state.";
        return false;
    }

    g_runtimeByState[m_state] = this;
    m_api.luaL_openlibs(m_state);
    sandboxStandardLibraries();
    m_registeredLuaFunctions.clear();
    registerBindings();
    writeLuaApiManifest();

    errorMessage.clear();
    return true;
}

bool LuaModuleRuntime::loadEntryScript(std::string& errorMessage)
{
    if (!m_state)
    {
        errorMessage = "Lua state is not initialized.";
        return false;
    }

    if (!std::filesystem::is_regular_file(m_scriptPath))
    {
        errorMessage = "Lua entry script was not found:\n" + m_scriptPath.string();
        return false;
    }

    const int loadStatus = m_api.luaL_loadfilex(
        m_state,
        m_scriptPath.string().c_str(),
        "t"); // Text only: module bytecode is deliberately not accepted.
    if (loadStatus != kLuaOk)
    {
        errorMessage = "Lua syntax/load error in " + m_scriptPath.string()
            + ":\n" + stackString(-1);
        pop(1);
        return false;
    }

    if (!protectedCall(0, 0, "loading the entry script"))
    {
        errorMessage = m_scriptError;
        return false;
    }

    refreshScriptWatchSnapshot();
    clearScriptError();
    errorMessage.clear();
    return true;
}

bool LuaModuleRuntime::reloadScript(std::string& errorMessage)
{
    const bool restartLifecycle = m_started;
    destroyState(true);

    // A reloaded script must not lose control of sounds created by the old
    // state. Stop every module-owned voice before creating the new state.
    if (m_audio)
    {
        for (const heritage::audio::AudioHandle handle : m_audioHandles)
            m_audio->stop(handle);
    }
    m_audioHandles.clear();
    m_uiImages.clear();
    m_lastUiError.clear();

    clearScriptError();

    if (!createState(errorMessage) || !loadEntryScript(errorMessage))
        return false;

    if (restartLifecycle)
        callOptionalNoArgs("OnStart");

    errorMessage = m_scriptError;
    return m_scriptError.empty();
}

void LuaModuleRuntime::destroyState(bool callShutdownFunction)
{
    closeOpenPanel();

    if (!m_state)
        return;

    if (callShutdownFunction && m_started && m_scriptError.empty())
        callOptionalNoArgs("OnShutdown");

    g_runtimeByState.erase(m_state);
    m_api.lua_close(m_state);
    m_state = nullptr;
}

void LuaModuleRuntime::registerBindings()
{
    // Base-library print remains runtime-owned; API domains register their own tables.
    m_api.lua_pushcclosure(m_state, &LuaModuleRuntime::luaPrint, 0);
    m_api.lua_setglobal(m_state, "print");

    registerUiBindings();
    registerEngineBindings();
    registerEnvironmentBindings();
    registerVegetationBindings();
    registerScriptBindings();
    registerSceneBindings();
    registerSaveBindings();
    registerAudioBindings();
    registerInputBindings();
    registerCameraBindings();
    registerPhysicsBindings();
    registerVehicleBindings();
    registerEntityBindings();
    registerPrefabBindings();
    registerModuleBindings();
}
void LuaModuleRuntime::sandboxStandardLibraries()
{
    // This is a practical mod sandbox foundation, not a hardened hostile-code
    // security boundary. Direct file/process/package/debug access is removed;
    // modules receive controlled engine services instead.
    const char* blockedGlobals[] = {
        "io", "os", "package", "debug",
        "dofile", "loadfile", "require"
    };

    for (const char* name : blockedGlobals)
        replaceGlobalWithNil(name);
}

void LuaModuleRuntime::registerFunction(
    const char* tableName,
    const char* functionName,
    LuaCFunction function)
{
    const std::string qualifiedName = std::string(tableName) + "." + functionName;
    if (std::find(
            m_registeredLuaFunctions.begin(),
            m_registeredLuaFunctions.end(),
            qualifiedName) == m_registeredLuaFunctions.end())
    {
        m_registeredLuaFunctions.push_back(qualifiedName);
    }
    else
    {
        std::cerr << "[LuaAPI] Duplicate binding registration: "
            << qualifiedName << '\n';
    }

    m_api.lua_getglobal(m_state, tableName);
    if (m_api.lua_type(m_state, -1) == kLuaTypeNil)
    {
        pop(1);
        m_api.lua_createtable(m_state, 0, 12);
    }

    m_api.lua_pushcclosure(m_state, function, 0);
    m_api.lua_setfield(m_state, -2, functionName);
    m_api.lua_setglobal(m_state, tableName);
}

void LuaModuleRuntime::writeLuaApiManifest() const
{
    if (!m_context)
        return;

    try
    {
        std::vector<std::string> names = m_registeredLuaFunctions;
        std::sort(names.begin(), names.end());

        const std::filesystem::path reportRoot =
            m_context->projectRoot() / "Build" / "Reports";
        std::filesystem::create_directories(reportRoot);

        const std::filesystem::path jsonPath =
            reportRoot / "LuaAPI_Runtime.json";
        const std::filesystem::path markdownPath =
            reportRoot / "LuaAPI_Runtime.md";

        std::ofstream json(jsonPath, std::ios::trunc);
        if (json)
        {
            json << "{\n";
            json << "  \"build_identity\": \""
                << jsonEscape(heritage::diagnostics::buildIdentity()) << "\",\n";
            json << "  \"module\": \""
                << jsonEscape(m_context->module().id) << "\",\n";
            json << "  \"binding_count\": " << names.size() << ",\n";
            json << "  \"bindings\": [\n";
            for (std::size_t index = 0; index < names.size(); ++index)
            {
                json << "    \"" << jsonEscape(names[index]) << "\"";
                if (index + 1 < names.size())
                    json << ',';
                json << '\n';
            }
            json << "  ]\n";
            json << "}\n";
        }

        std::ofstream markdown(markdownPath, std::ios::trunc);
        if (markdown)
        {
            markdown << "# Live Heritage Engine Lua API\n\n";
            markdown << "Build: `"
                << heritage::diagnostics::buildIdentity() << "`\n\n";
            markdown << "Module: `" << m_context->module().id << "`\n\n";
            markdown << "Registered functions: **" << names.size() << "**\n\n";
            markdown << "These names were captured from the running executable. "
                "Inspect `Build/Reports/LuaAPI.md` or the named C++ binding handler "
                "for arguments and return values; never guess a signature.\n\n";
            for (const std::string& name : names)
                markdown << "- `" << name << "`\n";
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "[LuaAPI] Could not write runtime manifest: "
            << exception.what() << '\n';
    }
    catch (...)
    {
        std::cerr << "[LuaAPI] Could not write runtime manifest: unknown error\n";
    }
}

bool LuaModuleRuntime::runSafetySmokeTests(
    std::string& summary,
    std::filesystem::path& reportPath)
{
    if (!m_context || !m_entities || !m_physics)
    {
        summary = "FAIL: Engine services required by the safety tests are unavailable.";
        reportPath.clear();
        m_lastSafetyReport = summary;
        m_lastSafetyReportPath.clear();
        return false;
    }

    auto& bodies = m_physics->rigidBodies();
    auto& collisions = m_physics->collisions();
    auto& constraints = m_physics->constraints();
    auto& vehicles = m_physics->vehicles();

    const std::size_t entityCountBefore = m_entities->count();
    const std::size_t bodyCountBefore = bodies.count();
    const std::size_t colliderCountBefore = collisions.count();
    const std::size_t constraintCountBefore = constraints.count();
    const std::size_t vehicleCountBefore = vehicles.count();

    heritage::entities::EntityHandle entity = heritage::entities::InvalidEntity;
    heritage::entities::EntityHandle replacementEntity = heritage::entities::InvalidEntity;
    heritage::physics::BodyHandle body = heritage::physics::InvalidBody;
    heritage::physics::ColliderHandle collider = heritage::physics::InvalidCollider;
    heritage::physics::ConstraintHandle constraint = heritage::physics::InvalidConstraint;
    heritage::vehicles::VehicleHandle vehicle = heritage::vehicles::InvalidVehicle;

    bool passed = true;
    std::size_t passedChecks = 0;
    std::size_t totalChecks = 0;
    std::vector<std::string> checks;

    auto record = [&](bool condition, const std::string& label)
    {
        ++totalChecks;
        if (condition)
            ++passedChecks;
        else
            passed = false;
        checks.push_back(std::string(condition ? "PASS: " : "FAIL: ") + label);
    };

    entity = m_entities->create("__heritage_safety_smoke_entity__");
    record(entity != heritage::entities::InvalidEntity, "created temporary entity");
    record(m_entities->exists(entity), "temporary entity handle resolves");

    heritage::physics::RigidBodyDescription bodyDescription;
    bodyDescription.entity = entity;
    bodyDescription.motionType = heritage::physics::BodyMotionType::Dynamic;
    bodyDescription.position = { 0.0f, 10000.0f, 0.0f };
    bodyDescription.mass = 25.0f;
    bodyDescription.gravityFactor = 0.0f;
    body = bodies.create(bodyDescription);
    record(body != heritage::physics::InvalidBody, "created temporary rigid body");
    record(bodies.exists(body), "temporary body handle resolves");

    if (bodies.exists(body))
    {
        collider = collisions.createSphere(
            body,
            0.25f,
            { 0.0f, 0.0f, 0.0f },
            0.7f,
            0.0f,
            false,
            bodies);
    }
    record(collider != heritage::physics::InvalidCollider, "created dependent collider");
    record(collisions.exists(collider), "dependent collider handle resolves");

    if (bodies.exists(body))
    {
        heritage::physics::SpringConstraintDescription spring;
        spring.bodyA = body;
        spring.anchorB = { 0.0f, 10001.0f, 0.0f };
        spring.restLength = 1.0f;
        spring.stiffness = 100.0f;
        spring.damping = 10.0f;
        constraint = constraints.createSpring(spring, bodies);
    }
    record(constraint != heritage::physics::InvalidConstraint, "created dependent constraint");
    record(constraints.exists(constraint), "dependent constraint handle resolves");

    if (bodies.exists(body))
    {
        heritage::vehicles::VehicleDescription vehicleDescription;
        vehicleDescription.chassisBody = body;
        vehicleDescription.highRateHertz = 1000.0f;
        vehicle = vehicles.create(vehicleDescription, bodies);
    }
    record(vehicle != heritage::vehicles::InvalidVehicle, "created dependent vehicle");
    record(vehicles.exists(vehicle), "dependent vehicle handle resolves");

    const bool bodyDestroyed = bodies.exists(body)
        && m_physics->destroyBody(body);
    record(bodyDestroyed, "PhysicsWorld destroyed the temporary body");
    record(!bodies.exists(body), "destroyed body handle is stale");
    record(!collisions.exists(collider), "body destruction invalidated its collider");
    record(!constraints.exists(constraint), "body destruction invalidated its constraint");
    record(!vehicles.exists(vehicle), "body destruction invalidated its vehicle");

    const bool entityDestroyed = m_entities->exists(entity)
        && m_entities->destroy(entity);
    record(entityDestroyed, "destroyed temporary entity");
    record(!m_entities->exists(entity), "destroyed entity handle is stale");

    replacementEntity = m_entities->create("__heritage_safety_smoke_replacement__");
    record(replacementEntity != heritage::entities::InvalidEntity, "created replacement entity");
    record(replacementEntity != entity, "slot reuse changed the entity generation");
    record(!m_entities->exists(entity), "old entity handle remains invalid after slot reuse");

    if (m_entities->exists(replacementEntity))
        m_entities->destroy(replacementEntity);

    // Defensive cleanup if a preceding creation or cascade check failed.
    if (vehicles.exists(vehicle))
        vehicles.destroy(vehicle);
    if (constraints.exists(constraint))
        constraints.destroy(constraint);
    if (collisions.exists(collider))
        collisions.destroy(collider);
    if (bodies.exists(body))
        m_physics->destroyBody(body);
    if (m_entities->exists(entity))
        m_entities->destroy(entity);
    if (m_entities->exists(replacementEntity))
        m_entities->destroy(replacementEntity);

    record(m_entities->count() == entityCountBefore, "entity count returned to baseline");
    record(bodies.count() == bodyCountBefore, "body count returned to baseline");
    record(collisions.count() == colliderCountBefore, "collider count returned to baseline");
    record(constraints.count() == constraintCountBefore, "constraint count returned to baseline");
    record(vehicles.count() == vehicleCountBefore, "vehicle count returned to baseline");

    try
    {
        const std::filesystem::path diagnosticRoot =
            m_context->projectRoot() / "UserData" / "Diagnostics";
        std::filesystem::create_directories(diagnosticRoot);
        reportPath = diagnosticRoot / "safety_smoke_last.txt";

        std::ofstream report(reportPath, std::ios::trunc);
        if (report)
        {
            report << "build_identity="
                << heritage::diagnostics::buildIdentity() << '\n';
            report << "module=" << m_context->module().id << '\n';
            report << "result=" << (passed ? "PASS" : "FAIL") << '\n';
            report << "checks_passed=" << passedChecks << '\n';
            report << "checks_total=" << totalChecks << '\n';
            report << "\n";
            for (const std::string& check : checks)
                report << check << '\n';
        }
    }
    catch (...)
    {
        reportPath.clear();
    }

    std::ostringstream result;
    result << (passed ? "PASS: " : "FAIL: ")
        << passedChecks << "/" << totalChecks
        << " engine lifetime safety checks passed.";
    if (!reportPath.empty())
        result << " Report: " << reportPath.string();

    summary = result.str();
    m_lastSafetyReport = summary;
    m_lastSafetyReportPath = reportPath;
    return passed;
}

void LuaModuleRuntime::replaceGlobalWithNil(const char* name)
{
    m_api.lua_pushnil(m_state);
    m_api.lua_setglobal(m_state, name);
}

bool LuaModuleRuntime::callOptionalNoArgs(const char* functionName)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }
    return protectedCall(0, 0, functionName);
}

bool LuaModuleRuntime::callOptionalNumber(
    const char* functionName,
    LuaNumber value)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushnumber(m_state, value);
    return protectedCall(1, 0, functionName);
}

bool LuaModuleRuntime::callOptionalTwoIntegers(
    const char* functionName,
    LuaInteger first,
    LuaInteger second)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushinteger(m_state, first);
    m_api.lua_pushinteger(m_state, second);
    return protectedCall(2, 0, functionName);
}

bool LuaModuleRuntime::callOptionalString(
    const char* functionName,
    const std::string& value)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushlstring(m_state, value.c_str(), value.size());
    return protectedCall(1, 0, functionName);
}

bool LuaModuleRuntime::callOptionalTwoStrings(
    const char* functionName,
    const std::string& first,
    const std::string& second)
{
    m_api.lua_getglobal(m_state, functionName);
    const int type = m_api.lua_type(m_state, -1);
    if (type == kLuaTypeNil)
    {
        pop(1);
        return true;
    }
    if (type != kLuaTypeFunction)
    {
        pop(1);
        setScriptError(std::string(functionName) + " exists but is not a function.");
        return false;
    }

    m_api.lua_pushlstring(m_state, first.c_str(), first.size());
    m_api.lua_pushlstring(m_state, second.c_str(), second.size());
    return protectedCall(2, 0, functionName);
}

bool LuaModuleRuntime::protectedCall(
    int argumentCount,
    int resultCount,
    const char* contextName)
{
    const int status = m_api.lua_pcallk(
        m_state,
        argumentCount,
        resultCount,
        0,
        0,
        nullptr);
    if (status == kLuaOk)
        return true;

    const std::string detail = stackString(-1);
    pop(1);
    setScriptError(
        "Lua error while " + std::string(contextName) + ":\n" + detail);
    return false;
}

void LuaModuleRuntime::setScriptError(const std::string& message)
{
    m_scriptError = message.empty()
        ? "Unknown Lua runtime error."
        : message;
}

void LuaModuleRuntime::clearScriptError()
{
    m_scriptError.clear();
}

std::string LuaModuleRuntime::stackString(int index) const
{
    if (!m_state || !m_api.lua_tolstring)
        return "<no Lua error text>";

    std::size_t length = 0;
    const char* text = m_api.lua_tolstring(m_state, index, &length);
    return text ? std::string(text, length) : "<non-string Lua error>";
}

void LuaModuleRuntime::pop(int count)
{
    if (m_state && count > 0)
        m_api.lua_settop(m_state, -count - 1);
}

void LuaModuleRuntime::drawRuntimeError(
    int framebufferWidth,
    int framebufferHeight)
{
    const float availableWidth = static_cast<float>((std::max)(framebufferWidth, 1));
    const float availableHeight = static_cast<float>((std::max)(framebufferHeight, 1));
    const float panelWidth = (std::max)(280.0f,
        (std::min)(720.0f, availableWidth - 32.0f));
    const float panelHeight = (std::max)(220.0f,
        (std::min)(460.0f, availableHeight - 48.0f));

    ImGui::SetNextWindowPos(
        ImVec2(availableWidth * 0.5f, availableHeight * 0.5f),
        ImGuiCond_Always,
        ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.015f, 0.018f, 0.97f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.0f, 20.0f));

    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##heritage_lua_error", nullptr, flags))
    {
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted("LUA MODULE ERROR");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::Separator();
        ImGui::Spacing();

        if (!m_scriptPath.empty())
            ImGui::TextDisabled("%s", m_scriptPath.string().c_str());

        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_scriptError.c_str());
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Fix and save any module Lua file to reload automatically, or press F5. "
            "If lua54.dll is missing, run Tools\\SetupLua.ps1 and rebuild.");
    }
    ImGui::End();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void LuaModuleRuntime::closeOpenPanel()
{
    while (!m_uiScopes.empty())
    {
        const UiScopeType scope = m_uiScopes.back();
        m_uiScopes.pop_back();
        if (scope == UiScopeType::TabItem)
            ImGui::EndTabItem();
        else if (scope == UiScopeType::HorizontalScroll)
            ImGui::EndChild();
        else
            ImGui::EndTabBar();
    }

    if (!m_panelOpen)
        return;

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    m_panelOpen = false;
    m_panelVisible = false;
    m_activePanelId.clear();
}

void LuaModuleRuntime::queueAction(
    ModuleRuntimeActionType type,
    const std::string& payload)
{
    ModuleRuntimeAction action;
    action.type = type;
    action.payload = payload;
    m_actions.push_back(std::move(action));
}

bool LuaModuleRuntime::checkForScriptChange(float deltaTime)
{
    // PERF05: automatic recursive filesystem polling is deliberately disabled
    // during runtime. Keep this API as a no-I/O compatibility hook; F5 performs
    // the explicit refresh/reload path.
    (void)deltaTime;
    return false;
}

std::unordered_map<std::string, std::filesystem::file_time_type>
LuaModuleRuntime::captureScriptWriteTimes() const
{
    std::unordered_map<std::string, std::filesystem::file_time_type> result;
    if (!m_context)
        return result;

    const std::filesystem::path root = m_context->scriptsRoot();
    std::error_code error;
    if (!std::filesystem::is_directory(root, error) || error)
        return result;

    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::skip_permission_denied,
        error);
    const std::filesystem::recursive_directory_iterator endIterator;

    while (!error && iterator != endIterator)
    {
        const std::filesystem::directory_entry& entry = *iterator;
        std::error_code entryError;
        if (entry.is_regular_file(entryError)
            && !entryError
            && entry.path().extension() == ".lua")
        {
            const auto writeTime = entry.last_write_time(entryError);
            if (!entryError)
            {
                result.emplace(
                    entry.path().lexically_normal().generic_string(),
                    writeTime);
            }
        }

        iterator.increment(error);
    }

    return result;
}

void LuaModuleRuntime::refreshScriptWatchSnapshot()
{
    m_scriptWriteTimes = captureScriptWriteTimes();
}

void LuaModuleRuntime::requestSceneLoad(
    const std::string& sceneId,
    bool forceReload)
{
    m_pendingSceneId = sceneId;
    m_pendingSceneReload = forceReload;
}

void LuaModuleRuntime::processPendingSceneTransition()
{
    if (!m_pendingSceneId || !m_context || !m_window)
        return;

    const std::string requested = *m_pendingSceneId;
    const bool forceReload = m_pendingSceneReload;
    m_pendingSceneId.reset();
    m_pendingSceneReload = false;

    const std::string previous = m_sceneManager.activeSceneId();
    if (!forceReload && !previous.empty() && previous == requested)
        return;

    if (!previous.empty() && m_state && m_scriptError.empty())
        callOptionalString("OnSceneExit", previous);

    std::string sceneError;
    const bool loaded = m_sceneManager.load(
        requested,
        m_window,
        *m_context,
        m_entities,
        sceneError);

    m_lastSceneError = sceneError;
    if (loaded)
    {
        if (m_state && m_scriptError.empty())
            callOptionalString("OnSceneEnter", requested);
    }
    else
    {
        std::cerr << "[Scene:" << m_context->module().id << "] "
            << sceneError << '\n';
        if (m_state && m_scriptError.empty())
            callOptionalTwoStrings("OnSceneError", requested, sceneError);
    }
}

LuaModuleRuntime* LuaModuleRuntime::runtimeFrom(lua_State* state)
{
    const auto found = g_runtimeByState.find(state);
    return found == g_runtimeByState.end() ? nullptr : found->second;
}

std::string LuaModuleRuntime::stringArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    const std::string& fallback)
{
    std::size_t length = 0;
    const char* value = runtime.m_api.lua_tolstring(state, index, &length);
    return value ? std::string(value, length) : fallback;
}

double LuaModuleRuntime::numberArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    double fallback)
{
    int isNumber = 0;
    const LuaNumber value = runtime.m_api.lua_tonumberx(state, index, &isNumber);
    return isNumber ? static_cast<double>(value) : fallback;
}

bool LuaModuleRuntime::booleanArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index,
    bool fallback)
{
    const int type = runtime.m_api.lua_type(state, index);
    return type == kLuaTypeBoolean
        ? runtime.m_api.lua_toboolean(state, index) != 0
        : fallback;
}

heritage::entities::EntityHandle LuaModuleRuntime::entityHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::entities::InvalidEntity;

    return static_cast<heritage::entities::EntityHandle>(value);
}

heritage::physics::BodyHandle LuaModuleRuntime::bodyHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidBody;

    return static_cast<heritage::physics::BodyHandle>(value);
}

heritage::physics::ColliderHandle LuaModuleRuntime::colliderHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidCollider;

    return static_cast<heritage::physics::ColliderHandle>(value);
}

heritage::physics::ConstraintHandle LuaModuleRuntime::constraintHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::physics::InvalidConstraint;

    return static_cast<heritage::physics::ConstraintHandle>(value);
}

heritage::vehicles::VehicleHandle LuaModuleRuntime::vehicleHandleArgument(
    LuaModuleRuntime& runtime,
    lua_State* state,
    int index)
{
    int converted = 0;
    const LuaInteger value = runtime.m_api.lua_tointegerx(
        state,
        index,
        &converted);
    if (!converted || value <= 0)
        return heritage::vehicles::InvalidVehicle;

    return static_cast<heritage::vehicles::VehicleHandle>(value);
}

int LuaModuleRuntime::luaPrint(lua_State* state)
{
    LuaModuleRuntime* runtime = runtimeFrom(state);
    if (!runtime)
        return 0;

    const int count = runtime->m_api.lua_gettop(state);
    std::ostringstream output;
    output << "[Lua:";
    output << (runtime->m_context ? runtime->m_context->module().id : "?");
    output << "] ";

    for (int index = 1; index <= count; ++index)
    {
        if (index > 1)
            output << '\t';

        const int type = runtime->m_api.lua_type(state, index);
        if (type == kLuaTypeBoolean)
            output << (runtime->m_api.lua_toboolean(state, index) ? "true" : "false");
        else if (type == kLuaTypeNil)
            output << "nil";
        else
            output << stringArgument(*runtime, state, index, "<value>");
    }

    std::cout << output.str() << '\n';
    return 0;
}

} // namespace heritage::modules
