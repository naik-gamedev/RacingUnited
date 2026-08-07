-- Racing United vehicle definition: Step 29J.4A Peugeot-oriented prototype.
-- This file is data only. Native C++ systems perform the actual simulation.
-- The visual/reference geometry below uses published 2003 Peugeot 206 RC
-- dimensions so the imported player body has sensible wheel placement now.
-- This is still NOT the final measured Peugeot physics definition.
PrototypeCarDefinition = {
    schemaVersion = 2,
    id = "step29j4a_prototype_car",
    displayName = "Step 29J.4A Peugeot-Oriented Prototype",
    classification = "car",

    -- This compatibility summary lets the current handwritten prototype live
    -- beside the complete topology-first VehicleDefinitionV2 contract. The
    -- Workshop exports the full bodies/power/transmission/contact graph.
    architecture = {
        bodyCount = 1,
        powerUnitCount = 1,
        transmissionCount = 1,
        contactUnitCount = 4
    },

    referenceGeometry = {
        name = "2003 Peugeot 206 RC",
        wheelbaseM = 2.442,
        frontTrackM = 1.437,
        rearTrackM = 1.428,
        tireSize = "205/40 ZR17",
        rimSize = "17x7J",
        tireWidthM = 0.205,
        wheelRadiusM = 0.2979
    },

    -- Visual presentation is deliberately separate from the physics chassis.
    -- Replace Assets/Vehicles/Player/PlayerCar.obj with your own authored OBJ.
    -- The renderer watches the file timestamp and reloads it while the engine runs.
    visual = {
        bodyAsset = "Vehicles/Player/PlayerCar.obj",
        fallbackBodyAsset = "Vehicles/Step27E_LowPolyHatchback.obj",
        normalize = false,
        doubleSided = false,
        color = { 0.10, 0.42, 0.92 },
        -- Creator-authored vehicle geometry is authoritative at 1:1 scale.
        -- The temporary OBJ body slot is imported through the same Blender
        -- authoring-axis conversion as the Player Scene. No automatic visual
        -- translation/scaling is applied. Racing United vehicle assets use
        -- Blender X=right, Y=fore/aft, Z=up with the vehicle nose pointing -Y
        -- (matching Blender Front-view vehicle authoring). The native solver
        -- drives toward +Z, so the temporary OBJ bridge applies one fixed
        -- 180-degree yaw at attachment; this is format convention, not tuning.
        offset = { 0.0, 0.0, 0.0 },
        rotationDegrees = { 0.0, 180.0, 0.0 },
        scale = 1.0,
        hideProxyWheels = true,

        articulatedWheels = {
            enabled = false,
            defaultAsset = "Vehicles/Player/PlayerWheel.obj",
            normalize = false,
            doubleSided = false,
            color = { 0.055, 0.060, 0.070 },

            -- Step 29J.2 rule: authored wheel dimensions are authoritative.
            -- Heritage Engine no longer resizes creator wheel geometry by default.
            radiusScale = 1.0,
            widthScale = 1.0,
            rotationOffsetDegrees = { 0.0, 0.0, 0.0 }
        }
    },

    chassis = {
        massKg = 1100.0,
        halfExtents = { 1.08, 0.36, 1.72 },
        colliderOffset = { 0.0, 0.82, 0.0 },
        friction = 0.35,
        restitution = 0.05,
        linearDamping = 0.015,
        angularDamping = 0.18
    },

    solver = {
        highRateHertz = 1000.0,
        maximumDriveForce = 7000.0,
        maximumBrakeForce = 12000.0,
        tireFriction = 1.15,
        lateralStiffness = 11000.0,
        rollingResistance = 90.0
    },

    steering = {
        maximumAngleDegrees = 38.0,
        ackermannPercent = 1.0,
        rateDegreesPerSecond = 260.0,
        returnRateDegreesPerSecond = 360.0,
        highSpeedRateFactor = 0.35,
        highSpeedReferenceMps = 40.0
    },

    -- Shared temporary suspension parameters. The mount coordinates themselves
    -- are per corner and already use the Peugeot reference wheelbase/tracks.
    wheelPhysics = {
        radiusM = 0.2979,
        restLengthM = 0.55,
        maximumCompressionM = 0.20,
        maximumDroopM = 0.15,
        springPreloadN = 0.0,
        springRateNPerM = 35000.0,
        springProgressionNPerM2 = 15000.0,
        bumpDampingNsPerM = 3200.0,
        bumpHighSpeedDampingNsPerM = 1800.0,
        bumpDampingKneeVelocityMps = 0.25,
        reboundDampingNsPerM = 4200.0,
        reboundHighSpeedDampingNsPerM = 2600.0,
        reboundDampingKneeVelocityMps = 0.30,
        bumpStopEngagementM = 0.15,
        bumpStopRateNPerM = 120000.0,
        bumpStopProgressionNPerM2 = 1000000.0,
        droopStopEngagementM = 0.1275,
        droopStopRateNPerM = 35000.0,
        motionRatio = 1.0,
        maximumForceN = 250000.0,
        effectiveUnsprungMassKg = 38.0,
        tireRadialStiffnessNPerM = 220000.0,
        tireRadialDampingNsPerM = 1800.0,
        maximumTireDeflectionM = 0.08,
        maximumTireNormalForceN = 250000.0
    },

    -- Vehicle-wide default. Each wheel below can override this with a named
    -- preset; the native solver owns a separate TireModelDescription per wheel.
    tire = TirePresets.prototype_road_front,

    powertrain = {
        idleRpm = 900.0,
        redlineRpm = 7000.0,
        maximumEngineTorque = 250.0,
        engineBrakingTorque = 70.0,
        finalDriveRatio = 3.90,
        efficiency = 0.88,
        shiftDuration = 0.22,
        clutchEngagementRate = 5.0,
        reverseRatio = -3.20,
        forwardRatios = { 3.40, 2.10, 1.45, 1.12, 0.89, 0.74 }
    },

    differential = {
        mode = 1,
        torqueBias = 2.25
    },

    driverAids = {
        absEnabled = true,
        tractionControlEnabled = true,
        absTargetSlip = 0.16,
        tractionTargetSlip = 0.12,
        minimumSpeed = 2.5,
        modulationRate = 18.0,
        maximumHandbrakeTorque = 3500.0,
        frontBrakeBias = 0.62
    },

    -- Convention for the temporary OBJ wheel slot before 29K:
    -- Racing United authored content uses Blender axes (X right, Y forward, Z up)
    -- and Blender's default OBJ export is converted explicitly by the importer.
    --   wheel origin = exact wheel center, local X = axle
    --   wheel outer/visible face points toward local +X
    -- Left-side meshes are therefore turned 180 deg around Y and their visual
    -- spin sign is inverted so all four still roll in the same world direction.
    -- Current Peugeot-oriented prototype layout: front-wheel drive and
    -- front-wheel steering. Rear-wheel steering remains fully supported by the
    -- generic solver simply by giving rear corners a non-zero steerFactor.
    wheels = {
        {
            name = "front_left",
            mount = { -0.7185, 0.85, 1.221 },
            driveFactor = 0.5,
            steerFactor = 1.0,
            axle = "front",
            tireProfile = "prototype_road_front",
            visualAsset = "Vehicles/Player/PlayerWheel.obj",
            visualFaceYawDegrees = 180.0,
            visualSpinSign = -1.0
        },
        {
            name = "front_right",
            mount = { 0.7185, 0.85, 1.221 },
            driveFactor = 0.5,
            steerFactor = 1.0,
            axle = "front",
            tireProfile = "prototype_road_front",
            visualAsset = "Vehicles/Player/PlayerWheel.obj",
            visualFaceYawDegrees = 0.0,
            visualSpinSign = 1.0
        },
        {
            name = "rear_left",
            mount = { -0.7140, 0.85, -1.221 },
            driveFactor = 0.0,
            steerFactor = 0.0,
            axle = "rear",
            tireProfile = "prototype_road_rear",
            visualAsset = "Vehicles/Player/PlayerWheel.obj",
            visualFaceYawDegrees = 180.0,
            visualSpinSign = -1.0
        },
        {
            name = "rear_right",
            mount = { 0.7140, 0.85, -1.221 },
            driveFactor = 0.0,
            steerFactor = 0.0,
            axle = "rear",
            tireProfile = "prototype_road_rear",
            visualAsset = "Vehicles/Player/PlayerWheel.obj",
            visualFaceYawDegrees = 0.0,
            visualSpinSign = 1.0
        }
    },

    resetPosition = { 0.0, 0.05, 0.0 }
}
