# PERF19B — Wet-Film Texture-State Hotfix

PERF19's isolated wet-film pass was correct in concept but used texture units 0, 1 and 2 for its hydrology atlas, breakup mask and environment cubemap. The root entity renderer caches material bindings on texture units 0–8 and therefore could believe textures remained bound after the wet-film pass had replaced and unbound them. Subsequent scene draws then skipped required material rebinds and rendered grey/textureless.

PERF19B moves the wet-film pass to dedicated units 12, 13 and 14. Units 0–11 remain untouched by the overlay (materials 0–8, environment 9, shadow arrays 10–11), so the root renderer's binding cache stays truthful. The overlay restores only the active texture-unit selector and the normal entity program after drawing.

No hydrology cadence, wet-film appearance, puddle threshold, rain density, or connected-water behavior changes in this hotfix.
