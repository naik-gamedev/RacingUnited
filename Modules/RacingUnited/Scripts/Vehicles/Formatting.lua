-- Human-readable vehicle telemetry labels used by debug UI.
function VehicleGearName(gear)
    if gear == nil then return "?" end
    if gear < 0 then return "R" end
    if gear == 0 then return "N" end
    return tostring(gear)
end

function VehicleSurfaceName(surface)
    if surface == 1 then return "wet asphalt" end
    if surface == 2 then return "gravel" end
    if surface == 3 then return "dirt" end
    if surface == 4 then return "snow" end
    if surface == 5 then return "ice" end
    return "dry asphalt"
end

function VehicleDifferentialName(mode)
    if mode == 0 then return "open" end
    if mode == 2 then return "locked" end
    return "limited-slip"
end


function VehicleDetectedSurfaceLabel(telemetry)
    if telemetry == nil or not telemetry.grounded then
        return "air"
    end
    local name = telemetry.surfaceName or "default"
    local wetness = telemetry.surfaceWetness or 0.0
    if wetness > 0.005 then
        return string.format("%s %.0f%% wet", name, wetness * 100.0)
    end
    return name
end
