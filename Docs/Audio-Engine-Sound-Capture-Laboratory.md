# AUDIO01 — Heritage Engine Sound Capture Laboratory

## Purpose

AUDIO01 treats Engine Simulator Community Edition as a virtual dyno/source generator and Heritage Engine as the recording/acoustic-authoring environment. The laboratory lives in **Vehicle → LAB → AUDIO**.

The design is deliberately non-destructive:

`Engine Simulator CE -> WASAPI loopback -> raw 32-bit-float WAV -> acoustic profile -> audition/render`

The raw source bank is never rewritten when a filter changes.

## Capture

Windows uses WASAPI shared-mode loopback on the default render endpoint. Heritage temporarily sets its own XAudio2 master to zero while capturing so its UI/vehicle/weather audio does not contaminate the Engine Simulator signal. Engine Simulator remains audible to the loopback endpoint because it is a separate process.

All accepted source captures are converted to interleaved stereo IEEE-float at 48 kHz and written as WAV.

AUDIO01 supplies a first Peugeot 206 RC / EW10J4S grid:

- warm idle: 850 rpm
- 1000–7000 rpm in 500-rpm steps
- 25 / 50 / 75 / 100% throttle at each step
- 53 steady-state cells total

Bank files and `capture_manifest.csv` are written beneath:

`UserData/Modules/RacingUnited/EngineSoundLab/Banks/<Vehicle>/<Engine>/`

## Acoustic shaping

The laboratory contains one shared, non-destructive source-character profile plus four audition perspectives:

- RAW — exact captured source (safety-clamped only)
- ENGINE BAY — source character plus controllable mechanical presence
- REAR / EXHAUST — source character plus muffling and exhaust-body resonance
- DRIVER CABIN — source character plus frequency-dependent cabin transmission, LF leakage and cabin resonance

The source chain includes high/low-pass filters, body resonance, removal of the characteristic raw/electric Engine-Simulator presence band, HF shelf, pulse-edge softening and bounded saturation.

The preview reflection control is intentionally a cheap audition effect. Production world reverberation remains owned by Heritage's shared acoustic/reverb buses; it is not baked per car.

Profiles are stored as `.hacoustic` key/value text beneath:

`UserData/Modules/RacingUnited/EngineSoundLab/Profiles/`

## Runtime boundary

AUDIO01 does not bake listener-dependent state into the RPM bank. Distance, Doppler, geometry occlusion, environmental reflections, cabin/window state and fleet audio LOD remain runtime responsibilities. Vehicle-intrinsic intake/exhaust/body coloration can be represented by the saved profile and later promoted into the production vehicle-audio definition.

## Peugeot authoring source

The final calibrated Community Edition `.mr` and its research notes are module authoring assets at:

`Modules/RacingUnited/Assets/Audio/Authoring/EngineSimulator/Peugeot206RC/`

The calibrated simulator result used to freeze the performance curve was approximately 173 hp @ 6790 rpm and 148 lb-ft @ 4798 rpm.
