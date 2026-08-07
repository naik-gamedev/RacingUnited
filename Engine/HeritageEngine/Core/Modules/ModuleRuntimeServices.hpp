#pragma once

namespace heritage::audio {
class AudioSystem;
}

namespace heritage::input {
class InputSystem;
}

namespace heritage::entities {
class EntityRegistry;
}

namespace heritage::physics {
class PhysicsWorld;
}

namespace heritage::modules {

// Engine services intentionally exposed to the active module runtime.
// Future scene, input, entity, physics and vehicle services can be added here
// without giving modules direct access to main.cpp or private engine systems.
struct ModuleRuntimeServices
{
    heritage::audio::AudioSystem* audio = nullptr;
    heritage::input::InputSystem* input = nullptr;
    heritage::entities::EntityRegistry* entities = nullptr;
    heritage::physics::PhysicsWorld* physics = nullptr;
};

} // namespace heritage::modules
