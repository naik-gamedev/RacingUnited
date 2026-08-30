-- Vehicle-owned UI load coordinator.
-- Main.lua coordinates subsystems; this file owns the vehicle panel set.

local function IncludeVehiclePanel(relativePath)
    local ok, message = Script.Include(relativePath)
    if not ok then
        error(message or ("Could not include vehicle UI file: " .. relativePath), 0)
    end
end

IncludeVehiclePanel("UI/Vehicle/DrivePanel.lua")
IncludeVehiclePanel("UI/Vehicle/Visual/BodyPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Visual/WheelsPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Visual/AssetMetadataPanel.lua")
IncludeVehiclePanel("UI/Vehicle/VisualPanel.lua")
IncludeVehiclePanel("UI/Vehicle/SurfacesPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/ProfilesPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/BasicPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/CurvePanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/LivePanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/CarcassMegaLabPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Tires/LabPanel.lua")
IncludeVehiclePanel("UI/Vehicle/TiresPanel.lua")
IncludeVehiclePanel("UI/Vehicle/Suspension/AuthoringPanel.lua")
IncludeVehiclePanel("UI/Vehicle/SuspensionPanel.lua")
IncludeVehiclePanel("UI/Vehicle/FitmentPanel.lua")
IncludeVehiclePanel("UI/Vehicle/DrivetrainPanel.lua")
IncludeVehiclePanel("UI/Vehicle/DriverAidsPanel.lua")
IncludeVehiclePanel("UI/Vehicle/TelemetryPanel.lua")
IncludeVehiclePanel("UI/Vehicle/WorkshopPanel.lua")
IncludeVehiclePanel("UI/Vehicle/DynamicsLabPanel.lua")
IncludeVehiclePanel("UI/Vehicle/CameraLabPanel.lua")
IncludeVehiclePanel("UI/Vehicle/AudioLabPanel.lua")
IncludeVehiclePanel("UI/Vehicle/LabPanel.lua")
IncludeVehiclePanel("UI/VehicleDebugPanel.lua")
