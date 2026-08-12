# Module Asset Discovery (AS01)

AS01 adds a small, engine-level asset index for the **active module**. It is the
bridge between "I copied/exported a file into `Modules/<Module>/Assets`" and
higher-level systems being able to discover that file without a Windows file
picker or a hard-coded path.

## What it does

`ModuleAssetRegistry` recursively scans the active module `Assets` directory at
most once per second and records:

- module-relative asset path;
- lowercase extension;
- file size;
- last-write time.

Its `revision` only changes when the indexed file set or file metadata changes.
This lets Lua/UI code cheaply ask whether anything changed before doing more
expensive work such as parsing GLB metadata.

The registry is deliberately generic. It does **not** instantiate every file it
finds. A newly copied texture is not a scene object and a newly copied wheel GLB
is not automatically a complete car. Higher-level systems choose what a file
means.

## Lua API

```lua
Module.GetAssetIndexRevision()
Module.GetAssetCount(extension, directoryPrefix, fileNamePrefix)
Module.GetAssetPath(index, extension, directoryPrefix, fileNamePrefix)
Module.GetLatestAsset(extension, directoryPrefix, fileNamePrefix)
Module.RefreshAssetIndex()
```

All paths returned to Lua are relative to the current module's `Assets` folder.
Filters are optional and case-insensitive where appropriate.

Example:

```lua
local latestVehicle = Module.GetLatestAsset(
    ".glb",
    "Vehicles",
    "Vehicle_")
```

This finds the most recently written `Vehicle_*.glb` anywhere beneath
`Assets/Vehicles`.

## Racing United development convenience

Racing United's current vehicle visual slot now watches that query.

By default, when the prototype is still using the historical creator
`PlayerCar.obj` slot, exporting/copying a file such as:

```text
Modules/RacingUnited/Assets/Vehicles/Peugeot206RC/Vehicle_Peugeot_206_RC.glb
```

causes it to appear in the asset index within about one second and the latest
`Vehicle_*.glb` becomes the development vehicle visual automatically.

The Body visual panel shows:

- whether automatic discovery is enabled;
- current asset-index revision;
- number of matching `Vehicle_*.glb` files;
- latest matching path;
- manual Refresh / Use Latest controls.

A manually selected OBJ/GLB is not unexpectedly replaced: automatic discovery
only owns the legacy default slot or a path that it previously auto-selected.

## Hot reload vs discovery

These are related but different:

- **Discovery** notices a new/removed/renamed asset and makes the path available.
- Existing renderer **hot reload** notices edits to a selected GLB/texture and
  refreshes the render resource.

AS01 also causes Racing United to re-read VA01 GLB semantic metadata when the
asset index changes, so updated Blender Custom Properties can become visible in
the Asset Data panel without selecting the file again.

## Why not auto-load every GLB?

Because `Assets/Vehicles` will eventually contain complete vehicles, wheels,
tires, spoilers, bumpers, brakes and other components. Loading every discovered
file as a chassis would be incorrect. The naming/metadata contract remains the
selector:

- complete development vehicles: `Vehicle_*.glb`;
- future modular parts: discovered/catalogued by VA metadata and mounted through
  the modular-part system.

A future filesystem-watcher backend can replace periodic scanning without
changing the Lua contract.

## SC01 scene discovery and UTF-8 paths

SC01 also uses the same index for creator worlds:

```lua
Module.GetLatestAsset(".glb", "Scenes", "Scene_")
```

The Player World waits for the lazy first index scan, then selects the newest
`Scene_*.glb` under `Assets/Scenes`. The visual renderer hot-reloads ordinary
GLB changes; collision/spawn changes are applied when the world is reloaded.

Module-facing relative paths are UTF-8 rather than the Windows active code page.
This keeps Central-European content names such as `Scene_Ivarčko_Jezero.glb`
usable through discovery, entity mesh loading, metadata inspection and static
scene import.
