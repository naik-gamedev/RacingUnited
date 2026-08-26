# CAM08 — Detached Camera Cloud-Travel Gear

CAM07's detached free camera remains FP64 world-space and keeps the same rebindable WASD + E/Q navigation.

The `Camera Fast` action (default Left Shift) now acts as a high-speed travel gear **only while the camera is detached**:

- base: 8 m/s
- Shift: 400 m/s (50x)
- Ctrl: 2 m/s (0.25x)

The existing vehicle-local camera authoring mode deliberately remains 4x on Shift so precise camera placement is not destroyed by the new travel multiplier.

The 400 m/s detached speed is intended for large-map inspection and rapid access to the 1.2–3.2 km volumetric cloud shell.
