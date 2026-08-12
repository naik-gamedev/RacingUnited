#include "../../LuaModuleRuntime.hpp"
#include "LuaPhysicsBindingHandlers.hpp"
#include "../LuaBindingInternals.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "../../../../Physics/PhysicsWorld.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace heritage::modules {
using namespace lua_binding_detail;

int LuaPhysicsBindingHandlers::luaPhysicsCreateBody(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    if (!runtime->m_physics || !runtime->m_entities)
    {
        runtime->m_lastPhysicsError = "Physics or Entity service is unavailable.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    const heritage::entities::EntityHandle entity = LuaModuleRuntime::entityHandleArgument(
        *runtime, state, 1);
    if (!runtime->m_entities->exists(entity))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody requires a valid entity handle.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::BodyMotionType motionType;
    const std::string motionText = LuaModuleRuntime::stringArgument(
        *runtime, state, 2, "dynamic");
    if (!heritage::physics::parseBodyMotionType(motionText, motionType))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody motion type must be static, kinematic, or dynamic.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::math::Vec3 position{};
    heritage::math::Vec3 rotationDegrees{};
    if (!runtime->m_entities->worldPosition(entity, position)
        || !runtime->m_entities->worldRotationDegrees(entity, rotationDegrees))
    {
        runtime->m_lastPhysicsError =
            "Physics.CreateBody could not read the entity world transform.";
        runtime->m_api.lua_pushinteger(state, 0);
        return 1;
    }

    heritage::physics::RigidBodyDescription description;
    description.entity = entity;
    description.motionType = motionType;
    description.position = position;
    description.rotationDegrees = rotationDegrees;
    description.mass = static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0));

    const heritage::physics::BodyHandle handle =
        runtime->m_physics->rigidBodies().create(description);
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsDestroyBody(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->destroyBody(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsBodyExists(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().exists(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const std::size_t count = runtime->m_physics
        ? runtime->m_physics->rigidBodies().count()
        : 0;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(count));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetSleepingBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->rigidBodies().sleepingCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetActiveDynamicBodyCount(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    runtime->m_api.lua_pushinteger(
        state,
        runtime->m_physics
            ? static_cast<LuaInteger>(
                runtime->m_physics->rigidBodies().activeDynamicCount())
            : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsFindBodyByEntity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::physics::BodyHandle handle = runtime->m_physics
        ? runtime->m_physics->rigidBodies().bodyForEntity(
            LuaModuleRuntime::entityHandleArgument(*runtime, state, 1))
        : heritage::physics::InvalidBody;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(handle));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyEntity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::entities::EntityHandle entity = runtime->m_physics
        ? runtime->m_physics->rigidBodies().entityForBody(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1))
        : heritage::entities::InvalidEntity;
    runtime->m_api.lua_pushinteger(state, static_cast<LuaInteger>(entity));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyMotionType(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::BodyMotionType value;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().motionType(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    const char* name = heritage::physics::bodyMotionTypeName(value);
    runtime->m_api.lua_pushlstring(state, name, std::char_traits<char>::length(name));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyMotionType(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::BodyMotionType value;
    const std::string text = LuaModuleRuntime::stringArgument(*runtime, state, 2);
    if (!heritage::physics::parseBodyMotionType(text, value))
    {
        runtime->m_lastPhysicsError =
            "Physics.SetBodyMotionType requires static, kinematic, or dynamic.";
        runtime->m_api.lua_pushboolean(state, 0);
        return 1;
    }

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setMotionType(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyMass(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().mass(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyMass(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setMass(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}


int LuaPhysicsBindingHandlers::luaPhysicsGetBodyInertiaLocal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().inertiaLocal(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyInertiaLocal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 1.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 1.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setInertiaLocal(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsClearBodyInertiaLocalOverride(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().clearInertiaLocalOverride(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsIsBodyInertiaLocalOverridden(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool value = false;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().inertiaLocalOverridden(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }
    runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyCenterOfMassLocal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().centerOfMassLocal(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyCenterOfMassLocal(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setCenterOfMassLocal(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyCenterOfMassWorld(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().centerOfMassWorld(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyGravityFactor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().gravityFactor(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyGravityFactor(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setGravityFactor(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 1.0)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyLinearDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().linearDamping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyLinearDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setLinearDamping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.02)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyAngularDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    float value = 0.0f;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().angularDamping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        return 1;
    }

    runtime->m_api.lua_pushnumber(state, static_cast<LuaNumber>(value));
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyAngularDamping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAngularDamping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.05)));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyContinuousCollision(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().continuousCollision(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyContinuousCollision(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setContinuousCollision(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RigidBodyPose pose;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().pose(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), pose))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, pose.position.x);
    runtime->m_api.lua_pushnumber(state, pose.position.y);
    runtime->m_api.lua_pushnumber(state, pose.position.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyPosition(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setPosition(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::physics::RigidBodyPose pose;
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().pose(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), pose))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.x);
    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.y);
    runtime->m_api.lua_pushnumber(state, pose.rotationDegrees.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyRotation(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setRotationDegrees(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyLinearVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().linearVelocity(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyLinearVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setLinearVelocity(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyAngularVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    heritage::math::Vec3 value{};
    if (!runtime->m_physics
        || !runtime->m_physics->rigidBodies().angularVelocityDegrees(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value))
    {
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        runtime->m_api.lua_pushnil(state);
        return 3;
    }

    runtime->m_api.lua_pushnumber(state, value.x);
    runtime->m_api.lua_pushnumber(state, value.y);
    runtime->m_api.lua_pushnumber(state, value.z);
    return 3;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyAngularVelocity(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAngularVelocityDegrees(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsApplyBodyForce(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyForce(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsApplyBodyImpulse(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 value{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyLinearImpulse(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsApplyBodyImpulseAtPoint(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 impulse{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const heritage::math::Vec3 worldPoint{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 5, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 6, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 7, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyImpulseAtPoint(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            impulse,
            worldPoint);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsApplyBodyAngularImpulse(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const heritage::math::Vec3 angularImpulse{
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 2, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 3, 0.0)),
        static_cast<float>(LuaModuleRuntime::numberArgument(*runtime, state, 4, 0.0))
    };
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().applyAngularImpulse(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            angularImpulse);
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsClearBodyForces(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().clearForces(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsIsBodySleeping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().sleeping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodySleeping(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setSleeping(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, false));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsGetBodyAllowSleep(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    bool value = false;
    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().allowSleep(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1), value);
    if (!result)
        runtime->m_api.lua_pushnil(state);
    else
        runtime->m_api.lua_pushboolean(state, value ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsSetBodyAllowSleep(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().setAllowSleep(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1),
            LuaModuleRuntime::booleanArgument(*runtime, state, 2, true));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

int LuaPhysicsBindingHandlers::luaPhysicsWakeBody(lua_State* state)
{
    LuaModuleRuntime* runtime = LuaModuleRuntime::runtimeFrom(state);
    if (!runtime)
        return 0;

    runtime->m_lastPhysicsError.clear();

    const bool result = runtime->m_physics
        && runtime->m_physics->rigidBodies().wake(
            LuaModuleRuntime::bodyHandleArgument(*runtime, state, 1));
    runtime->m_api.lua_pushboolean(state, result ? 1 : 0);
    return 1;
}

} // namespace heritage::modules
