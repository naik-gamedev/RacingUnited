#pragma once

namespace heritage::camera {
class ChaseCamera;
struct CameraFrame;
}
namespace heritage::diagnostics { class PerformanceMonitor; }
namespace heritage::entities { class EntityRegistry; }
namespace heritage::graphics { class EnvironmentSystem; }
namespace heritage::modules { class ModuleRuntimeManager; }
namespace heritage::physics { class PhysicsWorld; }

namespace heritage::engine {

void updateEngineSimulation(
    float dt,
    double now,
    bool menuOpen,
    heritage::physics::PhysicsWorld& physics,
    heritage::modules::ModuleRuntimeManager& moduleRuntime,
    heritage::graphics::EnvironmentSystem& environmentSystem,
    heritage::entities::EntityRegistry& entityRegistry,
    heritage::camera::ChaseCamera& chaseCamera,
    heritage::camera::CameraFrame& entityCameraFrame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor);

} // namespace heritage::engine
