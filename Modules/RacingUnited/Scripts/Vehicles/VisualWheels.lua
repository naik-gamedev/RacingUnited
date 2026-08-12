-- CLEAN08 compatibility load coordinator.
-- Articulated/embedded wheel presentation now lives under Vehicles/Visual/.

local function IncludeVehicleWheelVisual(relativePath)
    local ok, message = Script.Include(relativePath)
    if not ok then
        error(message or ("Could not include wheel visual file: " .. relativePath), 0)
    end
end

IncludeVehicleWheelVisual("Vehicles/Visual/TransformMath.lua")
IncludeVehicleWheelVisual("Vehicles/Visual/ArticulatedWheels.lua")
IncludeVehicleWheelVisual("Vehicles/Visual/EmbeddedWheelBinding.lua")
IncludeVehicleWheelVisual("Vehicles/Visual/VisualWheels.lua")
