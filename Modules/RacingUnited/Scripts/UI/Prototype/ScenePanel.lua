-- Prototype scene / player-world controls.
function DrawPlayerWorldPanel()
    UI.TextDisabled("DRIVEABLE GLB WORLD")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Racing United now discovers Scene_*.glb files under Assets/Scenes. One GLB can contain visible geometry, hidden static collision authoring, spawn metadata and surface metadata.")
    UI.Text("Detected Scene_*.glb: " .. tostring(playerWorld.detectedSceneCount))
    UI.Text("Latest: " .. tostring(playerWorld.latestSceneGlb))
    UI.Text("Active: " .. tostring(playerWorld.sceneAsset))
    UI.Text("Loaded: " .. tostring(playerWorld.loaded))
    UI.Text("Collision mode: " .. tostring(playerWorld.collisionMode))
    UI.Text("Using fallback collision: " .. tostring(playerWorld.usingFallbackCollision))
    UI.Text("Drive-surface triangles: " .. tostring(playerWorld.collisionTriangleCount))
    UI.Text("Spawn source: " .. tostring(playerWorld.spawnMode))
    UI.Text(string.format(
        "Spawn XYZ (local engine frame): %.3f, %.3f, %.3f",
        playerWorld.spawnPosition[1],
        playerWorld.spawnPosition[2],
        playerWorld.spawnPosition[3]))
    local originX, originY, originZ = Physics.GetWorldOrigin()
    UI.Text(string.format(
        "FP64 world origin: %.3f, %.3f, %.3f m",
        originX or 0.0, originY or 0.0, originZ or 0.0))
    UI.Text("Origin rebases: " .. tostring(Physics.GetOriginRebaseCount()))
    UI.TextDisabled("Visible GLB geometry hot-reloads. Reload the world after editing collision geometry or SPAWN_PLAYER.")
    UI.Spacing()

    if UI.Button("USE LATEST Scene_*.glb") then
        UseLatestDiscoveredSceneGlb()
    end
    if UI.Button("LOAD / RELOAD ACTIVE WORLD") then
        LoadPlayerWorld()
    end
    if UI.Button("REFRESH ASSET INDEX") then
        RefreshPlayerWorldAssetDiscovery(true)
    end
    if UI.Button("RESET VEHICLE AT PLAYER SPAWN") then
        if playerWorld.loaded then
            ResetVehicleAtPlayerWorldSpawn(
                "Reset vehicle at Player World spawn")
        else
            playerWorld.message = "Load a GLB world first"
        end
    end
    if UI.Button("RETURN TO PROTOTYPE LAB") then
        ReturnToPrototypeLab()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("BLENDER / GLB AUTHORING CONTRACT")
    UI.Text("1 Blender unit = 1 metre")
    UI.TextWrapped("Export glTF Binary (.glb). Heritage uses glTF coordinates directly; do not rotate or rescale the scene merely to satisfy the engine.")
    UI.Spacing()
    UI.Text("Collision object example: Scene_Ivarcko_Jezero_Collision")
    UI.Text("heritage.role = collision_mesh")
    UI.Text("heritage.collision_type = static_triangle_mesh")
    UI.TextWrapped("The Custom Properties are preferred. *_Collision / Collision_* names are also recognized as a convenient fallback. Collision nodes and their descendants are hidden automatically by the visual renderer.")
    UI.Spacing()
    UI.Text("Optional drive-surface property:")
    UI.Text("heritage.surface = asphalt / grass / gravel / dirt / snow / ice / kerb / paint")
    UI.Text("heritage.wetness = 0.0 .. 1.0")
    UI.TextWrapped("Surface names still provide a fallback, so Road_Asphalt, Grass_Collision and similar authoring names remain useful.")
    UI.TextWrapped("If no authored collision mesh exists yet, Heritage now keeps the pretty Scene_*.glb visible and silently falls back to one hidden flat asphalt floor at origin so the Peugeot remains driveable while you keep authoring the scene.")
    UI.Spacing()
    UI.Text("Spawn: Blender Empty or node named SPAWN_PLAYER")
    UI.Text("Optional: heritage.role = spawn_player")
    UI.TextWrapped("An Empty is enough. Heritage uses the node transform, then snaps its height to the imported collision surface. If no marker exists, the deterministic origin/nearest-terrain fallback remains available.")
    UI.Spacing()
    UI.TextDisabled(playerWorld.message)
end

function DrawPrototypeDebugScenePanel()
    UI.TextDisabled("PROTOTYPE LAB PRESENTATION")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("These presets only control the old laboratory/debug presentation. Use WORLD for your own driveable GLB scene.")
    UI.Text("Active preset: " .. tostring(prototypeScenePreset))
    UI.Spacing()

    if UI.Button("CLEAN VEHICLE VIEW") then
        SetPrototypeScenePreset("vehicle")
    end
    if UI.Button("CLEAN VISUAL SHOWROOM VIEW") then
        SetPrototypeScenePreset("visual")
    end
    if UI.Button("SURFACE TEST VIEW") then
        SetPrototypeScenePreset("surface")
    end
    if UI.Button("PHYSICS LAB VIEW") then
        SetPrototypeScenePreset("physics")
    end
    if UI.Button("ENTITY / PREFAB VIEW") then
        SetPrototypeScenePreset("entity")
    end
    if UI.Button("SHOW ALL DEBUG GEOMETRY") then
        SetPrototypeScenePreset("all")
    end

    UI.Spacing()
    UI.Separator()
    if UI.Button("HIDE CONTROL PANEL - VIEW 3D (TAB)") then
        showPrototypeControls = false
        if Camera.IsAvailable() then
            Camera.SetUiInteractionActive(false)
        end
    end
    if UI.Button("RELOAD THIS HSCENE") then
        if not Scene.Reload() then
            transitionMessage = Scene.GetLastError()
        end
    end
    if UI.Button("BACK TO MAIN MENU") then
        LoadScene("main_menu")
    end
end

function DrawPrototypeScenePanel()
    if UI.BeginTabBar("PrototypeSceneTabs") then
        if UI.BeginTabItem("WORLD") then
            DrawPlayerWorldPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("ENVIRONMENT") then
            DrawSceneEnvironmentPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("WEATHER") then
            DrawSceneWeatherPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("VEGETATION") then
            DrawVegetationFoundationPanel()
            UI.EndTabItem()
        end
        if UI.BeginTabItem("DEBUG / LAB") then
            DrawPrototypeDebugScenePanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
