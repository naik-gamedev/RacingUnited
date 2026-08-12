# ADR-052 — Input and glTF implementation ownership

Status: Accepted candidate (CLEAN06)

## Context

`InputSystem.cpp` had grown to more than 3,200 lines and mixed lifecycle, action/binding editing,
hardware polling, named profiles and persistence. `GltfBinary.cpp` had grown to more than 2,400 lines
and mixed JSON parsing, GLB/accessor decoding, materials, meshes, animation, metadata and collision
authoring extraction.

Both public APIs are already widely used, so changing caller-facing contracts during a cleanup would
create unnecessary risk.

## Decision

Keep `InputSystem.hpp` as the stable public input facade while physically separating implementation by
ownership: coordinator, bindings, devices, profiles and persistence. Shared helper code is private in
`InputSystemInternal.hpp` and must not become a second public API.

Keep `Graphics/GltfBinary.hpp` and `GltfSceneData.hpp` as the stable public glTF contracts. Compile the
implementation from `Graphics/Gltf/`, partitioned into JSON, document/accessor decoding, mesh import,
metadata and collision authoring. `GltfInternal.hpp` is private importer vocabulary only.

The legacy `Graphics/GltfBinary.cpp` remains a non-compiled signpost during migration so new importer
logic has an obvious destination and cannot silently rebuild the old dumping ground.

## Consequences

Incremental edits to input profiles, device handling or glTF collision extraction no longer require
recompiling unrelated large translation units. Future features have an explicit owner. Call sites and
Lua/native public interfaces remain unchanged.

CLEAN06 intentionally changes no input semantics, GLB coordinate conventions, material behavior,
animation behavior, metadata interpretation or collision extraction policy.
