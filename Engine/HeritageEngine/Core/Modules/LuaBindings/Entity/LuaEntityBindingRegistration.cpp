#include "../../LuaModuleRuntime.hpp"
#include "LuaEntityBindingHandlers.hpp"

namespace heritage::modules {

void LuaModuleRuntime::registerEntityBindings()
{
    registerFunction("Entity", "IsAvailable", &LuaEntityBindingHandlers::luaEntityIsAvailable);
    registerFunction("Entity", "Create", &LuaEntityBindingHandlers::luaEntityCreate);
    registerFunction("Entity", "Destroy", &LuaEntityBindingHandlers::luaEntityDestroy);
    registerFunction("Entity", "Exists", &LuaEntityBindingHandlers::luaEntityExists);
    registerFunction("Entity", "Count", &LuaEntityBindingHandlers::luaEntityCount);
    registerFunction("Entity", "GetPersistentId", &LuaEntityBindingHandlers::luaEntityGetPersistentId);
    registerFunction("Entity", "FindByName", &LuaEntityBindingHandlers::luaEntityFindByName);
    registerFunction("Entity", "SetName", &LuaEntityBindingHandlers::luaEntitySetName);
    registerFunction("Entity", "GetName", &LuaEntityBindingHandlers::luaEntityGetName);
    registerFunction("Entity", "AddTag", &LuaEntityBindingHandlers::luaEntityAddTag);
    registerFunction("Entity", "RemoveTag", &LuaEntityBindingHandlers::luaEntityRemoveTag);
    registerFunction("Entity", "HasTag", &LuaEntityBindingHandlers::luaEntityHasTag);
    registerFunction("Entity", "FindFirstWithTag", &LuaEntityBindingHandlers::luaEntityFindFirstWithTag);
    registerFunction("Entity", "SetParent", &LuaEntityBindingHandlers::luaEntitySetParent);
    registerFunction("Entity", "ClearParent", &LuaEntityBindingHandlers::luaEntityClearParent);
    registerFunction("Entity", "GetParent", &LuaEntityBindingHandlers::luaEntityGetParent);
    registerFunction("Entity", "GetChildCount", &LuaEntityBindingHandlers::luaEntityGetChildCount);
    registerFunction("Entity", "GetChildAt", &LuaEntityBindingHandlers::luaEntityGetChildAt);
    registerFunction("Entity", "IsDescendantOf", &LuaEntityBindingHandlers::luaEntityIsDescendantOf);
    registerFunction("Entity", "SetPosition", &LuaEntityBindingHandlers::luaEntitySetPosition);
    registerFunction("Entity", "GetPosition", &LuaEntityBindingHandlers::luaEntityGetPosition);
    registerFunction("Entity", "SetLocalPosition", &LuaEntityBindingHandlers::luaEntitySetPosition);
    registerFunction("Entity", "GetLocalPosition", &LuaEntityBindingHandlers::luaEntityGetPosition);
    registerFunction("Entity", "SetWorldPosition", &LuaEntityBindingHandlers::luaEntitySetWorldPosition);
    registerFunction("Entity", "GetWorldPosition", &LuaEntityBindingHandlers::luaEntityGetWorldPosition);
    registerFunction("Entity", "SetRotation", &LuaEntityBindingHandlers::luaEntitySetRotation);
    registerFunction("Entity", "GetRotation", &LuaEntityBindingHandlers::luaEntityGetRotation);
    registerFunction("Entity", "SetLocalRotation", &LuaEntityBindingHandlers::luaEntitySetRotation);
    registerFunction("Entity", "GetLocalRotation", &LuaEntityBindingHandlers::luaEntityGetRotation);
    registerFunction("Entity", "SetWorldRotation", &LuaEntityBindingHandlers::luaEntitySetWorldRotation);
    registerFunction("Entity", "GetWorldRotation", &LuaEntityBindingHandlers::luaEntityGetWorldRotation);
    registerFunction("Entity", "SetScale", &LuaEntityBindingHandlers::luaEntitySetScale);
    registerFunction("Entity", "GetScale", &LuaEntityBindingHandlers::luaEntityGetScale);
    registerFunction("Entity", "SetLocalScale", &LuaEntityBindingHandlers::luaEntitySetScale);
    registerFunction("Entity", "GetLocalScale", &LuaEntityBindingHandlers::luaEntityGetScale);
    registerFunction("Entity", "SetWorldScale", &LuaEntityBindingHandlers::luaEntitySetWorldScale);
    registerFunction("Entity", "GetWorldScale", &LuaEntityBindingHandlers::luaEntityGetWorldScale);
    registerFunction("Entity", "SetDebugPrimitive", &LuaEntityBindingHandlers::luaEntitySetDebugPrimitive);
    registerFunction("Entity", "RemoveDebugPrimitive", &LuaEntityBindingHandlers::luaEntityRemoveDebugPrimitive);
    registerFunction("Entity", "HasDebugPrimitive", &LuaEntityBindingHandlers::luaEntityHasDebugPrimitive);
    registerFunction("Entity", "SetDebugVisible", &LuaEntityBindingHandlers::luaEntitySetDebugVisible);
    registerFunction("Entity", "SetDebugColor", &LuaEntityBindingHandlers::luaEntitySetDebugColor);
    registerFunction("Entity", "GetDebugPrimitive", &LuaEntityBindingHandlers::luaEntityGetDebugPrimitive);
    registerFunction("Entity", "SetMesh", &LuaEntityBindingHandlers::luaEntitySetMesh);
    registerFunction("Entity", "RemoveMesh", &LuaEntityBindingHandlers::luaEntityRemoveMesh);
    registerFunction("Entity", "HasMesh", &LuaEntityBindingHandlers::luaEntityHasMesh);
    registerFunction("Entity", "SetMeshVisible", &LuaEntityBindingHandlers::luaEntitySetMeshVisible);
    registerFunction("Entity", "SetMeshNodePrefixFilter", &LuaEntityBindingHandlers::luaEntitySetMeshNodePrefixFilter);
    registerFunction("Entity", "SetMeshColor", &LuaEntityBindingHandlers::luaEntitySetMeshColor);
    registerFunction("Entity", "SetMeshNormalize", &LuaEntityBindingHandlers::luaEntitySetMeshNormalize);
    registerFunction("Entity", "SetMeshDoubleSided", &LuaEntityBindingHandlers::luaEntitySetMeshDoubleSided);
    registerFunction("Entity", "PlayMeshAnimation", &LuaEntityBindingHandlers::luaEntityPlayMeshAnimation);
    registerFunction("Entity", "SetMeshAnimationPlaying", &LuaEntityBindingHandlers::luaEntitySetMeshAnimationPlaying);
    registerFunction("Entity", "SetMeshAnimationSpeed", &LuaEntityBindingHandlers::luaEntitySetMeshAnimationSpeed);
    registerFunction("Entity", "SeekMeshAnimation", &LuaEntityBindingHandlers::luaEntitySeekMeshAnimation);
    registerFunction("Entity", "GetMeshAnimation", &LuaEntityBindingHandlers::luaEntityGetMeshAnimation);
    registerFunction("Entity", "SetMeshNodeWorldPose", &LuaEntityBindingHandlers::luaEntitySetMeshNodeWorldPose);
    registerFunction("Entity", "SetMeshNodeLocalRotationOffset", &LuaEntityBindingHandlers::luaEntitySetMeshNodeLocalRotationOffset);
    registerFunction("Entity", "SetMeshNodeAnchoredWorldPose", &LuaEntityBindingHandlers::luaEntitySetMeshNodeAnchoredWorldPose);
    registerFunction("Entity", "SetMeshNodeAnchoredWorldDelta", &LuaEntityBindingHandlers::luaEntitySetMeshNodeAnchoredWorldDelta);
    registerFunction("Entity", "SetMeshNodeTireFlexibleRingFromWheel", &LuaEntityBindingHandlers::luaEntitySetMeshNodeTireFlexibleRingFromWheel);
    registerFunction("Entity", "ClearMeshNodeOverrides", &LuaEntityBindingHandlers::luaEntityClearMeshNodeOverrides);
    registerFunction("Entity", "GetMesh", &LuaEntityBindingHandlers::luaEntityGetMesh);
    registerFunction("Entity", "GetLastError", &LuaEntityBindingHandlers::luaEntityGetLastError);
}

} // namespace heritage::modules
