# Lua API Contract Rules

## Authoritative names

`Tools/GenerateLuaApiManifest.ps1` parses the actual C++ binding registrations and writes:

- `Build/Reports/LuaAPI.json`
- `Build/Reports/LuaAPI.md`

The running engine also writes:

- `Build/Reports/LuaAPI_Runtime.json`
- `Build/Reports/LuaAPI_Runtime.md`

The runtime files are authoritative for the exact functions registered by that executable. A future contributor must not invent a function name from memory.

## Signature annotations

`Docs/LuaApiAnnotations.json` supplies human-readable signatures and descriptions for annotated APIs. Entries marked unannotated in the generated manifest require inspection of the named C++ handler. Do not infer argument order or return values from the function name alone.

## Binding rules

- Namespace and function names use stable PascalCase public names.
- A binding validates handle generations and all external inputs.
- Errors return an explicit boolean or safe sentinel and expose a subsystem `GetLastError` where appropriate.
- Lua receives numbers, booleans, strings, and handles; it does not receive raw pointers.
- File paths are module-relative and traversal-safe.
- New bindings must be added to annotations or deliberately marked unannotated.
- Renaming a binding requires a compatibility alias or migration milestone.

## Live introspection

Available engine functions include:

- `Engine.GetBuildIdentity()`
- `Engine.GetBuildStep()`
- `Engine.GetGitCommit()`
- `Engine.GetBuildConfiguration()`
- `Engine.GetLuaApiCount()`
- `Engine.GetLuaApiName(index)`
- `Engine.DumpLuaAPI()`
- `Engine.RunSafetySmokeTests()`
- `Engine.GetLastSafetyReport()`

Lua indexes passed to `Engine.GetLuaApiName` are one-based.

## Scripted UI tab scopes

The scripted UI exposes `UI.BeginTabBar`, `UI.EndTabBar`, `UI.BeginTabItem`, and
`UI.EndTabItem` for debug/tool organization. Begin calls return whether their
content should be drawn. End calls belong inside the matching successful Begin
scope. Heritage Engine tracks open tab scopes and closes them safely if a Lua
error aborts `OnDrawUI`, preventing a broken script from corrupting ImGui's
Begin/End stack.
## Exact numeric controls

`UI.SliderFloat` remains draggable for quick tuning, but Step 29J.2 also supports
double-click-to-type exact numeric entry. Press Enter to commit or click elsewhere to
leave text-input mode. Serious setup and engineering values must never depend on mouse
precision alone.

## Responsive tool layouts

`UI.GetAvailableWidth()` returns the remaining horizontal content width in pixels.
`UI.Button` accepts `(label, width, height, centered)`; `centered` defaults to
`true` for ordinary single-button panels. Tool grids pass `false` and calculate
two-column widths from the available region so controls remain visible without a
horizontal scrollbar.

## Blender authoring boundary

Racing United creator content is authored as X left/right, Y forward/backward, Z height
at 1 Blender unit = 1 metre. APIs/importers that opt into Blender-coordinate mode convert
that content at the engine boundary; scripts must not manually rotate or rescale creator
assets to compensate for native engine axes.
