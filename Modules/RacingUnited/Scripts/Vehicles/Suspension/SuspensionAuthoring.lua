-- CLEAN08: suspension-authoring facade and native-geometry activation.
-- Source acquisition, estimation and creator gizmos are owned by sibling files.

local I = SuspensionAuthoringInternal
local SuspensionAuthoringHardpointPosition = I.HardpointPosition
local SuspensionAuthoringAssembly = I.Assembly
local SuspensionAuthoringRefreshRuntimeProvider = I.RefreshRuntimeProvider
local SuspensionAuthoringWheelDefinition = I.WheelDefinition

local function SuspensionAuthoringNativeHardpointMap(assembly, wheel)
    local result = {}
    local corner = assembly and assembly.hardpointsByCorner
        and assembly.hardpointsByCorner[wheel.name] or {}
    for _, id in ipairs(assembly and assembly.requiredHardpoints or {}) do
        local position = SuspensionAuthoringHardpointPosition(corner[id])
        if position ~= nil then
            result[id] = {
                x = position[1],
                y = position[2],
                z = position[3]
            }
        end
    end
    return result
end

function ApplySuspensionAuthoringGeometryToNativeVehicle()
    if nativeVehicle == 0 or not Vehicle.Exists(nativeVehicle) then
        return false
    end
    if Vehicle.SetWheelSuspensionHardpoints == nil then
        vehicleSuspensionAuthoring.message =
            "Native suspension-hardpoint setter is unavailable"
        return false
    end

    local active = 0
    for index, wheel in ipairs(PrototypeCarDefinition.wheels or {}) do
        local assembly = SuspensionAuthoringAssembly(wheel)
        if assembly ~= nil then
            local ready = SuspensionAuthoringRefreshRuntimeProvider(assembly, wheel)
            local provider = tostring(assembly.runtimeProvider or "")
            local supported = provider == "macpherson_strut_v1"
                or provider == "trailing_arm_torsion_bar_v1"
            if ready and supported then
                local hardpoints = SuspensionAuthoringNativeHardpointMap(
                    assembly, wheel)
                if not Vehicle.SetWheelSuspensionHardpoints(
                    nativeVehicle, index, provider, hardpoints) then
                    vehicleSuspensionAuthoring.message =
                        "Could not activate " .. provider
                        .. " on " .. tostring(wheel.name)
                    return false
                end
                active = active + 1
            end
        end
    end
    vehicleSuspensionAuthoring.activeHardpointWheelCount = active
    if active > 0 then
        vehicleSuspensionAuthoring.message = string.format(
            "Authoritative hardpoint kinematics active on %d wheel(s)",
            active)
    elseif (vehicleSuspensionAuthoring.estimatedCount or 0) > 0 then
        vehicleSuspensionAuthoring.message =
            "Estimated hardpoints are authoring-only; linear suspension physics remains active until legacy/asset/measured hardpoints are complete"
    end
    return true
end


function SuspensionAuthoringOnVehicleCreated()
    ApplySuspensionAuthoringGeometryToNativeVehicle()
    if vehicleSuspensionAuthoring.enabled then
        RefreshSuspensionAuthoringGizmos()
    end
end

function SuspensionAuthoringSelectedAssembly()
    local wheel = SuspensionAuthoringWheelDefinition(vehicleSuspension.selectedWheel)
    if wheel == nil then
        return nil
    end
    return SuspensionAuthoringAssembly(wheel)
end

-- Populate missing front and rear geometry as soon as the authoring module is loaded.
-- Better GLB/measured data can replace these points later without schema churn.
EnsureSuspensionHardpointEstimates(false)
