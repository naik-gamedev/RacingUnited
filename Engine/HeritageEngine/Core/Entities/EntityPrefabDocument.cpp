#include "EntityPrefabDocument.hpp"

#include <algorithm>
#include <cwctype>
#include <system_error>
#include <unordered_map>

#include "EntitySceneDocument.hpp"

namespace heritage::entities {
namespace {

constexpr std::size_t kMaximumNestedPrefabDepth = 32;
thread_local std::vector<std::filesystem::path> g_prefabLoadStack;

class PrefabStackGuard
{
public:
    explicit PrefabStackGuard(std::filesystem::path path)
        : m_path(std::move(path))
    {
        g_prefabLoadStack.push_back(m_path);
    }

    ~PrefabStackGuard()
    {
        if (!g_prefabLoadStack.empty())
            g_prefabLoadStack.pop_back();
    }

private:
    std::filesystem::path m_path;
};

std::filesystem::path normalizedPath(const std::filesystem::path& path)
{
    std::error_code error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (!error)
        return canonical.lexically_normal();
    return path.lexically_normal();
}

bool samePath(
    const std::filesystem::path& left,
    const std::filesystem::path& right)
{
#ifdef _WIN32
    std::wstring leftText = left.native();
    std::wstring rightText = right.native();
    const auto lowerWide = [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    };
    std::transform(leftText.begin(), leftText.end(), leftText.begin(), lowerWide);
    std::transform(rightText.begin(), rightText.end(), rightText.begin(), lowerWide);
    return leftText == rightText;
#else
    return left == right;
#endif
}

void rollback(
    EntityRegistry& registry,
    std::vector<EntityHandle>& entities)
{
    for (auto iterator = entities.rbegin(); iterator != entities.rend(); ++iterator)
    {
        if (registry.exists(*iterator))
            registry.destroy(*iterator);
    }
    entities.clear();
}

} // namespace

bool EntityPrefabDocument::instantiate(
    const std::filesystem::path& requestedPath,
    EntityRegistry& targetRegistry,
    const PrefabInstantiationOptions& options,
    PrefabInstantiationResult& result,
    std::string& errorMessage)
{
    result = {};

    if (requestedPath.empty())
    {
        errorMessage = "Prefab path cannot be empty.";
        return false;
    }

    const std::filesystem::path path = normalizedPath(requestedPath);
    if (path.extension() != ".hprefab")
    {
        errorMessage = "Prefab documents must use the .hprefab extension:\n"
            + path.string();
        return false;
    }

    if (g_prefabLoadStack.size() >= kMaximumNestedPrefabDepth)
    {
        errorMessage = "Nested prefab depth exceeded "
            + std::to_string(kMaximumNestedPrefabDepth)
            + " while loading:\n" + path.string();
        return false;
    }

    for (const std::filesystem::path& activePath : g_prefabLoadStack)
    {
        if (samePath(activePath, path))
        {
            errorMessage = "Circular prefab reference detected:\n" + path.string();
            return false;
        }
    }

    PrefabStackGuard stackGuard(path);

    EntityRegistry sourceRegistry;
    sourceRegistry.resetForModule(targetRegistry.moduleId() + ":prefab-staging");

    EntitySceneDocumentInfo documentInfo;
    std::vector<EntityHandle> sourceEntities;
    std::string loadError;
    if (!EntitySceneDocument::load(
            path,
            {},
            &sourceRegistry,
            documentInfo,
            sourceEntities,
            loadError))
    {
        errorMessage = "Could not load prefab document:\n" + path.string()
            + "\n\n" + loadError;
        return false;
    }

    if (sourceEntities.empty())
    {
        errorMessage = "Prefab '" + documentInfo.id
            + "' contains no entities:\n" + path.string();
        return false;
    }

    std::vector<EntityHandle> sourceRoots;
    for (EntityHandle handle : sourceEntities)
    {
        if (sourceRegistry.exists(handle)
            && sourceRegistry.parent(handle) == InvalidEntity)
        {
            sourceRoots.push_back(handle);
        }
    }

    if (sourceRoots.size() != 1)
    {
        errorMessage = "Prefab '" + documentInfo.id
            + "' must contain exactly one root entity, but found "
            + std::to_string(sourceRoots.size()) + ".\n" + path.string();
        return false;
    }

    std::unordered_map<EntityHandle, EntityHandle> cloneBySource;
    cloneBySource.reserve(sourceEntities.size());
    result.entities.reserve(sourceEntities.size());

    for (EntityHandle source : sourceEntities)
    {
        if (!sourceRegistry.exists(source))
            continue;

        std::string clonedName = options.namePrefix + sourceRegistry.name(source);
        const EntityHandle clone = targetRegistry.create(clonedName);
        if (clone == InvalidEntity)
        {
            rollback(targetRegistry, result.entities);
            errorMessage = "Could not create prefab entity '" + clonedName
                + "': " + targetRegistry.lastError();
            return false;
        }

        result.entities.push_back(clone);
        cloneBySource.emplace(source, clone);

        std::vector<std::string> tags;
        if (!sourceRegistry.tags(source, tags))
        {
            rollback(targetRegistry, result.entities);
            errorMessage = "Could not read prefab tags: "
                + sourceRegistry.lastError();
            return false;
        }
        for (const std::string& tag : tags)
        {
            if (!targetRegistry.addTag(clone, tag))
            {
                rollback(targetRegistry, result.entities);
                errorMessage = "Could not copy prefab tag '" + tag
                    + "': " + targetRegistry.lastError();
                return false;
            }
        }

        heritage::math::Vec3 position{};
        heritage::math::Vec3 rotation{};
        heritage::math::Vec3 scale{};
        if (!sourceRegistry.position(source, position)
            || !sourceRegistry.rotationDegrees(source, rotation)
            || !sourceRegistry.scale(source, scale)
            || !targetRegistry.setPosition(clone, position)
            || !targetRegistry.setRotationDegrees(clone, rotation)
            || !targetRegistry.setScale(clone, scale))
        {
            rollback(targetRegistry, result.entities);
            errorMessage = "Could not copy prefab transform: "
                + (targetRegistry.lastError().empty()
                    ? sourceRegistry.lastError()
                    : targetRegistry.lastError());
            return false;
        }

        MeshComponent meshComponent{};
        if (sourceRegistry.mesh(source, meshComponent))
        {
            if (!targetRegistry.setMesh(
                    clone,
                    meshComponent.assetPath,
                    meshComponent.color,
                    meshComponent.normalize,
                    meshComponent.doubleSided,
                    meshComponent.blenderCoordinates)
                || !targetRegistry.setMeshVisible(clone, meshComponent.visible))
            {
                rollback(targetRegistry, result.entities);
                errorMessage = "Could not copy prefab Mesh component: "
                    + targetRegistry.lastError();
                return false;
            }
        }

        DebugPrimitiveComponent debugComponent{};
        if (sourceRegistry.debugPrimitive(source, debugComponent))
        {
            if (!targetRegistry.setDebugPrimitive(
                    clone,
                    debugComponent.type,
                    debugComponent.color)
                || !targetRegistry.setDebugPrimitiveVisible(
                    clone,
                    debugComponent.visible))
            {
                rollback(targetRegistry, result.entities);
                errorMessage = "Could not copy prefab DebugPrimitive component: "
                    + targetRegistry.lastError();
                return false;
            }
        }
    }

    for (EntityHandle source : sourceEntities)
    {
        const EntityHandle sourceParent = sourceRegistry.parent(source);
        if (sourceParent == InvalidEntity)
            continue;

        const auto childClone = cloneBySource.find(source);
        const auto parentClone = cloneBySource.find(sourceParent);
        if (childClone == cloneBySource.end() || parentClone == cloneBySource.end())
        {
            rollback(targetRegistry, result.entities);
            errorMessage = "Prefab clone mapping was incomplete while restoring hierarchy.";
            return false;
        }

        if (!targetRegistry.setParent(
                childClone->second,
                parentClone->second,
                false))
        {
            rollback(targetRegistry, result.entities);
            errorMessage = "Could not restore prefab hierarchy: "
                + targetRegistry.lastError();
            return false;
        }
    }

    result.root = cloneBySource.at(sourceRoots.front());
    result.prefabId = documentInfo.id;

    if (!options.rootName.empty()
        && !targetRegistry.setName(result.root, options.rootName))
    {
        rollback(targetRegistry, result.entities);
        result.root = InvalidEntity;
        errorMessage = "Could not apply prefab root name: "
            + targetRegistry.lastError();
        return false;
    }

    if (options.overrideRootTransform)
    {
        if (!targetRegistry.setPosition(result.root, options.position)
            || !targetRegistry.setRotationDegrees(
                result.root,
                options.rotationDegrees)
            || !targetRegistry.setScale(result.root, options.scale))
        {
            rollback(targetRegistry, result.entities);
            result.root = InvalidEntity;
            errorMessage = "Could not apply prefab instance transform: "
                + targetRegistry.lastError();
            return false;
        }
    }

    errorMessage.clear();
    return true;
}

} // namespace heritage::entities
