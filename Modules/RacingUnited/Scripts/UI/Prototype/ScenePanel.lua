-- Prototype scene / player-world controls.
function DrawPlayerWorldPanel()
    UI.TextDisabled("DRIVEABLE PLAYER SCENE - STEP 29J.4")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Drop your own 1:1 Blender-authored environment into the Player Scene slots and drive the current vehicle inside it. X = left/right, Y = forward/backward, Z = height.")
    UI.Text("Visual: " .. playerWorld.visualAsset)
    UI.Text("Collision: " .. playerWorld.collisionAsset)
    UI.Text("Loaded: " .. tostring(playerWorld.loaded))
    UI.Text("Drive-surface triangles: " .. tostring(playerWorld.collisionTriangleCount))
    UI.Text("Spawn source: " .. tostring(playerWorld.spawnMode))
    UI.Text(string.format(
        "Spawn XYZ (engine): %.3f, %.3f, %.3f",
        playerWorld.spawnPosition[1],
        playerWorld.spawnPosition[2],
        playerWorld.spawnPosition[3]))
    UI.TextDisabled("The visual OBJ is hot-reloaded. Press reload below after changing the collision OBJ.")
    UI.Spacing()

    if UI.Button("LOAD / RELOAD PLAYER SCENE") then
        LoadPlayerWorld()
    end
    if UI.Button("RESET VEHICLE AT PLAYER SPAWN") then
        if playerWorld.loaded then
            ResetVehicleAtPlayerWorldSpawn(
                "Reset vehicle at Player Scene spawn")
        else
            playerWorld.message = "Load the Player Scene first"
        end
    end
    if UI.Button("RETURN TO PROTOTYPE LAB") then
        ReturnToPrototypeLab()
    end

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("BLENDER AUTHORING CONTRACT")
    UI.Text("X = left / right")
    UI.Text("Y = forward / backward")
    UI.Text("Z = height")
    UI.Text("1 Blender unit = 1 metre")
    UI.TextWrapped("Step 29J.4 converts Blender OBJ export coordinates internally for the Player Scene. Do not rotate or rescale the scene merely to satisfy Heritage Engine.")
    UI.TextWrapped("SPAWN_PLAYER may live in either PlayerScene.obj or PlayerScene_Collision.obj. Heritage Engine uses the authored marker when present; otherwise it finds the terrain directly beneath world origin instead of spawning on top of a whole-mesh bounding box.")
    UI.Spacing()
    UI.TextDisabled("Drive-surface bridge: PlayerScene_Collision.obj triangles are queried directly by suspension/tire raycasts. Object names containing ROAD/ASPHALT, GRASS, GRAVEL, DIRT, SNOW, ICE, KERB/CURB or PAINT/LINE feed per-wheel surface identity. Full chassis-vs-triangle rigid-body collision comes later.")
    UI.Spacing()
    UI.TextDisabled(playerWorld.message)
end

function DrawPrototypeDebugScenePanel()
    UI.TextDisabled("PROTOTYPE LAB PRESENTATION")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("These presets only control the old laboratory/debug presentation. Use WORLD for your own driveable scene.")
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
        if UI.BeginTabItem("DEBUG / LAB") then
            DrawPrototypeDebugScenePanel()
            UI.EndTabItem()
        end
        UI.EndTabBar()
    end
end
