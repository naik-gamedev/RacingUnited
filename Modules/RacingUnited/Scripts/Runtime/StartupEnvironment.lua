-- Racing United fallback geographic/time context.
-- A loaded Scene_*.glb can override the geographic location through:
--   heritage.latitude_deg
--   heritage.longitude_deg
--   heritage.elevation_m
--   heritage.timezone
-- on the glTF scene extras or a Heritage_SceneMetadata Empty/node.
-- Ivarcko Jezero remains the module fallback while the current scene GLB is
-- being authored with those properties.
if Environment ~= nil then
    Environment.SetLocation(
        46.50619924,
        14.97089803,
        643.0,
        "Europe/Ljubljana")
    Environment.SetDate(2026, 8, 24)
end
