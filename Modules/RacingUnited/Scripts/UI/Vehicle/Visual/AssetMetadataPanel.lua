-- VA01: Human-readable view of semantic metadata authored as Blender Custom
-- Properties and exported into GLB node extras.

local function MetadataValue(properties, key, fallback)
    if properties == nil then return fallback end
    local value = properties[key]
    if value == nil then return fallback end
    return value
end

local function DrawWheelMetadata(part)
    local p = part.properties or {}
    UI.Text("Part ID: " .. tostring(part.part_id or ""))
    UI.Text("Model: " .. tostring(MetadataValue(p, "wheel.manufacturer", ""))
        .. " " .. tostring(MetadataValue(p, "wheel.model", "")))
    UI.Text(string.format("Size: %.1fx%.1f%s",
        tonumber(MetadataValue(p, "wheel.diameter_in", 0)) or 0,
        tonumber(MetadataValue(p, "wheel.width_in", 0)) or 0,
        tostring(MetadataValue(p, "wheel.flange_profile", ""))))
    UI.Text(string.format("PCD: %sx%s mm",
        tostring(MetadataValue(p, "wheel.pcd_mm", "?")),
        tostring(MetadataValue(p, "wheel.bolt_count", "?"))))
    UI.Text("Offset: ET" .. tostring(MetadataValue(p, "wheel.offset_et_mm", "?")))
    UI.Text("Center bore: " .. tostring(MetadataValue(p, "wheel.center_bore_mm", "?")) .. " mm")
    UI.Text("Construction: " .. tostring(MetadataValue(p, "wheel.construction", "?")))
    UI.Text("Hump profile: " .. tostring(MetadataValue(p, "wheel.hump_profile", "?")))
    UI.Text("Fastener seat: " .. tostring(MetadataValue(p, "wheel.fastener_seat", "?")))
    UI.Text("Mass estimate: " .. tostring(MetadataValue(p, "wheel.mass_kg_estimate", "?")) .. " kg")
end

local function DrawTireMetadata(part)
    local p = part.properties or {}
    UI.Text("Part ID: " .. tostring(part.part_id or ""))
    UI.Text("Model: " .. tostring(MetadataValue(p, "tire.manufacturer", ""))
        .. " " .. tostring(MetadataValue(p, "tire.model", "")))
    UI.Text("Marking: " .. tostring(MetadataValue(p, "tire.marking", "?")))
    UI.Text("Load index: " .. tostring(MetadataValue(p, "tire.load_index", "?"))
        .. " / " .. tostring(MetadataValue(p, "tire.max_load_kg", "?")) .. " kg")
    UI.Text("Speed rating: " .. tostring(MetadataValue(p, "tire.speed_rating", "?"))
        .. " / " .. tostring(MetadataValue(p, "tire.max_speed_kmh", "?")) .. " km/h")
    local diameter = tonumber(part.tire_nominal_diameter_m or 0) or 0
    if diameter > 0 then
        UI.Text(string.format("Nominal outside diameter: %.1f mm", diameter * 1000.0))
    end
end

local function DrawCorner(corner)
    local parts = vehicleAssetMetadata and vehicleAssetMetadata.parts or nil
    if parts == nil then return end
    local wheel = parts["WH_" .. corner]
    local tire = parts["WH_" .. corner .. "_Tire"]
    if wheel == nil and tire == nil then return end

    UI.Spacing()
    UI.TextDisabled(corner .. " ASSEMBLY")
    UI.Separator()
    if wheel ~= nil then
        UI.TextDisabled("WHEEL")
        DrawWheelMetadata(wheel)
    end
    if tire ~= nil then
        UI.Spacing()
        UI.TextDisabled("TIRE")
        DrawTireMetadata(tire)
    end
end

function DrawVehicleAssetMetadataPanel()
    UI.TextDisabled("GLB VEHICLE METADATA")
    UI.Separator()
    UI.Spacing()
    UI.TextWrapped("Blender Custom Properties exported as glTF extras are read by Heritage Engine. This is the first bridge for modular wheels, tires, brakes and later body customization parts.")
    UI.Text("Asset: " .. tostring(vehicleVisual.assetPath))

    if UI.Button("REFRESH GLB METADATA") then
        RefreshVehicleAssetMetadata()
    end

    UI.Spacing()
    if vehicleAssetMetadata == nil then
        UI.TextDisabled(vehicleAssetMetadataMessage)
        return
    end

    UI.Text("Semantic parts: " .. tostring(vehicleAssetMetadata.part_count or 0))
    UI.Text("Warnings: " .. tostring(vehicleAssetMetadata.warning_count or 0))
    UI.Spacing()
    UI.TextDisabled("VA02 EMBEDDED WHEEL BINDING")
    local embeddedEnabled, embeddedChanged = UI.Checkbox(
        "Bind WH_* GLB wheel hierarchy to native suspension",
        vehicleEmbeddedWheelBinding.enabled)
    if embeddedChanged then
        SetEmbeddedVehicleWheelBindingEnabled(embeddedEnabled)
    end
    UI.Text(string.format(
        "Complete Root+Pivot corners: %d/4",
        vehicleEmbeddedWheelBinding.detectedCorners or 0))
    UI.TextWrapped(tostring(vehicleEmbeddedWheelBinding.message or ""))
    if (vehicleAssetMetadata.warning_count or 0) > 0 then
        UI.TextWrapped(tostring(vehicleAssetMetadata.warning_summary or ""))
    end

    DrawCorner("FL")
    DrawCorner("FR")
    DrawCorner("RL")
    DrawCorner("RR")

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("VA02 binds embedded wheel roots/pivots to live native wheel telemetry. Runtime replacement GLBs and simulation consumption remain later bridges.")
    UI.TextDisabled(vehicleAssetMetadataMessage)
end
