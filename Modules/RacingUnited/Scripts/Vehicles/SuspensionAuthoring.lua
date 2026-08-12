-- CLEAN08 compatibility load coordinator.
-- New suspension-authoring code belongs under Vehicles/Suspension/. Keep this
-- root path only so existing module entry points and older creator scripts do not
-- need a flag-day rename.

local function IncludeSuspensionAuthoring(relativePath)
    local ok, message = Script.Include(relativePath)
    if not ok then
        error(message or ("Could not include suspension authoring file: " .. relativePath), 0)
    end
end

IncludeSuspensionAuthoring("Vehicles/Suspension/HardpointSources.lua")
IncludeSuspensionAuthoring("Vehicles/Suspension/HardpointEstimation.lua")
IncludeSuspensionAuthoring("Vehicles/Suspension/SuspensionAuthoring.lua")
IncludeSuspensionAuthoring("Vehicles/Suspension/HardpointGizmos.lua")
