-- Racing United vehicle definition: Step 29Q Peugeot-oriented prototype.
-- This file is data only. Native C++ systems perform the actual simulation.
-- The visual/reference geometry below uses published 2003 Peugeot 206 RC
-- dimensions so the imported player body has sensible wheel placement now.
-- This is still NOT the final measured Peugeot physics definition.
PrototypeCarDefinition = {
    schemaVersion = 2,
    id = "step29j4a_prototype_car",
    displayName = "Step 29Q Peugeot-Oriented Prototype",
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

    factoryAlignmentSpecification = Peugeot206RCAlignmentSpecification,
    referenceAlignment = Peugeot206RCReferenceAlignment,

    -- FITMENT01: the GLB is a neutral/reference assembly. Alignment and
    -- installed fitment are setup data layered on top of the reference wheel
    -- centers; neither is allowed to rewrite suspension hardpoints. The
    -- current 17x7 ET28 / 205-40R17 information is trusted metadata even
    -- though the provisional wheel/tire mesh shape will be replaced later.
    referenceFitment = {
        provenance = "asset_metadata_reference",
        rimDiameterIn = 17.0,
        rimWidthIn = 7.0,
        offsetEtMm = 28.0,
        tireWidthMm = 205.0,
        tireAspectRatio = 40.0,
        tireRimDiameterIn = 17.0
    },

    factorySetup = Peugeot206RCWorkshopAlignmentDefault,
    audio = Peugeot206RCAudioDefinition,

    -- Suspension estimates are chassis authoring data, NOT wheel-fitment data.
    -- These package scales stay fixed when the player installs a different rim,
    -- offset or tire size. Rebuild them only when the suspension reference itself
    -- is intentionally revised from better measurements/asset geometry.
    suspensionEstimation = {
        frontReferencePackageScaleM = 0.2979,
        rearReferencePackageScaleM = 0.2979
    },

    -- SUS03A authoring architecture. macpherson_strut_v1 is a real native
    -- provider. linear_raycast_v1 remains the safe data-file fallback, while
    -- SuspensionAuthoring.lua may promote each front corner to MacPherson using
    -- a deterministic low-confidence estimate. Measured or GLB-authored points
    -- always outrank and replace estimates; provenance is never hidden.
    -- SUS04: reusable anti-roll bars couple the left/right suspension on each
    -- axle. These are low-confidence project estimates, not claimed Peugeot
    -- factory rates. The native mechanism is torsional (Nm/rad) with explicit
    -- lever arms, so future measured bar geometry/rates can replace these data
    -- without changing MacPherson or trailing-arm code.
    antiRollBars = {
        front = {
            enabled = true,
            leftWheel = "front_left",
            rightWheel = "front_right",
            torsionalStiffnessNmPerRad = 520.0,
            torsionalDampingNmsPerRad = 18.0,
            leftLeverArmM = 0.20,
            rightLeverArmM = 0.20,
            leftLinkMotionRatio = 1.0,
            rightLinkMotionRatio = 1.0,
            maximumWheelForceN = 7000.0,
            provenance = "estimated",
            confidence = 0.20
        },
        rear = {
            enabled = true,
            leftWheel = "rear_left",
            rightWheel = "rear_right",
            torsionalStiffnessNmPerRad = 380.0,
            torsionalDampingNmsPerRad = 14.0,
            leftLeverArmM = 0.20,
            rightLeverArmM = 0.20,
            leftLinkMotionRatio = 1.0,
            rightLinkMotionRatio = 1.0,
            maximumWheelForceN = 6000.0,
            provenance = "estimated",
            confidence = 0.20
        }
    },

    suspensionArchitecture = {
        front = {
            kinematics = "macpherson_strut",
            preferredProvider = "macpherson_strut_v1",
            runtimeProvider = "linear_raycast_v1",
            spring = "coil_spring",
            damper = "strut_damper",
            antiRollGroup = "front",
            requiredHardpoints = {
                "strut_top_mount",
                "strut_upright_mount",
                "lower_arm_inner_front",
                "lower_arm_inner_rear",
                "lower_ball_joint",
                "tie_rod_inner",
                "tie_rod_outer",
                "wheel_center"
            },
            hardpointsByCorner = {
                front_left = {},
                front_right = {}
            }
        },
        rear = {
            kinematics = "trailing_arm",
            preferredProvider = "trailing_arm_torsion_bar_v1",
            runtimeProvider = "linear_raycast_v1",
            spring = "torsion_bar",
            damper = "separate_damper",
            antiRollGroup = "rear",
            requiredHardpoints = {
                "arm_pivot_inner",
                "arm_pivot_outer",
                "wheel_center",
                "damper_upper_mount",
                "damper_lower_mount"
            },
            hardpointsByCorner = {
                rear_left = {},
                rear_right = {}
            }
        }
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
        -- drives toward +Z. The legacy OBJ bridge needs one fixed 180-degree
        -- yaw at attachment; standard Blender -> glTF/GLB export already maps
        -- the -Y nose into Heritage +Z, so GLB visuals intentionally bypass
        -- this legacy yaw in Vehicles/Visuals.lua.
        offset = { 0.0, 0.0, 0.0 },
        rotationDegrees = { 0.0, 180.0, 0.0 },
        scale = 1.0,
        hideProxyWheels = true,

        articulatedWheels = {
            enabled = true,
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

        -- ROLL01: the prefab/root origin is an authoring datum close to road
        -- level, not the physical centre of mass. Keeping these separate lets
        -- tire/contact forces generate real pitch/roll torque without moving
        -- the authored mesh, wheel mounts or suspension hardpoints. This is a
        -- deliberately low-confidence compact-FWD-hatch estimate: ~60% front
        -- static distribution and ~0.52 m CG height. Better measured/AI/photo
        -- reconstructed data can replace it later without changing the solver.
        centerOfMassLocal = { 0.0, 0.52, 0.20 },
        centerOfMassProvenance = "estimated_compact_fwd_hatch_v1",
        centerOfMassConfidence = 0.20,

        -- MASS01: explicit rotational inertia is independent from the collision
        -- proxy. X/Y/Z are pitch/yaw/roll axes in Heritage local coordinates.
        -- These low-confidence values are generated by the native road-car
        -- mass-property estimator and may later be replaced by CAD/component
        -- reconstruction without changing the rigid-body solver.
        inertiaLocalKgM2 = { 1212.8886, 1511.3550, 564.3155 },
        frontStaticLoadFraction = 0.5819001,
        leftStaticLoadFraction = 0.50,
        massPropertiesProvenance = "estimated_mass_properties_road_car_v1",
        massPropertiesConfidence = 0.20,

        halfExtents = { 1.08, 0.36, 1.72 },
        colliderOffset = { 0.0, 0.82, 0.0 },
        friction = 0.35,
        restitution = 0.05,
        linearDamping = 0.015,
        angularDamping = 0.18
    },

    -- FLEX01: first-mode torsional chassis compliance. This is deliberately
    -- low-confidence estimated engineering data, not a claimed Peugeot factory
    -- torsional-rigidity measurement. The values come from Heritage's generic
    -- closed-unibody estimator using the known 2003-era mass/wheelbase/tracks
    -- and the current estimated COM height. Better evidence can replace this
    -- table without changing suspension or rigid-body code.
    chassisFlex = {
        enabled = true,
        provider = "chassis_torsional_mode_v1",
        mountBody = "chassis",
        construction = "closed_unibody",
        modelYear = 2003,
        torsionalRigidityNmPerDegree = 8700.0,
        torsionalDampingNmsPerRad = 11300.0,
        effectiveTorsionalInertiaKgM2 = 525.0,
        torsionAxisLocalY = 0.364,
        frontReferenceLocalZ = 1.221,
        rearReferenceLocalZ = -1.221,
        maximumTwistDegrees = 1.25,
        provenance = "estimated_chassis_flex_closed_unibody_v1",
        confidence = 0.18
    },

    solver = {
        highRateHertz = 1000.0,
        maximumDriveForce = 7000.0,
        maximumBrakeForce = 12000.0,
        tireFriction = 1.15,
        lateralStiffness = 11000.0,
        rollingResistance = 90.0
    },

    -- Native steering sign is -LEFT / +RIGHT. maximumAngleDegrees is the
    -- virtual steering-axle centre angle; ideal Ackermann can give the inside
    -- physical road wheel more lock and the outside wheel less.
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
        steeringAxis = { 0.0, 1.0, 0.0 },
        staticCamberDegrees = 0.0,
        camberGainDegreesPerM = 0.0,
        camberProgressionDegreesPerM2 = 0.0,
        staticToeDegrees = 0.0,
        toeGainDegreesPerM = 0.0,
        toeProgressionDegreesPerM2 = 0.0,
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
            steeringAxis = { 0.16845, 0.98406, -0.05698 },
            staticCamberDegrees = 0.0,
            staticToeDegrees = 0.0,
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
            steeringAxis = { -0.16845, 0.98406, -0.05698 },
            staticCamberDegrees = 0.0,
            staticToeDegrees = 0.0,
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
            steeringAxis = { 0.0, 1.0, 0.0 },
            staticCamberDegrees = 0.0,
            staticToeDegrees = 0.0,
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
            steeringAxis = { 0.0, 1.0, 0.0 },
            staticCamberDegrees = 0.0,
            staticToeDegrees = 0.0,
            visualAsset = "Vehicles/Player/PlayerWheel.obj",
            visualFaceYawDegrees = 0.0,
            visualSpinSign = 1.0
        }
    },

    resetPosition = { 0.0, 0.05, 0.0 }
}
