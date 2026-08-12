-- Racing United split runtime file. Loaded by Scripts/Main.lua through Script.Include.
function LoadScene(sceneId)
    if not Scene.Load(sceneId) then
        transitionMessage = Scene.GetLastError()
        Engine.Log(transitionMessage)
    end
end

function FlushSave(message)
    if Save.Flush() then
        saveMessage = message
    else
        saveMessage = "SAVE ERROR: " .. Save.GetLastError()
        Engine.Log(saveMessage)
    end
end

function PlayUiConfirmation()
    local handle = Audio.PlaySound("Audio/ui_confirm.wav", "UI", 0.90, 1.0)
    if handle == 0 then
        audioMessage = "AUDIO ERROR: " .. Audio.GetLastError()
    else
        audioMessage = "Played a module-owned UI sound"
    end
end

function StartAmbience()
    if ambienceHandle ~= 0 and Audio.IsPlaying(ambienceHandle) then
        audioMessage = "Countryside ambience is already playing"
        return
    end

    ambienceHandle = Audio.PlayLoop(
        "Audio/countryside_ambience_loop.wav",
        "Ambience",
        ambienceVolume,
        1.0)

    if ambienceHandle == 0 then
        audioMessage = "AUDIO ERROR: " .. Audio.GetLastError()
    else
        audioMessage = "Started looping module ambience"
    end
end

function StopAmbience()
    if ambienceHandle ~= 0 then
        Audio.Stop(ambienceHandle)
        ambienceHandle = 0
    end
    audioMessage = "Stopped module ambience"
end

function ClearSceneEntityHandles()
    playerEntity = 0
    chassisEntity = 0
    wheelFrontLeft = 0
    wheelFrontRight = 0
    wheelRearLeft = 0
    wheelRearRight = 0
    cameraMountEntity = 0
    cabinEntity = 0
    noseMarkerEntity = 0
end

function RefreshSceneEntityHandles()
    playerEntity = Entity.FindByName("Player Vehicle Root")
    chassisEntity = Entity.FindByName("Player Chassis")
    cabinEntity = 0
    noseMarkerEntity = 0
    wheelFrontLeft = Entity.FindByName("Wheel Front Left")
    wheelFrontRight = Entity.FindByName("Wheel Front Right")
    wheelRearLeft = Entity.FindByName("Wheel Rear Left")
    wheelRearRight = Entity.FindByName("Wheel Rear Right")
    cameraMountEntity = Entity.FindByName("Driver Camera Mount")

    if playerEntity ~= 0 and Entity.Exists(playerEntity) then
        entityMessage = "Loaded a reusable vehicle prefab through prototype.hscene"
        return true
    end

    entityMessage = "prototype.hscene did not create Player Vehicle Root: "
        .. Entity.GetLastError()
    return false
end

function SetVehicleProxyWheelsVisible(visible)
    local wheelEntities = {
        wheelFrontLeft,
        wheelFrontRight,
        wheelRearLeft,
        wheelRearRight
    }
    for _, entity in ipairs(wheelEntities) do
        if entity ~= 0 and Entity.Exists(entity) and Entity.HasDebugPrimitive(entity) then
            Entity.SetDebugVisible(entity, visible)
        end
    end
end

function SetVehicleDebugVisible(visible)
    local nonWheelEntities = {
        chassisEntity,
        cabinEntity,
        noseMarkerEntity,
        cameraMountEntity
    }
    for _, entity in ipairs(nonWheelEntities) do
        if entity ~= 0 and Entity.Exists(entity) and Entity.HasDebugPrimitive(entity) then
            Entity.SetDebugVisible(entity, visible)
        end
    end

    local hideProxyWheels = vehicleVisual ~= nil and vehicleVisual.hideProxyWheels
    local articulatedWheels = vehicleWheelVisual ~= nil and vehicleWheelVisual.enabled
    SetVehicleProxyWheelsVisible(
        visible and not hideProxyWheels and not articulatedWheels)
    if RefreshVehicleWheelVisibility ~= nil then
        RefreshVehicleWheelVisibility()
    end
end

function DestroyBodyAndEntity(bodyHandle, entityHandle)
    if bodyHandle ~= 0 and Physics.BodyExists(bodyHandle) then
        Physics.DestroyBody(bodyHandle)
    end
    if entityHandle ~= 0 and Entity.Exists(entityHandle) then
        Entity.Destroy(entityHandle)
    end
end


function RemoveEntitiesByName(name)
    while true do
        local entity = Entity.FindByName(name)
        if entity == 0 or not Entity.Exists(entity) then
            break
        end
        local body = Physics.FindBodyByEntity(entity)
        DestroyBodyAndEntity(body, entity)
    end
end
