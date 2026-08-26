#include "EntityMeshRenderer.hpp"
#include "../../Core/Paths/Utf8Path.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <utility>

namespace heritage::graphics {
namespace {

std::string lowerAuthoringName(std::string text)
{
    std::transform(
        text.begin(), text.end(), text.begin(),
        [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return text;
}

const AssetMetadataValue* authoredMetadataValue(
    const AssetMetadataMap& metadata,
    const char* key)
{
    const auto found = metadata.find(key);
    return found != metadata.end() ? &found->second : nullptr;
}

bool authoredBool(
    const AssetMetadataMap& metadata,
    const char* key,
    bool fallback)
{
    const AssetMetadataValue* value = authoredMetadataValue(metadata, key);
    return value && value->type == AssetMetadataValueType::Boolean
        ? value->boolValue
        : fallback;
}

std::string authoredString(
    const AssetMetadataMap& metadata,
    const char* key)
{
    const AssetMetadataValue* value = authoredMetadataValue(metadata, key);
    return value && value->type == AssetMetadataValueType::String
        ? value->stringValue
        : std::string{};
}

bool hasHiddenAuthoringName(const std::string& name)
{
    const std::string lowered = lowerAuthoringName(name);
    const bool collision = lowered == "collision"
        || lowered.rfind("collision_", 0) == 0
        || lowered.find("_collision") != std::string::npos;
    const bool spawn = lowered.find("spawn_player") != std::string::npos
        || lowered.find("player_spawn") != std::string::npos
        || lowered.find("playerspawn") != std::string::npos;
    return collision || spawn;
}

bool shouldHideAuthoringNode(const Mesh& mesh, int nodeIndex)
{
    int current = nodeIndex;
    while (current >= 0 && static_cast<std::size_t>(current) < mesh.nodes.size())
    {
        const MeshNode& node = mesh.nodes[static_cast<std::size_t>(current)];
        const std::string role = lowerAuthoringName(
            authoredString(node.metadata, "heritage.role"));
        const std::string collisionType = lowerAuthoringName(
            authoredString(node.metadata, "heritage.collision_type"));
        if (!authoredBool(node.metadata, "heritage.render", true)
            || role == "collision_mesh"
            || role == "collision"
            || role == "spawn_player"
            || role == "player_spawn"
            || collisionType == "static_triangle_mesh"
            || authoredBool(node.metadata, "heritage.collision", false)
            || hasHiddenAuthoringName(node.name))
        {
            return true;
        }
        current = node.parentIndex;
    }
    return false;
}

bool pathBeginsWith(
    const std::filesystem::path& candidate,
    const std::filesystem::path& root)
{
    auto candidatePart = candidate.begin();
    for (auto rootPart = root.begin();
         rootPart != root.end();
         ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end()
            || *candidatePart != *rootPart)
        {
            return false;
        }
    }
    return true;
}


} // namespace

void EntityMeshRenderer::clearCache()
{
    for (auto& [key, asset] : m_cache)
    {
        (void)key;
        destroyMesh(asset.mesh);
    }
    m_cache.clear();
    m_resolvedTexturePaths.clear();
    m_animationStates.clear();
    m_textureCache.clear();
    m_reportedMaterialWarnings.clear();
    m_reportedAnimationWarnings.clear();
}
bool EntityMeshRenderer::resolveAsset(
    const std::string& relativePath,
    std::filesystem::path& resolved,
    std::string& error) const
{
    const std::filesystem::path requested = heritage::paths::fromUtf8(relativePath);
    if (relativePath.empty()
        || requested.is_absolute()
        || requested.has_root_name())
    {
        error =
            "Mesh path must be relative to the active module Assets directory.";
        return false;
    }

    std::error_code canonicalError;
    resolved = std::filesystem::weakly_canonical(
        m_assetRoot / requested,
        canonicalError);
    if (canonicalError)
    {
        resolved =
            std::filesystem::absolute(m_assetRoot / requested)
                .lexically_normal();
    }

    if (!pathBeginsWith(resolved, m_assetRoot))
    {
        error =
            "Mesh path escaped the active module Assets directory.";
        return false;
    }

    if (!std::filesystem::is_regular_file(resolved))
    {
        error = "Mesh asset was not found: " + heritage::paths::toUtf8(resolved);
        return false;
    }

    error.clear();
    return true;
}
bool EntityMeshRenderer::resolveMaterialTexture(
    const std::filesystem::path& requested,
    std::filesystem::path& resolved,
    std::string& error)
{
    if (requested.empty())
        return false;

    const std::string cacheKey = heritage::paths::toUtf8(requested.lexically_normal());
    ResolvedTexturePath& cached = m_resolvedTexturePaths[cacheKey];
    if (cached.attempted
        && cached.lastHotReloadEpoch == m_hotReloadEpoch)
    {
        resolved = cached.resolved;
        error = cached.error;
        return cached.valid;
    }

    cached.attempted = true;
    cached.lastHotReloadEpoch = m_hotReloadEpoch;
    cached.valid = false;
    cached.error.clear();

    std::error_code canonicalError;
    cached.resolved =
        std::filesystem::weakly_canonical(requested, canonicalError);
    if (canonicalError)
    {
        cached.resolved =
            std::filesystem::absolute(requested).lexically_normal();
    }

    if (!pathBeginsWith(cached.resolved, m_assetRoot))
    {
        cached.error =
            "Material texture path escaped the active module Assets directory: "
            + heritage::paths::toUtf8(requested);
    }
    else
    {
        std::error_code fileError;
        if (!std::filesystem::is_regular_file(cached.resolved, fileError))
        {
            cached.error =
                "Material texture was not found: "
                + heritage::paths::toUtf8(cached.resolved);
        }
        else
        {
            cached.valid = true;
        }
    }

    resolved = cached.resolved;
    error = cached.error;
    return cached.valid;
}
bool EntityMeshRenderer::dependenciesChanged(
    const CachedAsset& asset) const
{
    for (const DependencyStamp& dependency : asset.dependencies)
    {
        std::error_code fileError;
        const bool exists =
            std::filesystem::is_regular_file(
                dependency.path, fileError);
        if (exists != dependency.exists)
            return true;

        if (!exists)
            continue;

        std::error_code timeError;
        const auto current =
            std::filesystem::last_write_time(
                dependency.path, timeError);
        if (!timeError && current != dependency.lastWriteTime)
            return true;
    }
    return false;
}
void EntityMeshRenderer::rememberDependencies(CachedAsset& asset)
{
    asset.dependencies.clear();
    asset.dependencies.reserve(
        asset.mesh.sourceDependencies.size());

    for (const auto& path : asset.mesh.sourceDependencies)
    {
        DependencyStamp stamp;
        stamp.path = path;

        std::error_code fileError;
        stamp.exists =
            std::filesystem::is_regular_file(path, fileError);
        if (stamp.exists)
        {
            std::error_code timeError;
            stamp.lastWriteTime =
                std::filesystem::last_write_time(path, timeError);
            if (timeError)
                stamp.lastWriteTime = {};
        }
        asset.dependencies.push_back(std::move(stamp));
    }
}
void EntityMeshRenderer::reportMaterialWarning(
    const std::string& warning)
{
    if (warning.empty())
        return;

    if (m_reportedMaterialWarnings.insert(warning).second)
        std::cerr << "Material texture warning: " << warning << '\n';
}
const Mesh* EntityMeshRenderer::acquireMesh(
    const std::string& relativePath,
    bool normalizeMesh,
    bool blenderCoordinates)
{
    std::filesystem::path resolved;
    std::string resolveError;
    const std::string cacheKey =
        relativePath
        + (normalizeMesh ? "|normalized" : "|authored")
        + (blenderCoordinates ? "|blender" : "|engine");
    CachedAsset& asset = m_cache[cacheKey];

    // Mesh/dependency timestamps are a hot-reload feature, not render work.
    // Poll them once per reload epoch instead of every time an instance is
    // submitted. This also prevents duplicate checks when several entities
    // share the same GLB.
    if (asset.attempted && asset.lastHotReloadEpoch == m_hotReloadEpoch)
        return asset.loaded ? &asset.mesh : nullptr;

    asset.lastHotReloadEpoch = m_hotReloadEpoch;
    if (!resolveAsset(relativePath, resolved, resolveError))
    {
        if (!asset.attempted || asset.error != resolveError)
            std::cerr << "Entity mesh warning: " << resolveError << '\n';
        asset.attempted = true;
        asset.loaded = false;
        asset.error = resolveError;
        m_lastError = resolveError;
        return nullptr;
    }

    std::error_code timeError;
    const auto writeTime =
        std::filesystem::last_write_time(resolved, timeError);
    const bool changed =
        !asset.attempted
        || (!timeError && writeTime != asset.lastWriteTime)
        || dependenciesChanged(asset);

    if (changed)
    {
        // A GLB reload may replace embedded image byte vectors while retaining
        // the same authored image keys. Clear texture/path caches only on the
        // rare asset-reload path so normal frames never re-hash those images.
        m_textureCache.clear();
        m_textureCache.setHotReloadEpoch(m_hotReloadEpoch);
        m_resolvedTexturePaths.clear();

        destroyMesh(asset.mesh);
        const std::string extension = resolved.extension().string();
        if (extension == ".glb" || extension == ".GLB")
        {
            if (blenderCoordinates)
            {
                std::cout
                    << "Entity mesh note: blenderCoordinates is ignored for GLB asset "
                    << heritage::paths::toUtf8(resolved) << '\n';
            }
            asset.mesh = loadGlbMesh(
                resolved,
                normalizeMesh);
        }
        else
        {
            asset.mesh = loadObjMesh(
                heritage::paths::toUtf8(resolved),
                normalizeMesh,
                blenderCoordinates);
        }
        asset.attempted = true;
        asset.lastWriteTime = writeTime;
        asset.loaded = !asset.mesh.indices.empty();
        rememberDependencies(asset);

        if (asset.loaded)
        {
            // PERF03: authoring-only collision/spawn nodes are a static asset
            // property. Resolve once here instead of walking parent metadata
            // and lower-casing names for every primitive every frame.
            for (MeshDrawRange& range : asset.mesh.drawRanges)
                range.hiddenByAuthoring = shouldHideAuthoringNode(asset.mesh, range.nodeIndex);

            uploadMesh(asset.mesh);
            asset.loaded = asset.mesh.vao != 0;
        }

        if (!asset.loaded)
        {
            asset.error =
                "Mesh asset contained no renderable triangles: "
                + heritage::paths::toUtf8(resolved);
            std::cerr
                << "Entity mesh warning: "
                << asset.error << '\n';
            m_lastError = asset.error;
        }
        else
        {
            asset.error.clear();
            m_lastError.clear();
        }
    }

    return asset.loaded ? &asset.mesh : nullptr;
}

void EntityMeshRenderer::requestHotReloadPoll()
{
    ++m_hotReloadEpoch;
    if (m_hotReloadEpoch == 0)
        m_hotReloadEpoch = 1;
    m_textureCache.setHotReloadEpoch(m_hotReloadEpoch);
}

std::size_t EntityMeshRenderer::loadedAssetCount() const
{
    return static_cast<std::size_t>(
        std::count_if(
            m_cache.begin(),
            m_cache.end(),
            [](const auto& item)
            {
                return item.second.loaded;
            }));
}

} // namespace heritage::graphics
