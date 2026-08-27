# Vehicle sample-bank authoring

`OggToWav` decodes a legally acquired OGG/Vorbis source with the same
`stb_vorbis` decoder used by Heritage Engine. `ConditionVehicleSampleBank.py`
then extracts authored, phase-conditioned RPM bands from one consistent field
recording. These are offline tools; neither ships in nor runs during a race.

The current Peugeot prototype bank is deliberately identified as a proxy. Its
engine loops come from TheLittleCrow's CC0 2019 Mini Cooper S contact recording
(Freesound 669618). A documented mix of its two synchronized contact channels
retains the engine-block body while restoring the upper-order detail that was
almost absent from the right channel on its own. A modest periodic EQ removes
excess sub-200 Hz boom and restores clarity without changing engine order. Its
startup comes from sound_catcher99's CC0 Fiat Punto petrol recording
(Freesound 425158). It is real inline-four material, not a measured Peugeot
EW10J4S session.

The source recordings live in ignored `Build/AudioSourceCache/Freesound`.
After decoding them, regenerate the checked-in runtime bank with:

```powershell
Tools\Audio\BuildOggToWav.cmd
Build\AudioSourceCache\OggToWav.exe <input.ogg> <output.wav>
C:\Users\Naik\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe Tools\Audio\ConditionVehicleSampleBank.py Tools\Audio\Peugeot206RCProxyBank.json
```

For a final vehicle, record steady loaded and unloaded bands every 250-500 RPM
from intake, engine bay, exhaust and cabin microphones. Replace the proxy paths
in the module definition; no native-code change should be necessary.
