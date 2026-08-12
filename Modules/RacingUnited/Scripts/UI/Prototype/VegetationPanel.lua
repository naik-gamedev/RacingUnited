-- VEG01 dormant vegetation/biome foundation. No vegetation assets are required.
-- Trees, shrubs and grass share one registry but may choose different runtime
-- representations as the renderer grows.
function DrawVegetationFoundationPanel()
    UI.TextDisabled("VEG01 - VEGETATION / BIOME FOUNDATION")
    UI.Separator()
    UI.Spacing()

    if not Vegetation.IsAvailable() then
        UI.Text("Vegetation system unavailable")
        return
    end

    local stats = Vegetation.GetStats()
    UI.Text("Registered species: " .. tostring(stats.species_count or 0))
    UI.Text("Stored instances: " .. tostring(stats.instance_count or 0))
    UI.Text("Occupied 64 m chunks: " .. tostring(stats.occupied_chunk_count or 0))
    UI.Text(string.format(
        "Chunk-local placement precision: %.3f mm",
        tonumber(stats.local_quantization_mm or 0)))
    UI.Text(string.format(
        "Packed placement storage: %.2f KiB",
        (tonumber(stats.packed_bytes or 0) or 0) / 1024.0))

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("ONE SYSTEM - DIFFERENT PLANT STRATEGIES")
    UI.TextWrapped("Trees: real trunk/meaningful branches + optional octahedral foliage clusters nearby; merged clusters at medium distance; optional whole-tree octahedral impostor farther away; forest/terrain HLOD later.")
    UI.Spacing()
    UI.TextWrapped("Shrubs: real stems only where useful; octahedral foliage clusters can dominate the near representation; whole-shrub impostors are natural farther away.")
    UI.Spacing()
    UI.TextWrapped("Grass: near grass remains very cheap blade/card geometry. Authored clumps (for example around 0.5 m across) may use octahedral clump impostors at medium distance. Far grass becomes coverage/terrain representation rather than millions of individual plants.")

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("AUTHORING CONTRACT - RESERVED FOR VEG02+")
    UI.Text("heritage.role = vegetation")
    UI.Text("heritage.vegetation_type = tree / shrub / grass / reed / flower / crop")
    UI.Text("heritage.species = <stable species id>")
    UI.Text("heritage.wind_profile = <profile id>")
    UI.Text("heritage.role = foliage_cluster")
    UI.Text("heritage.impostor_type = octahedral")
    UI.TextWrapped("Those GLB metadata names are now reserved as the stable direction. VEG01 does not require or render an octahedral asset yet; unsupported representations simply remain dormant until the baker/shader arrives.")

    UI.Spacing()
    UI.Separator()
    UI.TextDisabled("LOD POLICY IS SPECIES-SPECIFIC")
    UI.TextWrapped("The engine has safe defaults for tree, shrub, grass, reed, flower and crop species. Future species data can override every distance. No plant is forced to use octahedral impostors where geometry is cheaper.")
end
