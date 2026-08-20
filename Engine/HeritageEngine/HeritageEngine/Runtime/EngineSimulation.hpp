#pragma once

namespace heritage::camera {
class ChaseCamera;
class VehicleCameraController;
struct CameraFrame;
struct ChaseCameraInput;
struct VehicleCameraFlyInput;
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
    heritage::camera::VehicleCameraController& vehicleCamera,
    const heritage::camera::ChaseCameraInput& chaseCameraInput,
    const heritage::camera::VehicleCameraFlyInput& vehicleCameraFlyInput,
    heritage::camera::CameraFrame& entityCameraFrame,
    heritage::diagnostics::PerformanceMonitor& performanceMonitor);

} // namespace heritage::engine
