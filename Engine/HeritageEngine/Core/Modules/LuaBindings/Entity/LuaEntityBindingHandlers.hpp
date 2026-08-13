#pragma once

struct lua_State;

namespace heritage::modules {

// CLEAN12: private Lua C-handler catalogue for the entity binding domain.
// Kept out of LuaModuleRuntime.hpp so ordinary runtime consumers do not
// parse hundreds of unrelated binding declarations.

struct LuaEntityBindingHandlers
{

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
    static int luaEntitySetMeshNodePrefixFilter(lua_State* state);
    static int luaEntitySetMeshColor(lua_State* state);
    static int luaEntitySetMeshNormalize(lua_State* state);
    static int luaEntitySetMeshDoubleSided(lua_State* state);
    static int luaEntityPlayMeshAnimation(lua_State* state);
    static int luaEntitySetMeshAnimationPlaying(lua_State* state);
    static int luaEntitySetMeshAnimationSpeed(lua_State* state);
    static int luaEntitySeekMeshAnimation(lua_State* state);
    static int luaEntityGetMeshAnimation(lua_State* state);
    static int luaEntitySetMeshNodeWorldPose(lua_State* state);
    static int luaEntitySetMeshNodeLocalRotationOffset(lua_State* state);
    static int luaEntitySetMeshNodeAnchoredWorldPose(lua_State* state);
    static int luaEntitySetMeshNodeAnchoredWorldDelta(lua_State* state);
    static int luaEntitySetMeshNodeTireFlexibleRingFromWheel(lua_State* state);
    static int luaEntityClearMeshNodeOverrides(lua_State* state);
    static int luaEntityGetMesh(lua_State* state);
    static int luaEntityGetLastError(lua_State* state);
    static int luaEntitySetPosition(lua_State* state);
    static int luaEntityGetPosition(lua_State* state);
    static int luaEntitySetRotation(lua_State* state);
    static int luaEntityGetRotation(lua_State* state);
    static int luaEntitySetScale(lua_State* state);
    static int luaEntityGetScale(lua_State* state);
    static int luaEntitySetWorldPosition(lua_State* state);
    static int luaEntityGetWorldPosition(lua_State* state);
    static int luaEntitySetWorldRotation(lua_State* state);
    static int luaEntityGetWorldRotation(lua_State* state);
    static int luaEntitySetWorldScale(lua_State* state);
    static int luaEntityGetWorldScale(lua_State* state);
};

} // namespace heritage::modules
