-- AUTO-GENERATED COMPATIBILITY SEED.
-- Heritage Studio overwrites this file on Save All / race / road / gameplay publish.
StudioGameplay = {
    version = 18,
    race = {
        laps = 3,
        gridSlots = 24,
        pitSpeedKmh = 60.0,
        formationLap = false,
        standingStart = true,
        falseStartPenalty = true,
        trackLimitsEnabled = true,
        penaltiesEnabled = true,
        gridTemplate = "Staggered 2-wide",
        gridRowSpacingM = 6.0,
        gridLateralSpacingM = 3.0,
        gridBackOffsetM = 5.0
    },
    raceMarkers = {},
    broadcastCameraPaths = {}, broadcastCameraNodes = {},
    coneCourse = { enabled=true, defaultAssetPath="Assets/Props/TrafficCone.glb", minimumContactImpulseNs=1.0, defaultHitPenaltySeconds=2.0, defaultDisplacementToleranceM=0.12, wrongElementPenaltySeconds=10.0, missedElementDnf=true, resetEventConesOnStart=true, recordConeHitsToReplay=true, eventConesVisibleOnlyWhileActive=true },
    courseCones = {}, coneCourseGates = {},
    raceRoutes = {},
    raceRouteNodes = {},
    raceLayouts = {},
    raceSessions = {},
    raceControl = {},
    raceSupportPoints = {},
    trafficNodes = {},
    trafficLinks = {},
    roadSplines = {}, roadSplineNodes = {}, roadIntersections = {}, turnConnectors = {},
    trafficSignalPhases = {}, parkingStrips = {},
    trafficPopulation = {}, navigationBuild = {},
    trafficRules = {}, trafficStreaming = {}, intersectionControllers = {}, roadRestrictions = {},
    trafficAgentSimulation = { enabled=false, useHeritageVehicleDynamics=false, trafficVehicleHighRateHz=250.0 }, trafficAgentProfiles = {},
    trafficSpawnPortals = {}, trafficDensityRegions = {}, trafficIncidents = {},
    trafficEnvironment = {}, trafficBehavior = {}, trafficDebug = {},
    events = {},
    motorsport = { enabled=true, aiCompetitorsEnabled=true, autoBuildGrid=true, simulateUnspawnedCompetitors=true, maxPhysicalCompetitors=32, defaultAiSkill=0.80, qualifyingPaceSpreadPercent=4.0, baseMechanicalDnfChancePerHour=0.01, multiClassTiming=true, championshipPersistence=true },
    motorsportAi = { enabled=true, updateHz=20.0, lookaheadMinimumM=18.0, lookaheadMaximumM=120.0, brakingLookaheadM=180.0, opponentAwarenessM=100.0, slipstreamMinimumGapM=4.0, slipstreamMaximumGapM=32.0, overtakeMinimumClosingKmh=5.0, defensiveTriggerGapM=22.0, blueFlagYieldGapM=40.0, wetLineThreshold=0.20, maximumWetSpeedPenalty=0.24, fuelUseLitersPer100Km=35.0, tireWearPer100Km=0.18, fuelReserveLaps=1.25, tirePitThreshold=0.32, mistakeRecoverySeconds=2.0, strategyEnabled=true, mistakesEnabled=true, slipstreamEnabled=true, defendingEnabled=true, multiclassNegotiation=true, wetLineEnabled=true, liveDecisionTelemetry=true, fullPhysicsCompetitors=false, physicsHighRateHz=500.0, steeringLookaheadSeconds=0.70, steeringGain=1.10, crossTrackGain=0.22, throttleGain=0.11, brakeGain=0.16, maximumSteerAngleDeg=38.0, sideBySideSafetyM=1.25, trackLimitSafetyM=0.65, gripSlipRatioLimit=0.18, gripSlipAngleDeg=9.0, physicalRecoveryDistanceM=28.0, formationSpeedKmh=80.0, rollingStartSpeedKmh=90.0, pitLaneSpeedKmh=60.0, damagePitThreshold=0.62, damageDnfThreshold=0.16, collisionDamageScale=0.10, weatherForecastSeconds=60.0, gripAwareBraking=true, spatialAvoidance=true, trackLimitAwarePassing=true, damageStrategyEnabled=true, weatherForecastEnabled=true, colliderBoundsAuthority=true, collisionEnvelopeMarginM=0.18, sweptEnvelopeSeconds=0.70, sideBySideOverlapToleranceM=0.10, divebombCommitGapM=12.0, divebombClosingThresholdKmh=18.0, switchbackWindowS=1.50, maximumDefensiveMovesPerStraight=1, blockingPenaltySeconds=5.0, unsafeReleasePenaltySeconds=7.0, pitReleaseLookaheadM=55.0, multiclassPassHorizonS=2.50, tireOptimalMinimumC=75.0, tireOptimalMaximumC=105.0, fuelDensityKgPerLiter=0.745, predictiveCollisionAvoidance=true, divebombJudgement=true, blockingRules=true, unsafeReleaseStewarding=true, tireThermalStrategy=true, fuelMassAwareness=true, componentDamageStrategy=true },
    motorsportReplay = { enabled=true, sampleHz=12.0, preRollSeconds=8.0, postRollSeconds=5.0, maximumIncidentClips=12, maximumRecordedCompetitors=32, capturePlayer=true, captureControls=true, ghostReviewEnabled=true, maximumGhostVehicles=16, broadcastDirectorEnabled=true, autoIncidentCamera=true, incidentCameraDistanceM=13.0, incidentCameraHeightM=4.5, tracksideCameraLeadM=22.0, helicopterCameraHeightM=28.0, cameraSmoothing=9.0 },
    motorsportClasses = {}, motorsportEntrants = {}, motorsportChampionships = {}, motorsportRounds = {},
    eventExecution = { enabled=true, autoStagePlayer=true, autoSavePersonalBests=true, gridSettleSeconds=1.5, countdownSeconds=3.0, falseStartSpeedKmh=1.0, gateDebounceSeconds=0.35, trackLimitGraceSeconds=1.0, trackLimitRejoinSeconds=0.5, fullCourseYellowSpeedKmh=80.0, virtualSafetyCarSpeedKmh=80.0, safetyCarSpeedKmh=100.0, resultsHoldSeconds=12.0, practiceLoopEnabled=true, practiceLoopAutoRestart=true, practiceLoopRestoreAngularVelocity=true, practiceLoopRestoreGear=true, practiceLoopEndGateWidthM=12.0, practiceLoopRestoreDelayS=0.15 },
    worldPoints = {},
    policeGameplay = { enabled=false, maxHeatLevel=5, maxPursuitUnits=12, civilianWitnessRadiusM=120.0, policeDetectionRadiusM=180.0, speedToleranceKmh=12.0, heatDecayDelayS=20.0, heatDecayPerSecond=0.035, lostSightSeconds=12.0, searchDurationS=90.0, cooldownDurationS=30.0, bustHoldSeconds=5.0, backupDelayS=4.0, roadblockMinimumHeat=3.0, civilianWitnesses=true, speedingGeneratesHeat=true, collisionsGenerateHeat=true, illegalRacesGenerateHeat=true, evasionEscalatesHeat=true },
    policePatrolZones = {}, policeRoadblockSites = {}, policeEscapeZones = {}, clandestineMeets = {}
}

if RacingGameplay ~= nil then RacingGameplay.data = StudioGameplay end
