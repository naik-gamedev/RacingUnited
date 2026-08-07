#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <glad/glad.h>

#include "../../Core/Entities/EntityRegistry.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../Mesh.hpp"

namespace heritage::graphics {

// Draws module-owned OBJ assets attached through Entity Mesh components.
// Geometry is cached by safe module-relative asset path and reloads when the
// source OBJ changes on disk. Textures/material files arrive in a later step.
class EntityMeshRenderer
{
public:
    bool initialize(const std::filesystem::path& moduleAssetRoot);
    void shutdown();
    void clearCache();

    void draw(
        const heritage::entities::EntityRegistry& registry,
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings,
        float elapsedSeconds);

    std::size_t loadedAssetCount() const;
    const std::string& lastError() const { return m_lastError; }

private:
    struct CachedAsset
    {
        Mesh mesh;
        std::filesystem::file_time_type lastWriteTime{};
        bool attempted = false;
        bool loaded = false;
        std::string error;
    };

    bool resolveAsset(
        const std::string& relativePath,
        std::filesystem::path& resolved,
        std::string& error) const;
    const Mesh* acquireMesh(
        const std::string& relativePath,
        bool normalize,
        bool blenderCoordinates);

    std::filesystem::path m_assetRoot;
    std::unordered_map<std::string, CachedAsset> m_cache;
    GLuint m_program = 0;
    std::string m_lastError;
};

} // namespace heritage::graphics
