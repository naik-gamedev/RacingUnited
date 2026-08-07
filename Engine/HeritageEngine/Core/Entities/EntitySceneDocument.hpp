#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "EntityRegistry.hpp"

namespace heritage::entities {

struct EntitySceneDocumentInfo
{
    std::string id;
    std::string type = "empty";
    heritage::math::Vec3 clearColor{ 0.003f, 0.005f, 0.008f };
    bool showOverlay = false;
    std::string title;
    std::string subtitle;
    std::string text;
    std::size_t entityCount = 0;
};

// Reads and writes Heritage Engine .hscene entity documents. The file format
// uses scene metadata followed by [entity:<scene-local-key>] sections. Parent
// links refer to those stable keys rather than runtime handles.
class EntitySceneDocument
{
public:
    static bool load(
        const std::filesystem::path& path,
        const std::string& expectedSceneId,
        EntityRegistry* registry,
        EntitySceneDocumentInfo& info,
        std::vector<EntityHandle>& createdEntities,
        std::string& errorMessage);

    // Native round-trip foundation for the future scene editor. Only handles
    // supplied by the caller are serialized; parents outside that set are
    // intentionally omitted.
    static bool save(
        const std::filesystem::path& path,
        const EntitySceneDocumentInfo& info,
        const EntityRegistry& registry,
        const std::vector<EntityHandle>& entities,
        std::string& errorMessage);
};

} // namespace heritage::entities
