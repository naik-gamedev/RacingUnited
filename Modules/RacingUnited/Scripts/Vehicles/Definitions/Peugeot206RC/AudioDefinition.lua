-- First physically informed audio authoring definition for the Peugeot 206 RC.
-- It describes sources and engine topology; native code owns synthesis,
-- spatialization, cockpit filtering and race-field LOD. These gains are an
-- initial tuning baseline, not a claim of measured Peugeot sound pressure.
Peugeot206RCAudioDefinition = {
    id = "peugeot_206_rc_ew10j4s_v2",
    category = "car",
    cylinders = 4,
    cycleRevolutions = 2,
    firingOrder = { 1, 3, 4, 2 },
    idleRpm = 900.0,
    redlineRpm = 7300.0,
    referenceRpm = 1200.0,
    maximumTorqueNm = 202.0,

    -- Factory geometry/operating data establishes the engine orders. The
    -- pulse, resonance and transition values are an audible tuning baseline,
    -- not a substitute for a measured EW10J4S recording session.
    engineAcoustics = {
        displacementLiters = 1.997,
        compressionRatio = 11.0,
        exhaustPulseSharpness = 0.70,
        intakePulseSharpness = 0.54,
        exhaustHeaderImbalance = 0.02,
        intakeResonanceOrder = 3.25,
        mechanicalOrderGain = 0.18,
        combustionVariation = 0.018,

        -- The production engine has VVT and a dual-mode intake. This smooth
        -- acoustic transition makes the induction layer breathe more freely
        -- at high RPM without changing torque or any other physics value.
        variableIntakeTransitionRpm = 5100.0,
        variableIntakeTransitionWidthRpm = 900.0,
        variableIntakeGain = 0.35
    },

    gains = {
        exhaust = 0.78,
        intake = 0.54,
        mechanical = 0.18,
        transmission = 0.14,
        tires = 0.30,
        wind = 0.25,
        chassis = 0.20
    },

    -- A real CC0 inline-four contact recording now owns the audible engine
    -- character. It is a Mini Cooper S proxy, not a claim of measured EW10J4S
    -- audio. The quiet procedural bed preserves load response and fills states
    -- absent from the field recording until a dedicated Peugeot session exists.
    samples = {
        gain = 0.92,
        proceduralGain = 0.16,
        startup = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_start.wav",
        engineLoops = {
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_0900_rpm.wav", rpm = 900.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_1650_rpm.wav", rpm = 1650.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_3000_rpm.wav", rpm = 3000.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_3700_rpm.wav", rpm = 3700.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_4300_rpm.wav", rpm = 4300.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_5500_rpm.wav", rpm = 5500.0 },
            { path = "Audio/ThirdParty/Freesound/MiniCooperSContactBank/engine_7200_rpm.wav", rpm = 7200.0 }
        }
    },

    -- Generic CC0 development sounds prove the event pipeline. They are kept
    -- separate from continuous vehicle layers so measured vehicle-specific
    -- recordings can replace them without changing native code.
    events = {
        gain = 0.48,
        maximumVoices = 6,
        gearShift = {
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_light_000.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_light_001.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_light_002.ogg"
        },
        suspensionLight = {
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactSoft_medium_000.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactSoft_medium_001.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactSoft_medium_002.ogg"
        },
        suspensionHeavy = {
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_medium_000.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_medium_001.ogg",
            "Audio/ThirdParty/Kenney/ImpactSounds/Audio/impactMetal_medium_002.ogg"
        }
    },

    -- Heritage local vehicle axes: +X right, +Y up, +Z forward.
    emitters = {
        engine = { 0.0, 0.72, 0.86 },
        intake = { 0.34, 0.80, 0.98 },
        exhaust = { -0.18, 0.29, -1.72 },
        transmission = { -0.29, 0.50, 0.48 },
        chassis = { 0.0, 0.46, 0.0 }
    },

    cabinRadiusMeters = 1.35,
    fullDetailDistanceMeters = 50.0,
    reducedDetailDistanceMeters = 150.0,
    maximumDistanceMeters = 400.0
}
