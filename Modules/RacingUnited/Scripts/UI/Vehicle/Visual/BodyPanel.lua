-- Step 29J.2 body-mesh slot. Creator-authored geometry is authoritative:
-- no routine body translation/rotation/scale controls are exposed anymore.
function DrawVehicleVisualBodyPanel()
    local changed = false
    local meshChanged = false

    UI.TextDisabled("PLAYER BODY VISUAL")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("The creator-owned body slot accepts OBJ or GLB at authored 1:1 size and placement. GLB assets can additionally carry Heritage semantic metadata through Blender Custom Properties / glTF extras.")
    UI.Text("Asset: " .. tostring(vehicleVisual.assetPath))
    UI.Text("Mode: " .. (vehicleVisual.usingFallback and "fallback prototype mesh" or "player-car slot"))
    UI.Text("Runtime transform: identity / 1:1")
    UI.TextDisabled("If geometry is positioned or sized incorrectly, fix the source scene in Blender rather than compensating here.")
    UI.Spacing()

    if UI.Button("USE PLAYER CAR ASSET") then
        UsePlayerVehicleVisual()
    end
    UI.SameLine()
    if UI.Button("USE FALLBACK LOW-POLY") then
        UseFallbackVehicleVisual()
    end

    if UI.Button("SELECT OBJ / GLB FROM ASSETS...") then
        local selectedAsset, selectionError = Module.SelectAssetFile()
        if selectedAsset ~= nil then
            vehicleAssetDiscovery.autoOwnedPath = ""
            vehicleVisual.assetPath = selectedAsset
            vehicleVisual.usingFallback = false
            ApplyVehicleVisualMesh()
        elseif selectionError ~= nil then
            vehicleVisualMessage = tostring(selectionError)
        end
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("AUTOMATIC ASSET DISCOVERY")
    UI.TextWrapped("Heritage indexes the active module Assets tree once per second. Files named Vehicle_*.glb anywhere under Assets/Vehicles appear here automatically; the latest one can replace the legacy PlayerCar.obj development slot without using the file picker.")

    local discoveryChanged = false
    vehicleAssetDiscovery.enabled, discoveryChanged = UI.Checkbox(
        "Auto-load latest Vehicle_*.glb", vehicleAssetDiscovery.enabled)
    if discoveryChanged then
        SetVehicleAssetAutoDiscoveryEnabled(vehicleAssetDiscovery.enabled)
    end

    UI.Text("Asset index revision: " .. tostring(Module.GetAssetIndexRevision()))
    UI.Text("Detected Vehicle_*.glb: " .. tostring(vehicleAssetDiscovery.detectedCount or 0))
    UI.Text("Latest: " .. tostring(vehicleAssetDiscovery.latestVehicleGlb ~= "" and vehicleAssetDiscovery.latestVehicleGlb or "none"))
    UI.TextDisabled(tostring(vehicleAssetDiscovery.message))

    if UI.Button("REFRESH ASSET INDEX NOW") then
        RefreshVehicleAssetDiscovery(true, false)
    end
    UI.SameLine()
    if UI.Button("USE LATEST DETECTED GLB") then
        UseLatestDiscoveredVehicleGlb()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("PRESENTATION")

    local isolateChanged = false
    vehicleVisual.isolateWheelAssembly, isolateChanged = UI.Checkbox(
        "DIAGNOSTIC: isolate wheels / tires / brakes (WH_*)",
        vehicleVisual.isolateWheelAssembly)
    if isolateChanged then
        SetVehicleWheelAssemblyIsolation(vehicleVisual.isolateWheelAssembly)
    end
    UI.TextWrapped("This hides every GLB draw node outside the WH_* wheel subtrees. Use it for clean steering screenshots; it does not change vehicle physics or the authored GLB.")
    UI.Spacing()

    vehicleVisual.hideProxyWheels, changed = UI.Checkbox(
        "Hide prototype wheel cylinders", vehicleVisual.hideProxyWheels)
    if changed then
        SetVehicleProxyWheelPreference(vehicleVisual.hideProxyWheels)
    end

    vehicleVisual.doubleSided, changed = UI.Checkbox(
        "Double-sided body mesh", vehicleVisual.doubleSided)
    meshChanged = meshChanged or changed

    vehicleVisual.normalize, changed = UI.Checkbox(
        "Normalize mesh to unit size (diagnostic only)", vehicleVisual.normalize)
    meshChanged = meshChanged or changed

    vehicleVisual.color[1], changed = UI.SliderFloat(
        "Body color R", vehicleVisual.color[1], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleVisual.color[2], changed = UI.SliderFloat(
        "Body color G", vehicleVisual.color[2], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed
    vehicleVisual.color[3], changed = UI.SliderFloat(
        "Body color B", vehicleVisual.color[3], 0.0, 1.0, "%.3f")
    meshChanged = meshChanged or changed

    if meshChanged then
        ApplyVehicleVisualMesh()
    end

    UI.Spacing()
    UI.TextDisabled("Tip: double-click any numeric slider in the prototype UI to type an exact value and press Enter.")

    if UI.Button("RESET BODY TO AUTHORED IDENTITY") then
        ResetVehicleVisualTuning()
    end
    UI.SameLine()
    if UI.Button("RESET CAR ON DRY ASPHALT") then
        ResetNativeVehicle()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextWrapped("Body slot: Modules\\RacingUnited\\Assets\\Vehicles\\Player\\ (OBJ or GLB)")
    UI.TextDisabled(vehicleVisualMessage)
end
