-- Live vehicle geometry presentation.
--
-- The same native wheel-centre/upright snapshot that drives suspension and the
-- articulated wheels drives these markers.  The overlay never estimates or
-- changes physics; it only makes wheelbase, track and tire width inspectable in
-- the world while a compact HUD supplies exact millimetre values.

local Transform = VehicleVisualTransformMath
local QuaternionFromEulerDegrees = Transform.QuaternionFromEulerDegrees
local QuaternionConjugate = Transform.QuaternionConjugate
local RotateVectorByQuaternion = Transform.RotateVectorByQuaternion
local EulerDegreesFromQuaternion = Transform.EulerDegreesFromQuaternion

local function MeasurementDestroyEntity(entity)
    if entity ~= nil and entity ~= 0 and Entity.Exists(entity) then
        Entity.Destroy(entity)
    end
end

function DestroyVehicleMeasurementOverlay()
    if vehicleMeasurementOverlay == nil then return end
    for _, entity in pairs(vehicleMeasurementOverlay.entities or {}) do
        MeasurementDestroyEntity(entity)
    end
    vehicleMeasurementOverlay.entities = {}
    vehicleMeasurementOverlay.labels = {}
end

local function MeasurementCreateEntity(key, primitive, color)
    if playerEntity == 0 or not Entity.Exists(playerEntity) then return 0 end
    local entity = Entity.Create("Vehicle Measurement " .. tostring(key))
    if entity == 0 then return 0 end
    Entity.SetParent(entity, playerEntity, false)
    Entity.SetDebugPrimitive(entity, primitive, color[1], color[2], color[3])
    Entity.SetDebugVisible(entity, vehicleMeasurementOverlay.enabled)
    vehicleMeasurementOverlay.entities[key] = entity
    return entity
end

local function MeasurementEntity(key, primitive, color)
    local entity = vehicleMeasurementOverlay.entities[key]
    if entity ~= nil and entity ~= 0 and Entity.Exists(entity) then
        return entity
    end
    return MeasurementCreateEntity(key, primitive, color)
end

local function NormalizeQuaternion(q)
    local length = math.sqrt(
        q[1] * q[1] + q[2] * q[2] + q[3] * q[3] + q[4] * q[4])
    if length <= 0.000001 then return { 1.0, 0.0, 0.0, 0.0 } end
    return { q[1] / length, q[2] / length, q[3] / length, q[4] / length }
end

local function QuaternionFromPositiveX(dx, dy, dz)
    local length = math.sqrt(dx * dx + dy * dy + dz * dz)
    if length <= 0.000001 then return { 1.0, 0.0, 0.0, 0.0 }, 0.0 end
    dx, dy, dz = dx / length, dy / length, dz / length
    local dot = dx
    if dot >= 0.999999 then
        return { 1.0, 0.0, 0.0, 0.0 }, length
    end
    if dot <= -0.999999 then
        return { 0.0, 0.0, 1.0, 0.0 }, length
    end
    -- Quaternion rotating +X onto direction: [1+dot, cross(+X,direction)].
    return NormalizeQuaternion({ 1.0 + dot, 0.0, -dz, dy }), length
end

local function WorldPointToPlayerLocal(x, y, z)
    local px, py, pz = Entity.GetWorldPosition(playerEntity)
    local rx, ry, rz = Entity.GetWorldRotation(playerEntity)
    local sx, sy, sz = Entity.GetWorldScale(playerEntity)
    local inverse = QuaternionConjugate(QuaternionFromEulerDegrees(
        rx or 0.0, ry or 0.0, rz or 0.0))
    local lx, ly, lz = RotateVectorByQuaternion(
        inverse,
        x - (px or 0.0),
        y - (py or 0.0),
        z - (pz or 0.0))
    return lx / math.max(math.abs(sx or 1.0), 0.000001),
        ly / math.max(math.abs(sy or 1.0), 0.000001),
        lz / math.max(math.abs(sz or 1.0), 0.000001)
end

local function WorldVectorToPlayerLocal(x, y, z)
    local rx, ry, rz = Entity.GetWorldRotation(playerEntity)
    local inverse = QuaternionConjugate(QuaternionFromEulerDegrees(
        rx or 0.0, ry or 0.0, rz or 0.0))
    return RotateVectorByQuaternion(inverse, x, y, z)
end

local function PlayerLocalPointToWorld(point)
    local px, py, pz = Entity.GetWorldPosition(playerEntity)
    local rx, ry, rz = Entity.GetWorldRotation(playerEntity)
    local sx, sy, sz = Entity.GetWorldScale(playerEntity)
    local wx, wy, wz = RotateVectorByQuaternion(
        QuaternionFromEulerDegrees(rx or 0.0, ry or 0.0, rz or 0.0),
        point[1] * (sx or 1.0),
        point[2] * (sy or 1.0),
        point[3] * (sz or 1.0))
    return (px or 0.0) + wx, (py or 0.0) + wy, (pz or 0.0) + wz
end

local function SetMeasurementSegment(key, a, b, color, thickness)
    local entity = MeasurementEntity(key, "box", color)
    if entity == 0 then return false end
    local dx, dy, dz = b[1] - a[1], b[2] - a[2], b[3] - a[3]
    local rotation, length = QuaternionFromPositiveX(dx, dy, dz)
    local rotationX, rotationY, rotationZ = EulerDegreesFromQuaternion(rotation)
    Entity.SetLocalPosition(
        entity,
        (a[1] + b[1]) * 0.5,
        (a[2] + b[2]) * 0.5,
        (a[3] + b[3]) * 0.5)
    Entity.SetLocalRotation(entity, rotationX, rotationY, rotationZ)
    Entity.SetLocalScale(entity, length, thickness, thickness)
    Entity.SetDebugVisible(entity, vehicleMeasurementOverlay.enabled)
    return true
end

local function SetMeasurementMarker(key, point, color, scale)
    local entity = MeasurementEntity(key, "sphere", color)
    if entity == 0 then return false end
    Entity.SetLocalPosition(entity, point[1], point[2], point[3])
    Entity.SetLocalScale(entity, scale, scale, scale)
    Entity.SetDebugVisible(entity, vehicleMeasurementOverlay.enabled)
    return true
end

local function SetMeasurementLabel(key, point, text, color)
    vehicleMeasurementOverlay.labels[key] = {
        point = { point[1], point[2], point[3] },
        text = text,
        color = { color[1], color[2], color[3] }
    }
end

local function CurrentLocalWheelCentres()
    if #vehicleWheelTelemetry < 4 then return nil end
    local centres = {}
    for index = 1, 4 do
        local wheel = vehicleWheelTelemetry[index]
        if wheel == nil then return nil end
        local x, y, z = WorldPointToPlayerLocal(
            wheel.centerX or 0.0,
            wheel.centerY or 0.0,
            wheel.centerZ or 0.0)
        centres[index] = { x, y, z }
    end
    return centres
end

local function AveragePoint(a, b)
    return {
        (a[1] + b[1]) * 0.5,
        (a[2] + b[2]) * 0.5,
        (a[3] + b[3]) * 0.5
    }
end


local function IsFiniteNumber(value)
    return type(value) == "number"
        and value == value
        and value > -math.huge
        and value < math.huge
end

local function ValidMeasurementBounds(bounds)
    return bounds ~= nil
        and IsFiniteNumber(bounds.minX)
        and IsFiniteNumber(bounds.minY)
        and IsFiniteNumber(bounds.minZ)
        and IsFiniteNumber(bounds.maxX)
        and IsFiniteNumber(bounds.maxY)
        and IsFiniteNumber(bounds.maxZ)
        and bounds.maxX > bounds.minX
        and bounds.maxY > bounds.minY
        and bounds.maxZ > bounds.minZ
end

local function CurrentVehicleBodyBounds(centres)
    -- Preferred authority: importer-measured visual-body bounds. These exclude
    -- semantic tire nodes, so the dimension bars clear the Peugeot body rather
    -- than merely clearing the wheel centres or the simplified box collider.
    local geometry = vehicleAssetMetadata ~= nil
        and vehicleAssetMetadata.ride_height_geometry or nil
    if geometry ~= nil and geometry.valid == true then
        local imported = {
            minX = geometry.body_min_x,
            minY = geometry.body_min_y,
            minZ = geometry.body_min_z,
            maxX = geometry.body_max_x,
            maxY = geometry.body_max_y,
            maxZ = geometry.body_max_z,
            source = "authored GLB body bounds"
        }
        if ValidMeasurementBounds(imported) then return imported end
    end

    -- Physics-body bounds are the robust fallback for non-GLB module cars.
    if nativeVehicleBody ~= nil and nativeVehicleBody ~= 0
        and Physics.BodyExists(nativeVehicleBody)
        and Physics.GetBodyCollisionBounds ~= nil then
        local collision = Physics.GetBodyCollisionBounds(nativeVehicleBody)
        if ValidMeasurementBounds(collision) then
            collision.source = "native collider bounds"
            return collision
        end
    end

    -- Last-resort definition fallback keeps the generic modular vehicle tool
    -- usable even before its authored asset metadata has finished loading.
    local chassis = PrototypeCarDefinition.chassis or {}
    local half = chassis.halfExtents or { 1.0, 0.5, 1.8 }
    local offset = chassis.colliderOffset or { 0.0, 0.0, 0.0 }
    local fallback = {
        minX = (offset[1] or 0.0) - (half[1] or 1.0),
        minY = (offset[2] or 0.0) - (half[2] or 0.5),
        minZ = (offset[3] or 0.0) - (half[3] or 1.8),
        maxX = (offset[1] or 0.0) + (half[1] or 1.0),
        maxY = (offset[2] or 0.0) + (half[2] or 0.5),
        maxZ = (offset[3] or 0.0) + (half[3] or 1.8),
        source = "vehicle-definition collider bounds"
    }
    if ValidMeasurementBounds(fallback) then return fallback end

    -- An intentionally conservative wheel-envelope fallback should be nearly
    -- unreachable, but prevents invalid creator data from threading lines
    -- through the vehicle.
    local minimumX, maximumX = centres[1][1], centres[1][1]
    local minimumY, maximumY = centres[1][2], centres[1][2]
    local minimumZ, maximumZ = centres[1][3], centres[1][3]
    for index = 2, 4 do
        minimumX = math.min(minimumX, centres[index][1])
        maximumX = math.max(maximumX, centres[index][1])
        minimumY = math.min(minimumY, centres[index][2])
        maximumY = math.max(maximumY, centres[index][2])
        minimumZ = math.min(minimumZ, centres[index][3])
        maximumZ = math.max(maximumZ, centres[index][3])
    end
    return {
        minX = minimumX - 0.45,
        minY = minimumY - 0.45,
        minZ = minimumZ - 0.55,
        maxX = maximumX + 0.45,
        maxY = maximumY + 0.80,
        maxZ = maximumZ + 0.55,
        source = "conservative wheel envelope"
    }
end

local function SetDimensionEndCaps(prefix, first, second, capAxis, color, thickness)
    local halfCap = 0.085
    local function CapPoint(point, sign)
        return {
            point[1] + capAxis[1] * halfCap * sign,
            point[2] + capAxis[2] * halfCap * sign,
            point[3] + capAxis[3] * halfCap * sign
        }
    end
    SetMeasurementSegment(
        prefix .. " Cap A",
        CapPoint(first, -1.0), CapPoint(first, 1.0),
        color, thickness)
    SetMeasurementSegment(
        prefix .. " Cap B",
        CapPoint(second, -1.0), CapPoint(second, 1.0),
        color, thickness)
end

function UpdateVehicleMeasurementOverlay()
    if vehicleMeasurementOverlay == nil or not vehicleMeasurementOverlay.enabled then
        return false
    end
    if playerEntity == 0 or not Entity.Exists(playerEntity) then
        vehicleMeasurementOverlay.message = "Measurement presentation is waiting for Player Vehicle Root"
        return false
    end

    local centres = CurrentLocalWheelCentres()
    if centres == nil then
        vehicleMeasurementOverlay.message = "Measurement presentation is waiting for four native wheels"
        return false
    end

    local thickness = vehicleMeasurementOverlay.lineThicknessM
    local front = AveragePoint(centres[1], centres[2])
    local rear = AveragePoint(centres[3], centres[4])
    local bounds = CurrentVehicleBodyBounds(centres)
    local clearance = vehicleMeasurementOverlay.bodyClearanceM or 0.30
    local frontDimensionZ = math.max(bounds.maxZ, front[3] + 0.40) + clearance
    local rearDimensionZ = math.min(bounds.minZ, rear[3] - 0.40) - clearance
    local frontY = (centres[1][2] + centres[2][2]) * 0.5
    local rearY = (centres[3][2] + centres[4][2]) * 0.5

    local frontLeft = { centres[1][1], frontY, frontDimensionZ }
    local frontRight = { centres[2][1], frontY, frontDimensionZ }
    local rearLeft = { centres[3][1], rearY, rearDimensionZ }
    local rearRight = { centres[4][1], rearY, rearDimensionZ }
    SetMeasurementSegment(
        "Front Track", frontLeft, frontRight,
        { 1.0, 1.0, 1.0 }, thickness)
    SetMeasurementSegment(
        "Rear Track", rearLeft, rearRight,
        { 1.0, 1.0, 1.0 }, thickness)
    SetDimensionEndCaps(
        "Front Track", frontLeft, frontRight,
        { 0.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, thickness * 0.80)
    SetDimensionEndCaps(
        "Rear Track", rearLeft, rearRight,
        { 0.0, 0.0, 1.0 }, { 1.0, 1.0, 1.0 }, thickness * 0.80)

    local maximumWheelX = math.max(
        centres[1][1], centres[2][1], centres[3][1], centres[4][1])
    local outsideX = math.max(bounds.maxX, maximumWheelX + 0.13) + clearance
    local wheelbaseFront = { outsideX, frontY, front[3] }
    local wheelbaseRear = { outsideX, rearY, rear[3] }
    SetMeasurementSegment(
        "Wheelbase", wheelbaseFront, wheelbaseRear,
        { 0.25, 0.90, 1.0 }, thickness * 1.15)
    SetDimensionEndCaps(
        "Wheelbase", wheelbaseFront, wheelbaseRear,
        { 1.0, 0.0, 0.0 }, { 0.25, 0.90, 1.0 }, thickness * 0.90)

    local linePoints = {
        frontLeft, frontRight, rearLeft, rearRight,
        wheelbaseFront, wheelbaseRear
    }
    for index, point in ipairs(linePoints) do
        SetMeasurementMarker(
            "Endpoint " .. tostring(index), point,
            { 1.0, 1.0, 1.0 }, thickness * 2.4)
    end


    local live = vehicleGeometry ~= nil and vehicleGeometry.live or nil
    local measuredWheelbaseM = live ~= nil and live.valid
        and live.wheelbaseM or math.abs(front[3] - rear[3])
    local measuredFrontTrackM = live ~= nil and live.valid
        and live.frontTrackM or math.abs(frontLeft[1] - frontRight[1])
    local measuredRearTrackM = live ~= nil and live.valid
        and live.rearTrackM or math.abs(rearLeft[1] - rearRight[1])
    SetMeasurementLabel(
        "Front Track",
        { (frontLeft[1] + frontRight[1]) * 0.5, frontY + 0.11, frontDimensionZ },
        string.format("%.1f mm", measuredFrontTrackM * 1000.0),
        { 1.0, 1.0, 1.0 })
    SetMeasurementLabel(
        "Rear Track",
        { (rearLeft[1] + rearRight[1]) * 0.5, rearY + 0.11, rearDimensionZ },
        string.format("%.1f mm", measuredRearTrackM * 1000.0),
        { 1.0, 1.0, 1.0 })
    SetMeasurementLabel(
        "Wheelbase",
        {
            outsideX,
            (frontY + rearY) * 0.5 + 0.11,
            (front[3] + rear[3]) * 0.5
        },
        string.format("%.1f mm", measuredWheelbaseM * 1000.0),
        { 0.25, 0.90, 1.0 })

    for index = 1, 4 do
        local wheel = vehicleWheelTelemetry[index]
        local axisX, axisY, axisZ = WorldVectorToPlayerLocal(
            wheel.wheelRightX or 1.0,
            wheel.wheelRightY or 0.0,
            wheel.wheelRightZ or 0.0)
        local axisLength = math.sqrt(axisX * axisX + axisY * axisY + axisZ * axisZ)
        if axisLength <= 0.000001 then axisLength = 1.0 end
        axisX, axisY, axisZ = axisX / axisLength, axisY / axisLength, axisZ / axisLength
        local corner = vehicleFitment ~= nil and vehicleFitment.corners[index] or nil
        local widthM = ((corner ~= nil and corner.tireWidthMm) or 205.0) * 0.001
        local isFront = index <= 2
        local tireClearance = vehicleMeasurementOverlay.tireWidthClearanceM or 0.18
        local widthZ = isFront
            and (frontDimensionZ + tireClearance)
            or (rearDimensionZ - tireClearance)
        local centre = {
            centres[index][1],
            isFront and frontY or rearY,
            widthZ
        }
        local half = widthM * 0.5
        local widthFirst = {
            centre[1] - axisX * half,
            centre[2] - axisY * half,
            centre[3] - axisZ * half
        }
        local widthSecond = {
            centre[1] + axisX * half,
            centre[2] + axisY * half,
            centre[3] + axisZ * half
        }
        SetMeasurementSegment(
            "Tire Width " .. tostring(index),
            widthFirst,
            widthSecond,
            { 1.0, 0.72, 0.20 }, thickness * 1.20)
        SetDimensionEndCaps(
            "Tire Width " .. tostring(index),
            widthFirst, widthSecond,
            { 0.0, 0.0, 1.0 },
            { 1.0, 0.72, 0.20 }, thickness * 0.75)
        SetMeasurementLabel(
            "Tire Width " .. tostring(index),
            { centre[1], centre[2] + 0.10, centre[3] },
            string.format("%.1f mm", widthM * 1000.0),
            { 1.0, 0.72, 0.20 })
    end

    vehicleMeasurementOverlay.message =
        "Outside " .. tostring(bounds.source)
        .. ": cyan wheelbase, white axle tracks, amber tire widths"
    return true
end

function SetVehicleMeasurementOverlayEnabled(enabled)
    vehicleMeasurementOverlay.enabled = enabled == true
    Save.SetBool("vehicle.visual.measurement_overlay", vehicleMeasurementOverlay.enabled)
    if vehicleMeasurementOverlay.enabled then
        UpdateVehicleMeasurementOverlay()
    else
        DestroyVehicleMeasurementOverlay()
        vehicleMeasurementOverlay.message = "Live vehicle measurement presentation is hidden"
    end
end

function DrawVehicleMeasurementHud()
    if not vehicleMeasurementOverlay.enabled then return end
    if UI.BeginPanel("RacingUnitedVehicleMeasurements", 390, 205) then
        UI.TextDisabled("LIVE VEHICLE GEOMETRY")
        UI.TextDisabled("Tab -> VEHICLE -> VISUAL -> WHEELS to configure")
        UI.Separator()
        local live = vehicleGeometry ~= nil and vehicleGeometry.live or nil
        if live ~= nil and live.valid then
            UI.Text(string.format("Wheelbase       %7.1f mm", live.wheelbaseM * 1000.0))
            UI.Text(string.format("Front track     %7.1f mm", live.frontTrackM * 1000.0))
            UI.Text(string.format("Rear track      %7.1f mm", live.rearTrackM * 1000.0))
        else
            UI.TextDisabled("Waiting for native wheel-centre measurement")
        end
        local width = vehicleFitment ~= nil and vehicleFitment.corners[1]
            and vehicleFitment.corners[1].tireWidthMm or 205.0
        UI.Text(string.format("Tire width      %7.1f mm", width))
        if #vehicleWheelTelemetry >= 4 then
            UI.Text(string.format(
                "Camber FL/FR  %+5.2f / %+5.2f deg",
                vehicleWheelTelemetry[1].workshopCamberDegrees or 0.0,
                vehicleWheelTelemetry[2].workshopCamberDegrees or 0.0))
            UI.Text(string.format(
                "Camber RL/RR  %+5.2f / %+5.2f deg",
                vehicleWheelTelemetry[3].workshopCamberDegrees or 0.0,
                vehicleWheelTelemetry[4].workshopCamberDegrees or 0.0))
        end
    end
    UI.EndPanel()
end

function DrawVehicleMeasurementWorldLabels()
    if vehicleMeasurementOverlay == nil
        or not vehicleMeasurementOverlay.enabled
        or UI.WorldLabel == nil
        or playerEntity == 0
        or not Entity.Exists(playerEntity) then
        return
    end

    for _, label in pairs(vehicleMeasurementOverlay.labels or {}) do
        local x, y, z = PlayerLocalPointToWorld(label.point)
        UI.WorldLabel(
            label.text,
            x, y, z,
            label.color[1], label.color[2], label.color[3],
            1.0,
            7.0)
    end
end
