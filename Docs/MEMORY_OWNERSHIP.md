# Memory Ownership and Lifetime Rules

## Core rule

Native C++ services own engine objects. Lua and neighboring subsystems receive opaque generation-checked handles, not owning raw pointers.

A handle encodes a storage index and generation. Destroying an object invalidates the generation so stale handles fail validation rather than resolving to a newly allocated object in the same slot.

## Current owners

| Object | Owner | Lua representation | Destruction path |
|---|---|---|---|
| Entity | `EntityRegistry` | `EntityHandle` integer | `Entity.Destroy` / registry clear |
| Rigid body | `PhysicsWorld::RigidBodySystem` | `BodyHandle` integer | `PhysicsWorld::destroyBody` |
| Collider | `PhysicsWorld::CollisionSystem` | `ColliderHandle` integer | collider destroy or body cascade |
| Constraint | `PhysicsWorld::ConstraintSystem` | `ConstraintHandle` integer | constraint destroy or body cascade |
| Vehicle | `PhysicsWorld::VehicleSystem` | `VehicleHandle` integer | vehicle destroy or chassis-body cascade |
| Audio voice | `AudioSystem` | validated audio handle | explicit stop, completion, or module shutdown |
| Lua state | `LuaModuleRuntime` | not exposed | module reload/shutdown |

## Body destruction ordering

`PhysicsWorld::destroyBody` must destroy dependent vehicles, constraints, and colliders before invalidating the rigid-body handle. This ordering is validated by the runtime safety smoke test.

## Cross-system references

- Store handles when the referenced object's lifetime can end independently.
- Validate every handle at the public API boundary.
- Never retain pointers into a vector or slot array across operations that may reallocate it.
- Never let Lua own or delete a native object through garbage collection alone.
- Defer destruction when a subsystem is iterating the same collection or when another thread may still read it.
- Renderer, physics, audio, networking, and AI queues must either resolve handles at use time or hold explicit lifetime-managed snapshots.

## Module reload

On Lua reload:

- The old Lua state receives `OnShutdown` when safe.
- Module-owned audio voices are stopped.
- UI image state is cleared.
- The old Lua state is destroyed.
- Native scene/vehicle cleanup must be explicit through lifecycle functions or module reset paths.

## Compiled vehicle definitions

`Vehicle.CompileDefinitionV2` and `Vehicle.CreateFromDefinitionV2` copy bounded
Lua table data into native value types during the call. The compiler and loader
never retain a `lua_State*`, table index, Lua string pointer, or other borrowed
Lua memory. A created vehicle is owned by `VehicleSystem` and follows the normal
generation-handle and chassis-body cascade rules.

## Diagnostics

Use `Engine.RunSafetySmokeTests()` to verify generation invalidation and body-dependent cascade cleanup. AddressSanitizer is available through `Tools/BuildAddressSanitizerDebug.cmd` for deeper native lifetime errors.
