#pragma once

#include <cstdint>

#include <glad/glad.h>

#include "../../Camera/ChaseCamera.hpp"
#include "../../Core/Entities/EntityRegistry.hpp"
#include "../../Core/Math/Math.hpp"
#include "../../Core/Settings/VideoSettings.hpp"
#include "../Mesh.hpp"

namespace heritage::graphics {

struct EntityDebugRendererStats
{
    std::uint64_t drawCalls = 0;
    std::uint64_t triangles = 0;
    std::uint64_t instances = 0;
};

// Renders the first optional entity component: DebugPrimitive.
//
// This is deliberately a development renderer, not the final mesh/material
// component. It gives modules visible boxes, cylinders and spheres while the
// entity/component architecture is still being established.
class EntityDebugRenderer
{
public:
    bool initialize();
    void shutdown();

    void beginFrameStats() { m_frameStats = {}; }
    const EntityDebugRendererStats& frameStats() const { return m_frameStats; }

    void draw(
        const heritage::entities::EntityRegistry& registry,
        const heritage::math::Mat4& projection,
        const heritage::settings::VideoSettings& videoSettings,
        float elapsedSeconds,
        const heritage::camera::CameraFrame& cameraFrame) const;

private:
    Mesh m_box;
    Mesh m_cylinder;
    Mesh m_sphere;
    GLuint m_program = 0;
    mutable EntityDebugRendererStats m_frameStats{};
};

} // namespace heritage::graphics
