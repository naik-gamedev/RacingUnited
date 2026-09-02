-- Three-authority vehicle geometry diagnostics.
-- 1. Factory reference: published engineering dimensions.
-- 2. Authored asset: measurements reconstructed from GLB tire bounds.
-- 3. Live native: current authoritative suspension wheel centres.
-- Keeping these separate makes an incorrect model or setup visible instead of
-- silently rewriting the vehicle definition to agree with it.

vehicleGeometry = {
    reference = PrototypeCarDefinition.referenceGeometry,
    authored = nil,
    live = nil,
    message = "Live four-corner geometry is waiting for a spawned vehicle"
}

local function CopyGeometry(source)
    if source == nil or source.valid ~= true then return nil end
    return {
        valid = true,
        wheelbaseM = source.wheelbase_m or source.wheelbaseM or 0.0,
        frontTrackM = source.front_track_m or source.frontTrackM or 0.0,
        rearTrackM = source.rear_track_m or source.rearTrackM or 0.0,
        provenance = source.provenance or "unknown"
    }
end

function VehicleGeometryImportAssetGeometry(metadata)
    vehicleGeometry.authored = metadata ~= nil
        and CopyGeometry(metadata.wheel_geometry)
        or nil
end

-- Native camber/toe values are local upright rotations. Local X/Y signs mirror
-- on the left and right, while workshop convention uses the same sign on both
-- sides: negative camber always means top-in; positive toe always means toe-in.
function VehicleAlignmentToWorkshopConvention(wheelIndex, localCamber, localToe)
    local isLeft = wheelIndex == 1 or wheelIndex == 3
    local sideSign = isLeft and -1.0 or 1.0
    return -sideSign * (localCamber or 0.0),
        -sideSign * (localToe or 0.0)
end

function RefreshVehicleGeometryMeasurement()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle)
        or Vehicle.MeasureWheelGeometry == nil then
        vehicleGeometry.live = nil
        vehicleGeometry.message = "Live four-corner geometry is waiting for a spawned vehicle"
        return false
    end
    vehicleGeometry.live = CopyGeometry(
        Vehicle.MeasureWheelGeometry(nativeVehicle, 1, 2, 3, 4))
    if vehicleGeometry.live == nil then
        vehicleGeometry.message = "Native four-corner geometry measurement is unavailable"
        return false
    end
    vehicleGeometry.live.provenance = "authoritative_live_wheel_centres"
    vehicleGeometry.message = "Measured from native hub-centre planes (FL/FR/RL/RR)"
    return true
end
