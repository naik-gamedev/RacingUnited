# Lua Binding Architecture

## Purpose

Heritage Engine exposes native services to module Lua without making the runtime
itself a dumping ground. `LuaModuleRuntime` owns the Lua state, module lifecycle,
sandbox, ordered domain registration, hot reload, safety smoke tests and runtime
API introspection. Individual API handlers and their public registration tables
live below `Core/Modules/LuaBindings`.

The public Lua API is unchanged by this source split. Modules still see namespaces
such as `Physics`, `Vehicle`, `Entity`, `UI`, `Input`, `Audio`, `Scene` and `Module`.

## Source ownership

`LuaModuleRuntime::registerBindings()` remains the one ordered startup call site,
but it no longer contains hundreds of `registerFunction(...)` entries. It owns only
the global `print` override and calls domain methods such as:

```text
registerUiBindings()
registerEngineBindings()
registerInputBindings()
registerPhysicsBindings()
registerVehicleBindings()
registerEntityBindings()
...
```

Small domains register beside their handlers. Large domains that already span
multiple handler files use a dedicated registration translation unit:

```text
Core/Modules/LuaBindings/
  LuaUiBindings.cpp                # UI handlers + UI registration
  LuaEngineBindings.cpp            # Engine handlers + Engine registration
  LuaEnvironmentBindings.cpp
  LuaVegetationBindings.cpp
  LuaScriptBindings.cpp
  LuaSceneBindings.cpp
  LuaSaveBindings.cpp
  LuaAudioBindings.cpp
  LuaInputBindings.cpp
  LuaModuleBindings.cpp
  LuaPrefabBindings.cpp

  Physics/
    LuaPhysicsBindingRegistration.cpp
    LuaPhysicsWorldBindings.cpp
    LuaPhysicsBodyBindings.cpp
    LuaPhysicsColliderBindings.cpp
    LuaPhysicsQueryBindings.cpp
    LuaPhysicsConstraintBindings.cpp

  Vehicle/
    LuaVehicleBindingRegistration.cpp
    LuaVehicleDefinitionBindings.cpp
    LuaVehicleDefinitionParser.cpp
    LuaVehicleSuspensionBindings.cpp
    LuaVehicleControlBindings.cpp
    LuaVehicleTireBindings.cpp
    LuaVehicleDrivetrainBindings.cpp
    LuaVehicleDynamicsLabBindings.cpp
    LuaVehicleTelemetryBindings.cpp

  Entity/
    LuaEntityBindingRegistration.cpp
    LuaEntityCoreBindings.cpp
    LuaEntityTransformBindings.cpp
    LuaEntityDebugBindings.cpp
    LuaEntityMeshBindings.cpp
```

## Growth rule

Directories identify the broad subsystem. Files identify the concrete
responsibility. When a binding file starts accumulating independently changing
mechanisms, split it again before it becomes another monolith. Do not create a
single `LuaPhysicsBindings.cpp` or `LuaVehicleBindings.cpp` merely to reduce the
number of files.

The repository validator treats 1200 lines in one binding implementation
translation unit as an architectural warning/failure boundary. This is not a
claim that 1200 lines is inherently bad; it is an early signal to decide whether
that file now owns more than one responsibility.

## Registration and compatibility

Registration **order** remains explicit in `LuaModuleRuntime.cpp`; registration
**content** belongs to the domain that owns the API. Aliases may intentionally
point to the same C++ handler.

`GenerateLuaApiManifest.ps1` scans the runtime plus all binding-domain translation
units for exact `registerFunction(...)` calls. It records the actual registration
source and resolves every registered handler to the translation unit that
implements it.

A source reorganization must not silently rename or remove Lua bindings. API
changes require their own compatibility/migration decision.

## Engine/module boundary

Binding implementations expose generic Heritage Engine services. They must not
contain Racing United-specific gameplay, vehicle names, scene IDs or module
fallbacks. Racing United-specific orchestration belongs below `Modules/RacingUnited`.

This keeps the same engine usable for content-driven modules where new characters,
vehicles, stages, scenes, music and rules can be added without rebuilding
game-specific behavior into the engine core.

## Safety contract

`Tools/ValidateProject.ps1` verifies that:

- the split binding source tree exists;
- representative binding and registration files are compiled by the Visual Studio project;
- `LuaModuleRuntime.cpp` remains a compact runtime/registration orchestrator;
- large Physics/Vehicle/Entity registration tables are not reintroduced there;
- individual binding implementation files do not become new monoliths;
- historically fragile vector argument mappings are checked across all binding files;
- the generated API manifest still contains the required public functions.

`Tools/GenerateLuaApiManifest.ps1` records the exact registration and handler source
files and hashes the whole Lua runtime/binding source set.
## CLEAN12 private handler boundary

CLEAN12 removes the 400 domain Lua C-handler declarations from `LuaModuleRuntime.hpp`.
The runtime header now declares only lifecycle/orchestration state plus four private friend
catalogues:

```text
LuaCoreBindingHandlers
LuaPhysicsBindingHandlers
LuaVehicleBindingHandlers
LuaEntityBindingHandlers
```

The corresponding private declaration headers live beside their domains. Binding
translation units still access runtime-owned services through the friend boundary, but
ordinary users of `LuaModuleRuntime.hpp` no longer parse hundreds of unrelated handler
declarations. The runtime header also no longer includes Audio/Input/Physics/Entity/
Environment/Vegetation implementation headers merely because it stores service pointers;
those dependencies are included by the binding translation unit that actually uses them.

This is intentionally an implementation boundary, not a second Lua API. The public Lua
names and registration order remain unchanged. `GenerateLuaApiManifest.ps1` resolves the
new domain handler owners and still verifies every registered handler implementation.

## Validator ownership

The static repository safety net remains invoked through exactly one public entry point:
`Tools/ValidateProject.ps1`. CLEAN12 makes that file a small runner and moves the actual
checks under `Tools/Validation/`:

```text
00_FoundationAndLuaApi.ps1
10_CodeArchitecture.ps1
20_PhysicsAndRegression.ps1
30_VehicleAndContentArchitecture.ps1
40_RuntimeAndBuildHygiene.ps1
```

These files are dot-sourced in a fixed order so existing shared variables and `Check()`
semantics remain unchanged. New validation rules should go to the module that owns the
mechanism rather than growing the runner back into a multi-thousand-line script.

