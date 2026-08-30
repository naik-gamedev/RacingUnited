# STUDIO09 - Runtime Scene Spawn Bridge + Orbitron Editor Typography

## Goal

Remove the manual copy/paste step between Heritage Studio scene authoring and the Racing United runtime while keeping a recovery copy.

## Typography

Heritage Studio now loads `Assets/Fonts/Orbitron-SemiBold.ttf` as its default 15 px ImGui font. Headings use the same font at 17 px. The built-in ImGui font is only a fallback when the bundled font cannot be loaded.

## Authoritative runtime scene

Studio resolves the module's actual `entry_scene` from `Modules/RacingUnited/module.ini` instead of assuming a hard-coded filename. For Racing United this currently resolves to:

`Modules/RacingUnited/Scenes/prototype.hscene`

The Scene workspace still keeps its editor-owned recovery copy under:

`UserData/Modules/RacingUnited/HeritageStudio/Projects/RacingUnited/scene.hscene`

## Vehicle spawn publishing

The first enabled Scene-workspace `Vehicle Spawn` is the runtime player-vehicle spawn. If no enabled Vehicle Spawn exists, Studio leaves the runtime scene untouched so the existing GLB-authored spawn remains authoritative.

On Scene SAVE or global Ctrl+S, Studio patches only its owned section in the real module entry scene:

`[entity:heritage_studio_vehicle_spawn]`

The marker carries position and rotation. Existing scene metadata, prefab instances and other hand-authored sections are preserved.

Before overwriting the runtime scene, Studio writes a safety copy to:

`UserData/Modules/RacingUnited/HeritageStudio/Projects/RacingUnited/RuntimeBackups/prototype_before_last_studio_save.hscene`

If that backup fails, Studio refuses to overwrite the runtime scene.

## Runtime behavior

`PlayerWorld.lua` now checks for the `Heritage Studio Vehicle Spawn` entity after the world GLB and collision are loaded. When present, it overrides the GLB-derived spawn position and rotation. When absent, the pre-STUDIO09 GLB spawn path is unchanged.

`ResetNativeVehicleAt` accepts optional Euler rotation so a Studio-authored spawn heading is preserved on initial spawn and later resets.
