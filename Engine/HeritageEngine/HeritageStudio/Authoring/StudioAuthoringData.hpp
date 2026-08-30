#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace heritage::studio::authoring {

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

enum class SceneObjectType
{
    Empty,
    Mesh,
    PlayerSpawn,
    VehicleSpawn,
    AudioZone,
    WeatherZone,
    Trigger
};

struct SceneObject
{
    std::uint32_t id = 0;
    std::string name;
    SceneObjectType type = SceneObjectType::Empty;
    bool enabled = true;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{ 1.0f, 1.0f, 1.0f };
    std::string tag;
    std::string assetPath;
};

enum class RaceMarkerType
{
    StartFinish,
    Checkpoint,
    Sector,
    GridSlot,
    PitEntry,
    PitExit,
    PitSpeedLine,
    PitBox,
    TrackLimitLeft,
    TrackLimitRight,
    Recovery,
    ReplayCamera,
    AiLineNode,
    WetLineNode,
    TimingLoop,
    SpeedTrapStart,
    SpeedTrapFinish,
    SafetyCarLine,
    FormationLine
};

struct RaceMarker
{
    std::uint32_t id = 0;
    std::string name;
    RaceMarkerType type = RaceMarkerType::Checkpoint;
    Vec3 position{};
    float headingDeg = 0.0f;
    float radiusM = 4.0f;
    float gateWidthM = 12.0f;
    float gateHeightM = 4.0f;
    bool directionRequired = true;
    std::uint32_t layoutId = 0; // 0 = shared by every layout; nonzero scopes timing/grid/pit objects to one venue layout.
    int order = 0;
    int slot = -1;
    float speedLimitKmh = 0.0f;
};

enum class GridTemplate
{
    StaggeredTwoWide,
    TwoWide,
    ThreeWide,
    SingleFile,
    EnduranceAngled
};

struct RaceConfiguration
{
    int laps = 3;
    int gridSlots = 24;
    float pitSpeedKmh = 60.0f;
    bool formationLap = false;
    bool standingStart = true;
    bool falseStartPenalty = true;
    bool trackLimitsEnabled = true;
    bool penaltiesEnabled = true;
    GridTemplate gridTemplate = GridTemplate::StaggeredTwoWide;
    float gridRowSpacingM = 6.0f;
    float gridLateralSpacingM = 3.0f;
    float gridBackOffsetM = 5.0f;
};

enum class RaceRouteType
{
    MainCircuit,
    PitLane,
    SafetyCar,
    Formation,
    AlternateLayout,
    Sprint,
    Hillclimb,
    Drag
};

struct RaceRoute
{
    std::uint32_t id = 0;
    std::string name = "Main Route";
    RaceRouteType type = RaceRouteType::MainCircuit;
    bool enabled = true;
    bool closedLoop = true;
    bool reverseAllowed = false;
    float defaultLeftWidthM = 6.0f;
    float defaultRightWidthM = 6.0f;
};

struct RaceRouteNode
{
    std::uint32_t id = 0;
    std::uint32_t routeId = 0;
    int order = 0;
    Vec3 position{};
    Vec3 handleIn{};
    Vec3 handleOut{};
    bool automaticTangents = true;
    float leftWidthM = 6.0f;
    float rightWidthM = 6.0f;
    float targetSpeedKmh = 0.0f;
    float bankingDeg = 0.0f;
    bool overtakingPreferred = false;
};

struct RaceLayout
{
    std::uint32_t id = 0;
    std::string name = "Grand Prix";
    bool enabled = true;
    std::uint32_t routeId = 0;
    std::uint32_t pitRouteId = 0;
    std::uint32_t startFinishMarkerId = 0;
    int defaultLaps = 3;
    bool reverse = false;
    bool pitsEnabled = true;
};

enum class RaceSessionType
{
    Practice,
    Qualifying,
    Warmup,
    Race,
    TimeAttack,
    TestSession
};

enum class SessionGridSource
{
    EventOrder,
    PreviousSession,
    Qualifying,
    Championship,
    ReverseTopN
};

struct RaceSession
{
    std::uint32_t id = 0;
    std::string name = "Race";
    RaceSessionType type = RaceSessionType::Race;
    bool enabled = true;
    int order = 0;
    int durationMinutes = 0;
    int laps = 0;
    int mandatoryPitStops = 0;
    bool formationLap = false;
    bool rollingStart = false;
    bool weatherChangeAllowed = true;
    float startingFuelPercent = 100.0f;
    bool timedRace = false;
    bool timePlusOneLap = false;
    int maximumStintMinutes = 0;
    bool refuelingAllowed = true;
    bool tireChangesAllowed = true;
    bool mandatoryTireChange = false;
    float minimumPitServiceSeconds = 0.0f;
    float classificationPercent = 75.0f;
    SessionGridSource gridSource = SessionGridSource::EventOrder;
    int reverseTopN = 0;
};

struct RaceControlConfiguration
{
    bool localYellow = true;
    bool fullCourseYellow = true;
    bool virtualSafetyCar = true;
    bool safetyCar = true;
    bool redFlag = true;
    bool blueFlags = true;
    bool pitLaneOpenDuringSafetyCar = true;
    int maxTrackLimitWarnings = 3;
    int driveThroughAfterWarnings = 5;
    int pitWindowStartLap = 0;
    int pitWindowEndLap = 0;
    std::uint32_t safetyCarRouteId = 0;
    std::uint32_t restartMarkerId = 0;
};

enum class RaceSupportPointType
{
    MarshalPost,
    RecoveryVehicle,
    TowTruck,
    Medical,
    FireCrew,
    RaceControl,
    SafetyCarStandby,
    TimingEquipment
};

struct RaceSupportPoint
{
    std::uint32_t id = 0;
    std::string name = "Marshal Post";
    RaceSupportPointType type = RaceSupportPointType::MarshalPost;
    bool enabled = true;
    Vec3 position{};
    float headingDeg = 0.0f;
    float serviceRadiusM = 30.0f;
    int sector = 0;
};

enum class BroadcastCameraPathType
{
    Dolly,
    Crane,
    Cable,
    Drone
};

struct BroadcastCameraPath
{
    std::uint32_t id = 0;
    std::string name = "Broadcast Camera Move";
    BroadcastCameraPathType type = BroadcastCameraPathType::Dolly;
    bool enabled = true;
    std::uint32_t layoutId = 0;
    float activationRadiusM = 180.0f;
    float durationSeconds = 6.0f;
    float easing = 0.65f;
    bool reverse = false;
};

struct BroadcastCameraNode
{
    std::uint32_t id = 0;
    std::uint32_t pathId = 0;
    int order = 0;
    Vec3 position{};
};

// STUDIO28: traffic cones are first-class authored world/event props instead of
// decorative meshes.  Physical representation is deliberately separate from
// the invisible course gates so a knocked-over cone never destroys timing logic.
enum class ConeRole
{
    TrafficGuide,
    Boundary,
    GateLeft,
    GateRight,
    SlalomLeft,
    SlalomRight,
    Start,
    Finish,
    Turnaround,
    StopBox,
    Chicane,
    Pointer,
    NoGoBoundary,
    RoadClosure
};

enum class ConeTrafficMode
{
    None,
    Guide,
    Slow,
    CloseLane,
    CloseRoad
};

enum class ConePenaltyMode
{
    None,
    Contact,
    Displaced,
    KnockedDown
};

struct CourseCone
{
    std::uint32_t id = 0;
    std::string name = "Traffic Cone";
    bool enabled = true;
    ConeRole role = ConeRole::Boundary;
    std::uint32_t eventId = 0; // 0 = persistent free-roam/world cone.
    Vec3 position{};
    float headingDeg = 0.0f;
    std::string assetPath; // Empty uses ConeCourseConfiguration::defaultAssetPath.
    float visualScale = 1.0f;
    float baseRadiusM = 0.18f;
    float heightM = 0.70f;
    float massKg = 1.2f;
    float friction = 0.72f;
    float restitution = 0.04f;
    bool physical = true;
    ConePenaltyMode penaltyMode = ConePenaltyMode::Displaced;
    float hitPenaltySeconds = 2.0f;
    float displacementToleranceM = 0.12f;
    ConeTrafficMode trafficMode = ConeTrafficMode::None;
    std::uint32_t roadId = 0;
    std::uint32_t linkId = 0;
    int laneIndex = -1;
    float trafficSpeedLimitKmh = 20.0f;
    float routeCostMultiplier = 6.0f;
};

enum class ConeCourseGateType
{
    Gate,
    SlalomLeft,
    SlalomRight,
    TurnaroundLeft,
    TurnaroundRight,
    StopBox,
    Finish,
    CircleLeft,
    CircleRight
};

struct ConeCourseGate
{
    std::uint32_t id = 0;
    std::string name = "Course Gate";
    bool enabled = true;
    std::uint32_t eventId = 0;
    int order = 0;
    ConeCourseGateType type = ConeCourseGateType::Gate;
    Vec3 position{};
    float headingDeg = 0.0f;
    float widthM = 4.0f;
    float lengthM = 4.0f;
    bool directionRequired = true;
    float sideClearanceM = 0.35f;
    float stopSpeedKmh = 1.0f;
    float stopDwellS = 0.25f;
    float wrongElementPenaltySeconds = 10.0f;
    bool dnfOnMiss = true;
    std::uint32_t leftConeId = 0;
    std::uint32_t rightConeId = 0;
};

struct ConeCourseConfiguration
{
    bool enabled = true;
    std::string defaultAssetPath = "Assets/Props/TrafficCone.glb";
    float minimumContactImpulseNs = 1.0f;
    float defaultHitPenaltySeconds = 2.0f;
    float defaultDisplacementToleranceM = 0.12f;
    float wrongElementPenaltySeconds = 10.0f;
    bool missedElementDnf = true;
    bool resetEventConesOnStart = true;
    bool recordConeHitsToReplay = true;
    bool eventConesVisibleOnlyWhileActive = true;
};

enum class TrafficNodeType
{
    LaneNode,
    Intersection,
    Stop,
    Yield,
    TrafficLight,
    Parking,
    Spawn,
    Despawn,
    Destination
};

struct TrafficNode
{
    std::uint32_t id = 0;
    std::string name;
    TrafficNodeType type = TrafficNodeType::LaneNode;
    Vec3 position{};
    float headingDeg = 0.0f;
    float speedLimitKmh = 50.0f;
    int lanes = 1;
    int priority = 0;
    bool bidirectional = false;
    bool overtakingAllowed = true;
    float density = 1.0f;
    std::uint32_t roadId = 0;
    int laneIndex = 0;
    int laneDirection = 0;
    bool generated = false;
};

enum class TrafficLinkType
{
    Travel,
    LaneChange,
    Merge,
    JunctionTurn,
    ParkingAccess,
    SpawnAccess
};

struct TrafficLink
{
    std::uint32_t id = 0;
    std::uint32_t fromNodeId = 0;
    std::uint32_t toNodeId = 0;
    int lanes = 1;
    float speedLimitKmh = 50.0f;
    bool bidirectional = false;
    bool overtakingAllowed = true;
    float density = 1.0f;
    TrafficLinkType type = TrafficLinkType::Travel;
    float routeCostMultiplier = 1.0f;
    bool enabled = true;
    bool generated = false;
};


enum class RoadClass
{
    Motorway,
    Arterial,
    Collector,
    Local,
    Residential,
    Service,
    Mountain,
    Gravel,
    Dirt
};

struct RoadSpline
{
    std::uint32_t id = 0;
    std::string name = "Road";
    RoadClass roadClass = RoadClass::Local;
    bool enabled = true;
    bool oneWay = false;
    int lanesForward = 1;
    int lanesBackward = 1;
    float laneWidthM = 3.25f;
    float shoulderLeftM = 0.5f;
    float shoulderRightM = 0.5f;
    float medianWidthM = 0.0f;
    float speedLimitKmh = 50.0f;
    bool sidewalkLeft = false;
    bool sidewalkRight = false;
    bool parkingLeft = false;
    bool parkingRight = false;
    float trafficDensity = 1.0f;
    float spawnWeight = 1.0f;
};

struct RoadSplineNode
{
    std::uint32_t id = 0;
    std::uint32_t roadId = 0;
    int order = 0;
    Vec3 position{};
    Vec3 handleIn{};
    Vec3 handleOut{};
    bool automaticTangents = true;
    float widthScale = 1.0f;
    float bankingDeg = 0.0f;
};

enum class JunctionPriority
{
    PriorityRoad,
    Yield,
    Stop,
    Signalized,
    Roundabout,
    Uncontrolled
};

struct RoadIntersection
{
    std::uint32_t id = 0;
    std::string name = "Intersection";
    Vec3 position{};
    float radiusM = 12.0f;
    JunctionPriority priority = JunctionPriority::PriorityRoad;
    bool trafficLights = false;
    bool pedestrianCrossing = false;
    float approachSpeedKmh = 30.0f;
};

struct TurnConnector
{
    std::uint32_t id = 0;
    std::uint32_t intersectionId = 0;
    std::uint32_t fromRoadId = 0;
    std::uint32_t toRoadId = 0;
    int fromLane = 0;
    int toLane = 0;
    bool enabled = true;
    bool yield = false;
    bool uTurn = false;
    float speedLimitKmh = 25.0f;
    int conflictGroup = 0;
    float reservationSeconds = 2.5f;
};

struct TrafficSignalPhase
{
    std::uint32_t id = 0;
    std::uint32_t intersectionId = 0;
    int order = 0;
    std::string name = "Phase";
    float greenSeconds = 25.0f;
    float yellowSeconds = 3.0f;
    float allRedSeconds = 1.0f;
    std::string connectorIds;
};

struct ParkingStrip
{
    std::uint32_t id = 0;
    std::string name = "Parking Strip";
    std::uint32_t roadId = 0;
    Vec3 position{};
    float headingDeg = 0.0f;
    int spaces = 8;
    float spacingM = 6.0f;
    float angleDeg = 0.0f;
    bool rightSide = true;
    float occupancy = 0.55f;
};

struct TrafficPopulationConfiguration
{
    float globalDensity = 1.0f;
    float parkedDensity = 0.55f;
    float rushHourMultiplier = 1.35f;
    float nightMultiplier = 0.55f;
    float heavyVehicleShare = 0.08f;
    float motorcycleShare = 0.05f;
    float commercialShare = 0.12f;
    float emergencyShare = 0.002f;
    float laneChangeAggression = 0.5f;
    float speedVariance = 0.08f;
    int maxActiveVehicles = 180;
};

struct NavigationBuildConfiguration
{
    bool enabled = true;
    bool rebuildOnSave = true;
    float maxSlopeDeg = 18.0f;
    float minimumTurnRadiusM = 5.0f;
    float laneChangeLengthM = 22.0f;
    float junctionLookaheadM = 35.0f;
    float mergeLookaheadM = 55.0f;
};

enum class DrivingSide
{
    Right,
    Left
};

enum class SignalControlMode
{
    FixedTime,
    Actuated,
    Adaptive
};

struct TrafficRulesConfiguration
{
    DrivingSide drivingSide = DrivingSide::Right;
    bool keepToDrivingSide = true;
    bool allowTurnOnRed = false;
    bool emergencyCorridor = true;
    float desiredTimeGapS = 1.6f;
    float minimumGapM = 2.5f;
    float desiredAccelerationMps2 = 2.0f;
    float comfortableBrakingMps2 = 2.5f;
    float laneChangeCooldownS = 3.0f;
    float laneChangeMinimumGapM = 8.0f;
    float laneChangeRouteCost = 1.18f;
    float mergeRouteCost = 1.10f;
    float emergencyYieldRadiusM = 80.0f;
    float roundaboutYieldDistanceM = 20.0f;
};

struct TrafficStreamingConfiguration
{
    float fullSimulationRadiusM = 450.0f;
    float simplifiedSimulationRadiusM = 900.0f;
    float dormantPersistenceRadiusM = 2500.0f;
    float sectorSizeM = 250.0f;
    int maxSpawnsPerSecond = 8;
    int maxDespawnsPerSecond = 16;
    float despawnBehindDistanceM = 300.0f;
    bool retainDormantState = true;
    float dormantStateMinutes = 15.0f;
};

enum class TrafficAgentClass
{
    Compact,
    Sedan,
    Sport,
    Van,
    Truck,
    Motorcycle,
    Emergency
};

struct TrafficAgentProfile
{
    std::uint32_t id = 0;
    std::string name = "Everyday Driver";
    TrafficAgentClass vehicleClass = TrafficAgentClass::Sedan;
    bool enabled = true;
    std::string vehiclePreset = "PrototypeCar";
    float spawnWeight = 1.0f;
    float lengthM = 4.4f;
    float widthM = 1.82f;
    float maxSpeedFactor = 1.0f;
    float accelerationFactor = 1.0f;
    float brakingFactor = 1.0f;
    float desiredTimeGapS = 1.6f;
    float minimumGapM = 2.5f;
    float reactionTimeS = 0.45f;
    float laneChangeAggression = 0.5f;
    float courtesy = 0.6f;
    float speedCompliance = 0.92f;
    float illegalOvertakeChance = 0.0f;
    float parkingSkill = 0.8f;
};

struct TrafficAgentSimulationConfiguration
{
    bool enabled = false;
    bool createDebugProxyVehicles = true;
    bool useHeritageVehicleDynamics = false;
    bool enableLaneChanges = true;
    bool enableMerges = true;
    bool enableParking = true;
    int maxFullPhysicsAgents = 64;
    int routeLookaheadLinks = 10;
    float trafficVehicleHighRateHz = 250.0f;
    float fullSimulationHz = 30.0f;
    float simplifiedSimulationHz = 10.0f;
    float perceptionRangeM = 120.0f;
    float stopLineBufferM = 2.0f;
    float intersectionCreepSpeedKmh = 5.0f;
    float parkingApproachSpeedKmh = 10.0f;
    float spawnMinDistancePlayerM = 100.0f;
    float spawnMaxDistancePlayerM = 650.0f;
    float minimumSpawnGapM = 25.0f;
    float stuckTimeoutS = 20.0f;
    float despawnGraceS = 5.0f;
};

enum class TrafficPortalMode
{
    SpawnAndDespawn,
    SpawnOnly,
    DespawnOnly
};

struct TrafficSpawnPortal
{
    std::uint32_t id = 0;
    std::string name = "Traffic Portal";
    bool enabled = true;
    std::uint32_t nodeId = 0;
    TrafficPortalMode mode = TrafficPortalMode::SpawnAndDespawn;
    Vec3 position{};
    float headingDeg = 0.0f;
    float radiusM = 18.0f;
    float spawnWeight = 1.0f;
    int maxConcurrentAgents = 24;
    float startHour = 0.0f;
    float endHour = 24.0f;
    float minimumPlayerDistanceM = 80.0f;
    float maximumPlayerDistanceM = 900.0f;
    bool emergencyAllowed = true;
    std::string allowedClasses;
};

struct TrafficDensityRegion
{
    std::uint32_t id = 0;
    std::string name = "Traffic Density Region";
    bool enabled = true;
    Vec3 position{};
    float radiusM = 250.0f;
    float densityMultiplier = 1.0f;
    float speedMultiplier = 1.0f;
    float laneChangeAggressionOffset = 0.0f;
    float parkingMultiplier = 1.0f;
    float startHour = 0.0f;
    float endHour = 24.0f;
};

enum class TrafficIncidentType
{
    Breakdown,
    Collision,
    Roadworks,
    PoliceStop,
    Debris,
    Flooding
};

struct TrafficIncident
{
    std::uint32_t id = 0;
    std::string name = "Traffic Incident";
    TrafficIncidentType type = TrafficIncidentType::Breakdown;
    bool enabled = true;
    std::uint32_t roadId = 0;
    std::uint32_t linkId = 0;
    Vec3 position{};
    float radiusM = 20.0f;
    float severity = 0.5f;
    float blockedLaneFraction = 0.5f;
    float speedLimitKmh = 20.0f;
    float routeCostMultiplier = 3.0f;
    float responseDelayS = 30.0f;
    float clearAfterS = 300.0f;
    bool emergencyResponse = true;
    bool hazardLights = true;
};

struct TrafficEnvironmentConfiguration
{
    float wetSpeedFactor = 0.88f;
    float heavyRainSpeedFactor = 0.72f;
    float snowSpeedFactor = 0.62f;
    float iceSpeedFactor = 0.38f;
    float nightSpeedFactor = 0.92f;
    float wetFollowingGapFactor = 1.18f;
    float wetBrakingFactor = 0.78f;
    float poorVisibilitySpeedFactor = 0.72f;
    bool standingWaterAvoidance = true;
    bool weatherAwareLaneChanges = true;
};

struct TrafficBehaviorConfiguration
{
    bool zipperMerging = true;
    bool roundaboutNegotiation = true;
    bool enforceStopDwell = true;
    bool opportunisticOvertaking = true;
    bool queueDischargeReaction = true;
    bool stagedParkingManeuvers = true;
    bool stuckRecovery = true;
    bool collisionIncidentResponse = true;
    bool emergencyIncidentDispatch = true;
    float zipperAlternationWindowS = 3.0f;
    float mergeCourtesyGapS = 1.2f;
    float roundaboutEntryGapS = 2.5f;
    float stopDwellS = 1.0f;
    float yieldCreepSpeedKmh = 4.0f;
    float overtakeMinimumGainKmh = 8.0f;
    float overtakeReturnGapM = 18.0f;
    float queueReactionSpreadS = 0.65f;
    float parkingReverseSpeedKmh = 4.0f;
    float recoveryReverseSeconds = 1.25f;
    float recoveryRerouteSeconds = 8.0f;
    float recoveryTeleportSeconds = 35.0f;
    float collisionDistanceM = 1.2f;
    float emergencyIncidentLookaheadM = 1500.0f;
};

struct TrafficDebugConfiguration
{
    bool enabled = true;
    bool showAgentIds = true;
    bool showRoutes = true;
    bool showIntentions = true;
    bool showPerception = false;
    bool showFollowingGaps = true;
    bool showLaneChangeScores = true;
    bool showWaitReasons = true;
    bool showStreamingTiers = true;
    bool showIncidentInfluence = true;
    int maxDetailedAgents = 48;
};

struct IntersectionController
{
    std::uint32_t intersectionId = 0;
    SignalControlMode mode = SignalControlMode::FixedTime;
    float phaseOffsetSeconds = 0.0f;
    float minimumGreenSeconds = 8.0f;
    float maximumGreenSeconds = 50.0f;
    float detectorDistanceM = 45.0f;
    float gapOutSeconds = 3.0f;
    bool queueAdaptive = false;
    bool emergencyPreemption = true;
};

enum class RoadRestrictionType
{
    Closure,
    Incident,
    Construction,
    EventClosure,
    Toll,
    LowEmission,
    WeightLimit,
    HeightLimit
};

struct RoadRestriction
{
    std::uint32_t id = 0;
    std::string name = "Road Restriction";
    RoadRestrictionType type = RoadRestrictionType::Closure;
    bool enabled = true;
    std::uint32_t roadId = 0;
    std::uint32_t linkId = 0;
    bool blockTraffic = true;
    bool emergencyExempt = true;
    float speedLimitKmh = 0.0f;
    float routeCostMultiplier = 4.0f;
    float startHour = 0.0f;
    float endHour = 24.0f;
    float vehicleMassLimitKg = 0.0f;
    float vehicleHeightLimitM = 0.0f;
};

enum class GameEventType
{
    CircuitRace,
    Sprint,
    TimeTrial,
    TimeAttack,
    Drag,
    Drift,
    Touge,
    ClandestineCircuit,
    ClandestineSprint,
    Cruise,
    TestDrive,
    Autoslalom,
    Gymkhana
};

struct GameEvent
{
    std::uint32_t id = 0;
    std::string name = "New Event";
    GameEventType type = GameEventType::CircuitRace;
    bool enabled = true;
    std::uint32_t startMarkerId = 0;
    std::uint32_t finishMarkerId = 0;
    std::uint32_t layoutId = 0;
    int laps = 3;
    int maxEntrants = 16;
    bool rollingStart = false;
    bool trafficEnabled = false;
    bool policeEnabled = false;
    bool nightOnly = false;
    float entryFee = 0.0f;
    float reward = 1000.0f;
    float heat = 0.0f;
};

enum class WorldPointType
{
    Garage,
    Dealership,
    FuelStation,
    RepairShop,
    CarWash,
    MeetSpot,
    EventHub,
    Safehouse,
    PoliceStation,
    SpeedCamera,
    SpeedTrap,
    FastTravel,
    Landmark,
    ParkingArea
};

struct WorldPoint
{
    std::uint32_t id = 0;
    std::string name = "World Point";
    WorldPointType type = WorldPointType::Landmark;
    bool enabled = true;
    Vec3 position{};
    float headingDeg = 0.0f;
    float radiusM = 8.0f;
    bool discoverable = true;
    bool fastTravelEnabled = false;
    float servicePriceMultiplier = 1.0f;
};



struct PoliceGameplayConfiguration
{
    bool enabled = false;
    int maxHeatLevel = 5;
    int maxPursuitUnits = 12;
    float civilianWitnessRadiusM = 120.0f;
    float policeDetectionRadiusM = 180.0f;
    float speedToleranceKmh = 12.0f;
    float heatDecayDelayS = 20.0f;
    float heatDecayPerSecond = 0.035f;
    float lostSightSeconds = 12.0f;
    float searchDurationS = 90.0f;
    float cooldownDurationS = 30.0f;
    float bustHoldSeconds = 5.0f;
    float backupDelayS = 4.0f;
    float roadblockMinimumHeat = 3.0f;
    bool civilianWitnesses = true;
    bool speedingGeneratesHeat = true;
    bool collisionsGenerateHeat = true;
    bool illegalRacesGenerateHeat = true;
    bool evasionEscalatesHeat = true;
};

struct PolicePatrolZone
{
    std::uint32_t id = 0;
    std::string name = "Police Patrol Zone";
    bool enabled = true;
    Vec3 position{};
    float radiusM = 600.0f;
    float patrolWeight = 1.0f;
    int maximumUnits = 4;
    float responseMultiplier = 1.0f;
    float speedToleranceKmh = 10.0f;
    float startHour = 0.0f;
    float endHour = 24.0f;
    std::uint32_t responsePortalId = 0;
};

struct PoliceRoadblockSite
{
    std::uint32_t id = 0;
    std::string name = "Roadblock Site";
    bool enabled = true;
    std::uint32_t nodeId = 0;
    Vec3 position{};
    float headingDeg = 0.0f;
    float widthM = 10.0f;
    float minimumHeat = 3.0f;
    int unitCount = 2;
    bool spikeStrip = true;
    bool leaveEscapeGap = false;
    float selectionWeight = 1.0f;
};

struct PoliceEscapeZone
{
    std::uint32_t id = 0;
    std::string name = "Escape / Cooldown Zone";
    bool enabled = true;
    Vec3 position{};
    float radiusM = 180.0f;
    float searchTimeMultiplier = 0.5f;
    float heatDecayMultiplier = 2.0f;
    bool breakLineOfSight = true;
    bool safehouse = false;
};

struct ClandestineMeet
{
    std::uint32_t id = 0;
    std::string name = "Clandestine Meet";
    bool enabled = true;
    Vec3 position{};
    float headingDeg = 0.0f;
    float radiusM = 35.0f;
    float openHour = 20.0f;
    float closeHour = 5.0f;
    int maximumVehicles = 24;
    float policeRisk = 0.25f;
    float heatMultiplier = 1.0f;
    std::uint32_t eventId = 0;
    bool discoverable = true;
};

struct MotorsportClass
{
    std::uint32_t id = 0;
    std::string name = "Open";
    std::string code = "OPEN";
    bool enabled = true;
    float minimumPowerKw = 0.0f;
    float maximumPowerKw = 10000.0f;
    float minimumWeightKg = 0.0f;
    float maximumWeightKg = 10000.0f;
    float balanceBallastKg = 0.0f;
    int maximumEntrants = 64;
};

struct MotorsportAiConfiguration
{
    bool enabled = true;
    float updateHz = 20.0f;
    float lookaheadMinimumM = 18.0f;
    float lookaheadMaximumM = 120.0f;
    float brakingLookaheadM = 180.0f;
    float opponentAwarenessM = 100.0f;
    float slipstreamMinimumGapM = 4.0f;
    float slipstreamMaximumGapM = 32.0f;
    float overtakeMinimumClosingKmh = 5.0f;
    float defensiveTriggerGapM = 22.0f;
    float blueFlagYieldGapM = 40.0f;
    float wetLineThreshold = 0.20f;
    float maximumWetSpeedPenalty = 0.24f;
    float fuelUseLitersPer100Km = 35.0f;
    float tireWearPer100Km = 0.18f;
    float fuelReserveLaps = 1.25f;
    float tirePitThreshold = 0.32f;
    float mistakeRecoverySeconds = 2.0f;
    bool strategyEnabled = true;
    bool mistakesEnabled = true;
    bool slipstreamEnabled = true;
    bool defendingEnabled = true;
    bool multiclassNegotiation = true;
    bool wetLineEnabled = true;
    bool liveDecisionTelemetry = true;

    // STUDIO21: optional native Heritage Vehicle control for nearby Racing AI.
    // Logical competitors remain authoritative/fallback outside the physical budget.
    bool fullPhysicsCompetitors = false;
    float physicsHighRateHz = 500.0f;
    float steeringLookaheadSeconds = 0.70f;
    float steeringGain = 1.10f;
    float crossTrackGain = 0.22f;
    float throttleGain = 0.11f;
    float brakeGain = 0.16f;
    float maximumSteerAngleDeg = 38.0f;
    float sideBySideSafetyM = 1.25f;
    float trackLimitSafetyM = 0.65f;
    float gripSlipRatioLimit = 0.18f;
    float gripSlipAngleDeg = 9.0f;
    float physicalRecoveryDistanceM = 28.0f;
    float formationSpeedKmh = 80.0f;
    float rollingStartSpeedKmh = 90.0f;
    float pitLaneSpeedKmh = 60.0f;
    float damagePitThreshold = 0.62f;
    float damageDnfThreshold = 0.16f;
    float collisionDamageScale = 0.10f;
    float weatherForecastSeconds = 60.0f;
    bool gripAwareBraking = true;
    bool spatialAvoidance = true;
    bool trackLimitAwarePassing = true;
    bool damageStrategyEnabled = true;
    bool weatherForecastEnabled = true;

    // STUDIO22: close-quarters racecraft uses the actual physics collider
    // footprint as its primary spatial authority, with explicit predictive
    // margins/rules layered above it for stewarding and strategy.
    bool colliderBoundsAuthority = true;
    float collisionEnvelopeMarginM = 0.18f;
    float sweptEnvelopeSeconds = 0.70f;
    float sideBySideOverlapToleranceM = 0.10f;
    float divebombCommitGapM = 12.0f;
    float divebombClosingThresholdKmh = 18.0f;
    float switchbackWindowS = 1.50f;
    int maximumDefensiveMovesPerStraight = 1;
    float blockingPenaltySeconds = 5.0f;
    float unsafeReleasePenaltySeconds = 7.0f;
    float pitReleaseLookaheadM = 55.0f;
    float multiclassPassHorizonS = 2.50f;
    float tireOptimalMinimumC = 75.0f;
    float tireOptimalMaximumC = 105.0f;
    float fuelDensityKgPerLiter = 0.745f;
    bool predictiveCollisionAvoidance = true;
    bool divebombJudgement = true;
    bool blockingRules = true;
    bool unsafeReleaseStewarding = true;
    bool tireThermalStrategy = true;
    bool fuelMassAwareness = true;
    bool componentDamageStrategy = true;

    // STUDIO23: solver-contact evidence and incident stewarding. The collision
    // solver remains the physical authority; these values only decide when a
    // real resolved contact is significant enough to record/judge.
    bool contactEvidenceEnabled = true;
    bool incidentStewardingEnabled = true;
    float incidentMinimumNormalImpulseNs = 180.0f;
    float incidentMinimumClosingKmh = 4.0f;
    float severeIncidentNormalImpulseNs = 3200.0f;
    float severeIncidentClosingKmh = 35.0f;
    float avoidableContactPenaltySeconds = 5.0f;
    float severeContactPenaltySeconds = 10.0f;
    float contactEvidenceCooldownSeconds = 0.85f;
    int retainedIncidentEvidence = 32;
};

// STUDIO24: bounded incident-replay capture and ghost steward review. This is
// intentionally separate from Racing AI policy because replay is presentation /
// race-control infrastructure and may later consume player, network and broadcast data too.
struct MotorsportReplayConfiguration
{
    bool enabled = true;
    float sampleHz = 12.0f;
    float preRollSeconds = 8.0f;
    float postRollSeconds = 5.0f;
    int maximumIncidentClips = 12;
    int maximumRecordedCompetitors = 32;
    bool capturePlayer = true;
    bool captureControls = true;
    bool ghostReviewEnabled = true;
    int maximumGhostVehicles = 16;

    // STUDIO25: replay/broadcast camera director. Camera poses are FP64 global
    // positions and therefore survive floating-origin shifts during review.
    bool broadcastDirectorEnabled = true;
    bool autoIncidentCamera = true;
    float incidentCameraDistanceM = 13.0f;
    float incidentCameraHeightM = 4.5f;
    float tracksideCameraLeadM = 22.0f;
    float helicopterCameraHeightM = 28.0f;
    float cameraSmoothing = 9.0f;
};

struct MotorsportEntrant
{
    std::uint32_t id = 0;
    std::string driverName = "AI Driver";
    std::string teamName = "Privateer";
    std::string vehiclePreset = "PrototypeCar";
    bool enabled = true;
    std::uint32_t eventId = 0;
    std::uint32_t classId = 0;
    int raceNumber = 1;
    float aiSkill = 0.80f;
    float qualifyingPace = 0.80f;
    float racePace = 0.80f;
    float wetSkill = 0.75f;
    float aggression = 0.50f;
    float consistency = 0.80f;
    float pitSkill = 0.75f;
    float racecraft = 0.75f;
    float awareness = 0.80f;
    float defending = 0.55f;
    float tireManagement = 0.70f;
    float fuelManagement = 0.70f;
    float strategyRisk = 0.50f;
    float mistakeRatePerHour = 0.18f;
    float reactionTimeS = 0.25f;
    float preferredLineBias = 0.0f;
    bool clandestine = false;
    int gridOverride = 0;
};

struct MotorsportChampionship
{
    std::uint32_t id = 0;
    std::string name = "Championship";
    bool enabled = true;
    std::uint32_t classId = 0;
    std::string pointsScheme = "25,18,15,12,10,8,6,4,2,1";
    float poleBonus = 0.0f;
    float fastestLapBonus = 1.0f;
    int dropWorstRounds = 0;
};

struct MotorsportRound
{
    std::uint32_t id = 0;
    std::uint32_t championshipId = 0;
    std::uint32_t eventId = 0;
    std::string name = "Round";
    bool enabled = true;
    int order = 0;
    float pointsMultiplier = 1.0f;
};

struct MotorsportConfiguration
{
    bool enabled = true;
    bool aiCompetitorsEnabled = true;
    bool autoBuildGrid = true;
    bool simulateUnspawnedCompetitors = true;
    int maxPhysicalCompetitors = 32;
    float defaultAiSkill = 0.80f;
    float qualifyingPaceSpreadPercent = 4.0f;
    float baseMechanicalDnfChancePerHour = 0.01f;
    bool multiClassTiming = true;
    bool championshipPersistence = true;
};

struct EventExecutionConfiguration
{
    bool enabled = true;
    bool autoStagePlayer = true;
    bool autoSavePersonalBests = true;
    float gridSettleSeconds = 1.5f;
    float countdownSeconds = 3.0f;
    float falseStartSpeedKmh = 1.0f;
    float gateDebounceSeconds = 0.35f;
    float trackLimitGraceSeconds = 1.0f;
    float trackLimitRejoinSeconds = 0.5f;
    float fullCourseYellowSpeedKmh = 80.0f;
    float virtualSafetyCarSpeedKmh = 80.0f;
    float safetyCarSpeedKmh = 100.0f;
    float resultsHoldSeconds = 12.0f;
    bool practiceLoopEnabled = true;
    bool practiceLoopAutoRestart = true;
    bool practiceLoopRestoreAngularVelocity = true;
    bool practiceLoopRestoreGear = true;
    float practiceLoopEndGateWidthM = 12.0f;
    float practiceLoopRestoreDelayS = 0.15f;
};

struct WeatherWorld
{
    float latitude = 46.49f;
    float longitude = 14.97f;
    float elevationM = 640.0f;
    float startHour = 12.0f;
    float cloudCoverage = 0.35f;
    float rainIntensity = 0.0f;
    float temperatureC = 20.0f;
    float humidity = 0.55f;
    float surfaceWetness = 0.0f;
};

struct VehicleStudioProfile
{
    std::string displayName = "Peugeot 206 RC";
    std::string vehicleDefinition = "Modules/RacingUnited/Assets/Vehicles/Peugeot206RC";
    std::string acousticProfile = "Peugeot206RC_EW10J4S_Stock";
    std::string tireSet;
    float spawnHeightM = 0.15f;
    float fuelLiters = 50.0f;
    bool trafficEligible = true;
    bool raceEligible = true;
};

class StudioAuthoringData
{
public:
    void resetDefaults();

    SceneObject& addSceneObject(SceneObjectType type, const char* preferredName = nullptr);
    RaceMarker& addRaceMarker(RaceMarkerType type, const char* preferredName = nullptr);
    TrafficNode& addTrafficNode(TrafficNodeType type, const char* preferredName = nullptr);
    TrafficLink& addTrafficLink(std::uint32_t fromNodeId, std::uint32_t toNodeId);
    RoadSpline& addRoadSpline(RoadClass roadClass, const char* preferredName = nullptr);
    RoadSplineNode& addRoadSplineNode(std::uint32_t roadId);
    RoadIntersection& addRoadIntersection(const char* preferredName = nullptr);
    TurnConnector& addTurnConnector(std::uint32_t intersectionId, std::uint32_t fromRoadId, std::uint32_t toRoadId);
    TrafficSignalPhase& addTrafficSignalPhase(std::uint32_t intersectionId, const char* preferredName = nullptr);
    ParkingStrip& addParkingStrip(const char* preferredName = nullptr);
    RoadRestriction& addRoadRestriction(RoadRestrictionType type, const char* preferredName = nullptr);
    TrafficAgentProfile& addTrafficAgentProfile(TrafficAgentClass vehicleClass, const char* preferredName = nullptr);
    TrafficSpawnPortal& addTrafficSpawnPortal(const char* preferredName = nullptr);
    TrafficDensityRegion& addTrafficDensityRegion(const char* preferredName = nullptr);
    TrafficIncident& addTrafficIncident(TrafficIncidentType type, const char* preferredName = nullptr);
    RaceRoute& addRaceRoute(RaceRouteType type, const char* preferredName = nullptr);
    RaceRouteNode& addRaceRouteNode(std::uint32_t routeId);
    RaceLayout& addRaceLayout(const char* preferredName = nullptr);
    RaceSession& addRaceSession(RaceSessionType type, const char* preferredName = nullptr);
    RaceSupportPoint& addRaceSupportPoint(RaceSupportPointType type, const char* preferredName = nullptr);
    BroadcastCameraPath& addBroadcastCameraPath(BroadcastCameraPathType type, const char* preferredName = nullptr);
    BroadcastCameraNode& addBroadcastCameraNode(std::uint32_t pathId);
    CourseCone& addCourseCone(ConeRole role, const char* preferredName = nullptr);
    ConeCourseGate& addConeCourseGate(ConeCourseGateType type, const char* preferredName = nullptr);
    GameEvent& addGameEvent(GameEventType type, const char* preferredName = nullptr);
    WorldPoint& addWorldPoint(WorldPointType type, const char* preferredName = nullptr);
    PolicePatrolZone& addPolicePatrolZone(const char* preferredName = nullptr);
    PoliceRoadblockSite& addPoliceRoadblockSite(const char* preferredName = nullptr);
    PoliceEscapeZone& addPoliceEscapeZone(const char* preferredName = nullptr);
    ClandestineMeet& addClandestineMeet(const char* preferredName = nullptr);
    MotorsportClass& addMotorsportClass(const char* preferredName = nullptr);
    MotorsportEntrant& addMotorsportEntrant(const char* preferredName = nullptr);
    MotorsportChampionship& addMotorsportChampionship(const char* preferredName = nullptr);
    MotorsportRound& addMotorsportRound(std::uint32_t championshipId, const char* preferredName = nullptr);

    bool removeSceneObject(std::size_t index);
    bool removeRaceMarker(std::size_t index);
    bool removeTrafficNode(std::size_t index);
    bool removeTrafficLink(std::size_t index);
    bool removeRoadSpline(std::size_t index);
    bool removeRoadSplineNode(std::size_t index);
    bool removeRoadIntersection(std::size_t index);
    bool removeTurnConnector(std::size_t index);
    bool removeTrafficSignalPhase(std::size_t index);
    bool removeParkingStrip(std::size_t index);
    bool removeRoadRestriction(std::size_t index);
    bool removeTrafficAgentProfile(std::size_t index);
    bool removeTrafficSpawnPortal(std::size_t index);
    bool removeTrafficDensityRegion(std::size_t index);
    bool removeTrafficIncident(std::size_t index);
    void compileRoadSplinesToLaneGraph(int& createdNodes, int& updatedNodes, int& createdLinks);
    bool removeRaceRoute(std::size_t index);
    bool removeRaceRouteNode(std::size_t index);
    bool removeRaceLayout(std::size_t index);
    bool removeRaceSession(std::size_t index);
    bool removeRaceSupportPoint(std::size_t index);
    bool removeBroadcastCameraPath(std::size_t index);
    bool removeBroadcastCameraNode(std::size_t index);
    bool removeCourseCone(std::size_t index);
    bool removeConeCourseGate(std::size_t index);
    bool removeGameEvent(std::size_t index);
    bool removeWorldPoint(std::size_t index);
    bool removePolicePatrolZone(std::size_t index);
    bool removePoliceRoadblockSite(std::size_t index);
    bool removePoliceEscapeZone(std::size_t index);
    bool removeClandestineMeet(std::size_t index);
    bool removeMotorsportClass(std::size_t index);
    bool removeMotorsportEntrant(std::size_t index);
    bool removeMotorsportChampionship(std::size_t index);
    bool removeMotorsportRound(std::size_t index);

    bool saveAll(const std::filesystem::path& root, std::string& message) const;
    bool loadAll(const std::filesystem::path& root, std::string& message);

    bool saveScene(const std::filesystem::path& file, std::string& message) const;
    bool loadScene(const std::filesystem::path& file, std::string& message);
    bool saveRace(const std::filesystem::path& file, std::string& message) const;
    bool loadRace(const std::filesystem::path& file, std::string& message);
    bool saveTraffic(const std::filesystem::path& file, std::string& message) const;
    bool loadTraffic(const std::filesystem::path& file, std::string& message);
    bool saveWeather(const std::filesystem::path& file, std::string& message) const;
    bool loadWeather(const std::filesystem::path& file, std::string& message);
    bool saveVehicle(const std::filesystem::path& file, std::string& message) const;
    bool loadVehicle(const std::filesystem::path& file, std::string& message);
    bool saveGameplay(const std::filesystem::path& file, std::string& message) const;
    bool loadGameplay(const std::filesystem::path& file, std::string& message);

    std::vector<SceneObject> sceneObjects;
    std::vector<RaceMarker> raceMarkers;
    RaceConfiguration race;
    std::vector<RaceRoute> raceRoutes;
    std::vector<RaceRouteNode> raceRouteNodes;
    std::vector<RaceLayout> raceLayouts;
    std::vector<RaceSession> raceSessions;
    RaceControlConfiguration raceControl;
    std::vector<RaceSupportPoint> raceSupportPoints;
    std::vector<BroadcastCameraPath> broadcastCameraPaths;
    std::vector<BroadcastCameraNode> broadcastCameraNodes;
    ConeCourseConfiguration coneCourse;
    std::vector<CourseCone> courseCones;
    std::vector<ConeCourseGate> coneCourseGates;
    std::vector<TrafficNode> trafficNodes;
    std::vector<TrafficLink> trafficLinks;
    std::vector<RoadSpline> roadSplines;
    std::vector<RoadSplineNode> roadSplineNodes;
    std::vector<RoadIntersection> roadIntersections;
    std::vector<TurnConnector> turnConnectors;
    std::vector<TrafficSignalPhase> trafficSignalPhases;
    std::vector<ParkingStrip> parkingStrips;
    TrafficPopulationConfiguration trafficPopulation;
    NavigationBuildConfiguration navigationBuild;
    TrafficRulesConfiguration trafficRules;
    TrafficStreamingConfiguration trafficStreaming;
    std::vector<IntersectionController> intersectionControllers;
    std::vector<RoadRestriction> roadRestrictions;
    TrafficAgentSimulationConfiguration trafficAgentSimulation;
    std::vector<TrafficAgentProfile> trafficAgentProfiles;
    std::vector<TrafficSpawnPortal> trafficSpawnPortals;
    std::vector<TrafficDensityRegion> trafficDensityRegions;
    std::vector<TrafficIncident> trafficIncidents;
    TrafficEnvironmentConfiguration trafficEnvironment;
    TrafficBehaviorConfiguration trafficBehavior;
    TrafficDebugConfiguration trafficDebug;
    std::vector<GameEvent> gameEvents;
    std::vector<WorldPoint> worldPoints;
    PoliceGameplayConfiguration policeGameplay;
    std::vector<PolicePatrolZone> policePatrolZones;
    std::vector<PoliceRoadblockSite> policeRoadblockSites;
    std::vector<PoliceEscapeZone> policeEscapeZones;
    std::vector<ClandestineMeet> clandestineMeets;
    MotorsportConfiguration motorsport;
    MotorsportAiConfiguration motorsportAi;
    MotorsportReplayConfiguration motorsportReplay;
    std::vector<MotorsportClass> motorsportClasses;
    std::vector<MotorsportEntrant> motorsportEntrants;
    std::vector<MotorsportChampionship> motorsportChampionships;
    std::vector<MotorsportRound> motorsportRounds;
    EventExecutionConfiguration eventExecution;
    WeatherWorld weather;
    VehicleStudioProfile vehicle;

private:
    std::uint32_t m_nextId = 1;
    std::uint32_t allocateId();
};

const char* sceneObjectTypeName(SceneObjectType type);
const char* raceMarkerTypeName(RaceMarkerType type);
const char* broadcastCameraPathTypeName(BroadcastCameraPathType type);
const char* coneRoleName(ConeRole role);
const char* coneTrafficModeName(ConeTrafficMode mode);
const char* conePenaltyModeName(ConePenaltyMode mode);
const char* coneCourseGateTypeName(ConeCourseGateType type);
const char* gridTemplateName(GridTemplate value);
const char* raceRouteTypeName(RaceRouteType type);
const char* raceSessionTypeName(RaceSessionType type);
const char* sessionGridSourceName(SessionGridSource value);
const char* raceSupportPointTypeName(RaceSupportPointType type);
const char* trafficNodeTypeName(TrafficNodeType type);
const char* trafficLinkTypeName(TrafficLinkType type);
const char* roadClassName(RoadClass value);
const char* junctionPriorityName(JunctionPriority value);
const char* drivingSideName(DrivingSide value);
const char* signalControlModeName(SignalControlMode value);
const char* roadRestrictionTypeName(RoadRestrictionType value);
const char* trafficAgentClassName(TrafficAgentClass value);
const char* trafficPortalModeName(TrafficPortalMode value);
const char* trafficIncidentTypeName(TrafficIncidentType value);
const char* gameEventTypeName(GameEventType type);
const char* worldPointTypeName(WorldPointType type);

} // namespace heritage::studio::authoring
