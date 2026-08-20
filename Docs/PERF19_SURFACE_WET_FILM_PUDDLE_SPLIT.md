# PERF19 – Surface Wet-Film / Puddle Split

## Why
PERF16–PERF18 improved connected puddles but live testing still exposed two artifacts from using explicit geometry too early: forming water revealed polygon footprints, and shallow runoff along curbs/slopes could appear as detached ribbons.

## Architecture
- The 0.5 m hydrology grid remains authoritative for depth, flow, drainage, tire wetness and persistence.
- Shallow film/runoff is no longer represented by free-standing water geometry.
- A dedicated `kWetFilm*` shader re-renders only entities tagged `SurfaceWetnessReceiver` on their exact authored triangles.
- The wet-film shader samples a bounded 400x400 hydrology atlas (0.5 m cells => 200 m square / ±100 m per axis), preserving up to two vertical layers per X/Z texel for bridges/tunnels.
- The user-authored `Water_ShorelineBreakup_A8.png` modulates only the advancing wetness front in stable world space.
- The universal entity PBR shader remains hydrology-free; the wet-film path has its own program/state and can fail independently without corrupting scene rendering.
- Explicit connected water geometry now starts at 6 mm near, 8 mm at 50–100 m, and 12 mm at 100–200 m.
- Explicit geometry requires surface slope <=25 degrees. Shallow water moving faster than 0.75 m/s is classified as runoff and stays in the wet-film path.

## Expected visual result
Rain first changes the actual road/terrain material: darkening, smoothing and reflecting on the true mesh, with no quad silhouettes or detached slope ribbons. Only genuine accumulated puddles transition into the connected free-surface mesh.

## Performance
The wet-film overlay adds a second draw of tagged visible surface ranges, but uses the existing authored geometry and a single cheap atlas lookup path. Atlas refresh is bounded to <=10 Hz or camera recenter/topology changes; no per-fragment hydrology neighbourhood search is performed.
