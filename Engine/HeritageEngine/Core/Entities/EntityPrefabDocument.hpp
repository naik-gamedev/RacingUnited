#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "EntityRegistry.hpp"

namespace heritage::entities {

struct PrefabInstantiationOptions
{
    // Optional display-name override for the unique root entity.
    std::string rootName;

    // Optional prefix applied to every cloned entity name. The explicit
    // rootName override is applied after the prefix.
    std::string namePrefix;

    // Runtime spawn transform. Scene documents may disable this and apply
    // their own ordinary entity transform properties after instantiation.
    heritage::math::Vec3 position{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 rotationDegrees{ 0.0f, 0.0f, 0.0f };
    heritage::math::Vec3 scale{ 1.0f, 1.0f, 1.0f };
    bool overrideRootTransform = true;
};

struct PrefabInstantiationResult
{
    std::string prefabId;
    EntityHandle root = InvalidEntity;
    std::vector<EntityHandle> entities;
};

// Reusable entity-hierarchy document. A .hprefab deliberately reuses the
// proven [entity:key] document syntax, but it is loaded into a temporary
// registry and cloned with fresh persistent IDs for every instance.
class EntityPrefabDocument
{
public:
    static bool instantiate(
        const std::filesystem::path& path,
        EntityRegistry& targetRegistry,
        const PrefabInstantiationOptions& options,
        PrefabInstantiationResult& result,
        std::string& errorMessage);
};

} // namespace heritage::entities
