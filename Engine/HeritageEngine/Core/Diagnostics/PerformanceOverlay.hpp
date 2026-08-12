#pragma once

#include <cstddef>

#include "PerformanceMonitor.hpp"
#include "../../Graphics/Renderer/EntityDebugRenderer.hpp"
#include "../../Graphics/Renderer/EntityMeshRenderer.hpp"
#include "../../Graphics/VegetationSystem.hpp"

namespace heritage::diagnostics {

void drawPerformanceOverlay(
    const PerformanceSnapshot& performance,
    const heritage::graphics::EntityMeshRendererStats& meshStats,
    const heritage::graphics::EntityDebugRendererStats& debugStats,
    const heritage::graphics::VegetationStats& vegetationStats,
    std::size_t entityCount,
    std::size_t loadedAssetCount,
    int physicsWorldSteps,
    bool physicsOverloaded);

} // namespace heritage::diagnostics
