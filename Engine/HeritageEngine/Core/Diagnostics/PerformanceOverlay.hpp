#pragma once

#include <cstddef>

#include "PerformanceMonitor.hpp"
#include "../Jobs/JobSystem.hpp"
#include "../../Graphics/Renderer/EntityDebugRenderer.hpp"
#include "../../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../../Graphics/Renderer/SurfacePresentationRenderer.hpp"
#include "../../Graphics/Renderer/WeatherPresentationRenderer.hpp"
#include "../../Graphics/VegetationSystem.hpp"

namespace heritage::diagnostics {

void drawPerformanceOverlay(
    const PerformanceSnapshot& performance,
    const heritage::graphics::EntityMeshRendererStats& meshStats,
    const heritage::graphics::EntityDebugRendererStats& debugStats,
    const heritage::graphics::SurfacePresentationRendererStats& surfaceStats,
    const heritage::graphics::WeatherPresentationRendererStats& weatherStats,
    const heritage::graphics::VegetationStats& vegetationStats,
    std::size_t entityCount,
    std::size_t loadedAssetCount,
    const heritage::jobs::JobSystemStats& jobStats,
    int physicsWorldSteps,
    bool physicsOverloaded,
    bool vsyncEnabled,
    int fpsCap);

} // namespace heritage::diagnostics
