#include "../../LuaModuleRuntime.hpp"
#include "LuaPhysicsBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"
#include "../../../Paths/Utf8Path.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cstdint>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "../../../../Physics/StaticBoxSceneImporter.hpp"
#include "../../../../Physics/StaticTriangleSceneImporter.hpp"

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaPhysicsBindingHandlers::luaPhysicsCreateSphereCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::physics::ColliderHandle handle =
        runtime->m_physics->collisions().createSphere(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.5)),
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.75)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.15)),
            LuaModuleRuntime::booleanArgument(*runtime, state, 8, false),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsCreateBoxCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics)
    {
        runtime->m_lastPhysicsError = "PhysicsWorld is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::physics::ColliderHandle handle =
        runtime->m_physics->collisions().createBox(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.5)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.5)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.5))
            },
            {
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
                static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0))
            },
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 8, 0.75)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 9, 0.15)),
            LuaModuleRuntime::booleanArgument(*runtime, state, 10, false),
            runtime->m_physics->rigidBodies());
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsLoadStaticBoxScene(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_entities || !runtime->m_context)
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene requires Physics, Entity, and Module services.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const bool blenderCoordinates = LuaModuleRuntime::booleanArgument(*runtime, state, 2, true);
    const std::filesystem::path resolved =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    if (resolved.empty())
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene requires a safe module-asset-relative path.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::string extension = resolved.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".obj")
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticBoxScene currently supports OBJ proxy documents only.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::vector<heritage::physics::StaticBoxSceneDescriptor> descriptors;
    heritage::physics::StaticBoxSceneSpawn importedSpawn;
    std::string importError;
    if (!heritage::physics::loadStaticBoxSceneFromObj(
            resolved,
            blenderCoordinates,
            descriptors,
            &importedSpawn,
            importError))
    {
        runtime->m_lastPhysicsError = importError;
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    runtime->clearImportedStaticBoxScene();

    for (const auto& descriptor : descriptors)
    {
        const heritage::entities::EntityHandle entity =
            runtime->m_entities->create("Player Scene Collision: " + descriptor.name);
        if (entity == heritage::entities::InvalidEntity
            || !runtime->m_entities->setPosition(entity, descriptor.center))
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create a collision-proxy entity.";
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        heritage::physics::RigidBodyDescription bodyDescription;
        bodyDescription.entity = entity;
        bodyDescription.motionType = heritage::physics::BodyMotionType::Static;
        bodyDescription.position = descriptor.center;
        bodyDescription.mass = 1.0f;
        const heritage::physics::BodyHandle body =
            runtime->m_physics->rigidBodies().create(bodyDescription);
        if (body == heritage::physics::InvalidBody)
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create a static body: "
                + runtime->m_physics->rigidBodies().lastError();
            runtime->m_entities->destroy(entity);
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        const heritage::physics::ColliderHandle collider =
            runtime->m_physics->collisions().createBox(
                body,
                descriptor.halfExtents,
                { 0.0f, 0.0f, 0.0f },
                descriptor.friction,
                descriptor.restitution,
                false,
                runtime->m_physics->rigidBodies());
        if (collider == heritage::physics::InvalidCollider
            || !runtime->m_physics->collisions().setSurface(
                collider,
                descriptor.surfaceMaterial,
                descriptor.surfaceWetness))
        {
            runtime->m_lastPhysicsError =
                "Physics.LoadStaticBoxScene could not create/configure collider '"
                + descriptor.name + "': "
                + runtime->m_physics->collisions().lastError();
            runtime->m_physics->destroyBody(body);
            runtime->m_entities->destroy(entity);
            runtime->clearImportedStaticBoxScene();
            runtime->m_api.lua_pushinteger(state, -1);
            return 1;
        }

        runtime->m_importedStaticSceneEntities.push_back(entity);
        runtime->m_importedStaticSceneBodies.push_back(body);
    }

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_importedStaticSceneBodies.size()));
    if (importedSpawn.found)
    {
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.x);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.y);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.z);
        runtime->m_api.lua_pushstring(
            state,
            importedSpawn.explicitMarker ? "marker" : "auto-ground");
    }
    else
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "origin-fallback");
    }
    return 5;
}

int LuaPhysicsBindingHandlers::luaPhysicsLoadStaticTriangleScene(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_context)
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene requires Physics and Module services.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    const std::string relativePath = LuaModuleRuntime::stringArgument(*runtime, state, 1);
    const std::string spawnRelativePath = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    const bool blenderDefaultObjCoordinates = LuaModuleRuntime::booleanArgument(*runtime, state, 3, true);
    const std::filesystem::path resolved =
        runtime->m_context->resolveAssetPath(heritage::paths::fromUtf8(relativePath));
    if (resolved.empty())
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene requires a safe module-asset-relative collision path.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::string extension = resolved.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".obj" && extension != ".glb")
    {
        runtime->m_lastPhysicsError =
            "Physics.LoadStaticTriangleScene supports .obj and .glb scene assets.";
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    std::vector<heritage::physics::StaticSceneTriangle> triangles;
    heritage::physics::StaticTriangleSceneSpawn importedSpawn;
    std::string importError;
    bool imported = false;
    if (extension == ".glb")
    {
        // GLB keeps visible geometry, collision authoring and SPAWN_PLAYER in
        // one container. Blender/glTF coordinates are already converted by
        // the exporter into Heritage's X/right, Y/up, Z/forward convention.
        imported = heritage::physics::loadStaticTriangleSceneFromGlb(
            resolved,
            triangles,
            &importedSpawn,
            importError);
    }
    else
    {
        imported = heritage::physics::loadStaticTriangleSceneFromObj(
            resolved,
            blenderDefaultObjCoordinates,
            triangles,
            &importedSpawn,
            importError);

        // Legacy OBJ compatibility: SPAWN_PLAYER may live in a second visual
        // OBJ. New GLB scenes intentionally keep it in the same container.
        if (imported && !spawnRelativePath.empty())
        {
            const std::filesystem::path spawnResolved =
                runtime->m_context->resolveAssetPath(
                    heritage::paths::fromUtf8(spawnRelativePath));
            if (!spawnResolved.empty())
            {
                heritage::physics::StaticTriangleSceneSpawn visualSpawn;
                std::string spawnError;
                if (heritage::physics::loadStaticTriangleSceneSpawnFromObj(
                        spawnResolved,
                        blenderDefaultObjCoordinates,
                        visualSpawn,
                        spawnError)
                    && visualSpawn.found)
                {
                    importedSpawn = visualSpawn;
                    heritage::physics::snapStaticTriangleSceneSpawnToSurface(
                        triangles, importedSpawn);
                }
            }
        }
    }

    if (!imported)
    {
        runtime->m_lastPhysicsError = importError;
        runtime->m_api.lua_pushinteger(state, -1);
        return 1;
    }

    // Imported scene coordinates are absolute/authored world coordinates.
    // Convert once at the import boundary into the current compact local frame.
    // The FP64 origin remains authoritative even though triangle storage stays
    // FP32 for fast BVH/contact work.
    const heritage::math::DVec3 worldOrigin = runtime->m_physics->globalOrigin();
    const auto toCurrentLocal = [&](heritage::math::Vec3& point) {
        point.x = static_cast<float>(
            static_cast<double>(point.x) - worldOrigin.x);
        point.y = static_cast<float>(
            static_cast<double>(point.y) - worldOrigin.y);
        point.z = static_cast<float>(
            static_cast<double>(point.z) - worldOrigin.z);
    };
    for (heritage::physics::StaticSceneTriangle& triangle : triangles)
    {
        toCurrentLocal(triangle.a);
        toCurrentLocal(triangle.b);
        toCurrentLocal(triangle.c);
    }
    if (importedSpawn.found)
        toCurrentLocal(importedSpawn.groundPoint);

    // DSURF01: the same authoritative static collision mesh is baked once into
    // persistent 100m Dynamic Surface chunks/sheets before any dynamic state
    // migrates into them. The cache stores only immutable surface geometry and
    // metadata; water/rubber/dirt state never belongs to this file.
    const std::size_t surfacePathHash = std::hash<std::string>{}(
        resolved.lexically_normal().generic_string());
    const std::filesystem::path dynamicSurfaceCache =
        runtime->m_context->settingsRoot()
        / "DynamicSurface"
        / (resolved.stem().string() + "_"
            + std::to_string(surfacePathHash) + ".hdsurf");
    heritage::physics::dynamicsurface::DynamicSurfaceStaticBakeReport
        dynamicSurfaceReport;
    runtime->m_physics->surfaces().loadOrBakeDynamicSurface(
        triangles, dynamicSurfaceCache, dynamicSurfaceReport);

    // LIVETRACK08 immutable scene hydrology bake. Dynamic Surface still owns
    // runtime water/dry-line state, but .hhyd now also caches the expensive
    // static drainage answer: broad basin spill elevation + downhill direction.
    // The production GPU puddle response samples this topology instead of
    // probing collision triangles or solving neighbour flow while driving.
    const std::filesystem::path hydrologyCache =
        runtime->m_context->settingsRoot()
        / "Hydrology"
        / (resolved.stem().string() + "_"
            + std::to_string(surfacePathHash) + ".hhyd");
    heritage::physics::water::SurfaceHydrologyBakeReport hydrologyReport;
    runtime->m_physics->surfaces().loadOrBakeHydrology(
        triangles, hydrologyCache, hydrologyReport);

    runtime->m_physics->collisions().setStaticSceneTriangles(std::move(triangles));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(
            runtime->m_physics->collisions().staticSceneTriangleCount()));
    if (importedSpawn.found)
    {
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.x);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.y);
        runtime->m_api.lua_pushnumber(state, importedSpawn.groundPoint.z);
        runtime->m_api.lua_pushstring(
            state,
            importedSpawn.explicitMarker ? "marker" : importedSpawn.sourceName.c_str());
    }
    else
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushstring(state, "origin-fallback");
    }
    return 5;
}

int LuaPhysicsBindingHandlers::luaPhysicsUnloadStaticTriangleScene(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    if (runtime->m_physics)
    {
        runtime->m_physics->collisions().clearStaticSceneTriangles();
        runtime->m_physics->surfaces().clearHydrology();
    }
    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetStaticTriangleSceneCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    const std::size_t count = runtime->m_physics
        ? runtime->m_physics->collisions().staticSceneTriangleCount()
        : 0u;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsUnloadStaticBoxScene(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->clearImportedStaticBoxScene();
    runtime->m_lastPhysicsError.clear();
    runtime->m_api.lua_pushboolean(state, 1);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetStaticBoxSceneCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(runtime->m_importedStaticSceneBodies.size()));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsDestroyCollider(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().destroy(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsColliderExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().exists(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetColliderCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().count())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyColliderCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(runtime->m_physics->collisions().countForBody(
                LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1)))
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyCollisionBounds(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::BodyCollisionBounds bounds;
    if (!runtime->m_physics
        || !runtime->m_physics->collisions().bodyCollisionBounds(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), bounds))
    {
        if (runtime->m_physics)
            runtime->m_lastPhysicsError = runtime->m_physics->collisions().lastError();
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_createtable(state, 0, 22);
    const auto pushNumberField = [&](const char* name, LuaNumber value) {
        runtime->m_api.lua_pushnumber(state, value);
        runtime->m_api.lua_setfield(state, -2, name);
    };
    const auto pushIntegerField = [&](const char* name, LuaInteger value) {
        runtime->m_api.lua_pushinteger(state, value);
        runtime->m_api.lua_setfield(state, -2, name);
    };

    pushNumberField("minX", bounds.minimum.x);
    pushNumberField("minY", bounds.minimum.y);
    pushNumberField("minZ", bounds.minimum.z);
    pushNumberField("maxX", bounds.maximum.x);
    pushNumberField("maxY", bounds.maximum.y);
    pushNumberField("maxZ", bounds.maximum.z);
    pushNumberField("centerX", bounds.center.x);
    pushNumberField("centerY", bounds.center.y);
    pushNumberField("centerZ", bounds.center.z);
    pushNumberField("sizeX", bounds.size.x);
    pushNumberField("sizeY", bounds.size.y);
    pushNumberField("sizeZ", bounds.size.z);
    pushNumberField("halfExtentX", bounds.size.x * 0.5f);
    pushNumberField("halfExtentY", bounds.size.y * 0.5f);
    pushNumberField("halfExtentZ", bounds.size.z * 0.5f);
    pushNumberField("width", bounds.size.x);
    pushNumberField("height", bounds.size.y);
    pushNumberField("length", bounds.size.z);
    pushIntegerField("colliderCount", static_cast<LuaInteger>(bounds.colliderCount));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetColliderBody(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle handle = runtime->m_physics
        ? runtime->m_physics->collisions().body(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetColliderShape(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    heritage::physics::ColliderShapeType value;
    if (!runtime->m_physics
        || !runtime->m_physics->collisions().shapeType(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    const char* name = heritage::physics::colliderShapeTypeName(value);
    runtime->m_api.lua_pushstring(state, name);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetColliderMaterial(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setMaterial(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.75)),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.15)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetColliderSurface(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SurfaceMaterial material;
    const std::string materialText = LuaModuleRuntime::stringArgument(
        *runtime, state, 2, "default");
    if (!heritage::physics::parseSurfaceMaterial(materialText, material))
    {
        runtime->m_lastPhysicsError =
            "Physics.SetColliderSurface material must be default, asphalt, "
            "gravel, dirt, grass, snow, ice, kerb/curb, or painted_line.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setSurface(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1),
            material,
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)));
    if (!result && runtime->m_physics && runtime->m_lastPhysicsError.empty())
        runtime->m_lastPhysicsError = runtime->m_physics->collisions().lastError();
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetColliderSurface(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::SurfaceMaterial material =
        heritage::physics::SurfaceMaterial::Default;
    float wetness = 0.0f;
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().surface(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1),
            material,
            wetness);
    if (!result)
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushstring(
        state,
        heritage::physics::surfaceMaterialName(material));
    runtime->m_api.lua_pushinteger(
        state,
        static_cast<LuaInteger>(material));
    runtime->m_api.lua_pushnumber(
        state,
        static_cast<LuaNumber>(wetness));
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetColliderTrigger(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setTrigger(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetColliderFilter(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;
    runtime->m_lastPhysicsError.clear();
    const std::uint32_t layer = static_cast<std::uint32_t>(
        LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0));
    const std::uint32_t mask = static_cast<std::uint32_t>(
        LuaModuleRuntime::numberArgument(*runtime, state, 3, 4294967295.0));
    const bool result = runtime->m_physics
        && runtime->m_physics->collisions().setFilter(
            LuaModuleRuntime::colliderHandleArgument(*runtime, state, 1), layer, mask);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

} // namespace heritage::modules
