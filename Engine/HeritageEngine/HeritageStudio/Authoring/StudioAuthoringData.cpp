#include "StudioAuthoringData.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace heritage::studio::authoring {
namespace {

std::string quote(const std::string& value)
{
    std::ostringstream stream;
    stream << std::quoted(value);
    return stream.str();
}

bool ensureParent(const std::filesystem::path& file, std::string& message)
{
    std::error_code ec;
    if (!file.parent_path().empty())
        std::filesystem::create_directories(file.parent_path(), ec);
    if (ec)
    {
        message = "Could not create authoring directory: " + ec.message();
        return false;
    }
    return true;
}

bool openOutput(const std::filesystem::path& file, std::ofstream& out, std::string& message)
{
    if (!ensureParent(file, message))
        return false;
    out.open(file, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        message = "Could not write " + file.string();
        return false;
    }
    return true;
}

bool openInput(const std::filesystem::path& file, std::ifstream& in, std::string& message)
{
    in.open(file, std::ios::binary);
    if (!in)
    {
        message = "Could not read " + file.string();
        return false;
    }
    return true;
}

void writeVec3(std::ostream& out, const Vec3& value)
{
    out << value.x << ' ' << value.y << ' ' << value.z;
}

bool readVec3(std::istream& in, Vec3& value)
{
    return static_cast<bool>(in >> value.x >> value.y >> value.z);
}

template <typename Enum>
Enum clampEnum(int value, int maximum)
{
    return static_cast<Enum>(std::clamp(value, 0, maximum));
}

} // namespace

const char* sceneObjectTypeName(SceneObjectType type)
{
    switch (type)
    {
    case SceneObjectType::Empty: return "Empty";
    case SceneObjectType::Mesh: return "Mesh";
    case SceneObjectType::PlayerSpawn: return "Player Spawn";
    case SceneObjectType::VehicleSpawn: return "Vehicle Spawn";
    case SceneObjectType::AudioZone: return "Audio Zone";
    case SceneObjectType::WeatherZone: return "Weather Zone";
    case SceneObjectType::Trigger: return "Trigger";
    }
    return "Unknown";
}

const char* raceMarkerTypeName(RaceMarkerType type)
{
    switch (type)
    {
    case RaceMarkerType::StartFinish: return "Start / Finish";
    case RaceMarkerType::Checkpoint: return "Checkpoint";
    case RaceMarkerType::Sector: return "Sector";
    case RaceMarkerType::GridSlot: return "Grid Slot";
    case RaceMarkerType::PitEntry: return "Pit Entry";
    case RaceMarkerType::PitExit: return "Pit Exit";
    case RaceMarkerType::PitSpeedLine: return "Pit Speed Line";
    case RaceMarkerType::PitBox: return "Pit Box";
    case RaceMarkerType::TrackLimitLeft: return "Track Limit Left";
    case RaceMarkerType::TrackLimitRight: return "Track Limit Right";
    case RaceMarkerType::Recovery: return "Recovery";
    case RaceMarkerType::ReplayCamera: return "Replay Camera";
    case RaceMarkerType::AiLineNode: return "AI Race Line";
    case RaceMarkerType::WetLineNode: return "AI Wet Line";
    case RaceMarkerType::TimingLoop: return "Timing Loop";
    case RaceMarkerType::SpeedTrapStart: return "Speed Trap Start";
    case RaceMarkerType::SpeedTrapFinish: return "Speed Trap Finish";
    case RaceMarkerType::SafetyCarLine: return "Safety Car Line";
    case RaceMarkerType::FormationLine: return "Formation Line";
    }
    return "Unknown";
}

const char* broadcastCameraPathTypeName(BroadcastCameraPathType type)
{
    switch (type)
    {
    case BroadcastCameraPathType::Dolly: return "Dolly";
    case BroadcastCameraPathType::Crane: return "Crane";
    case BroadcastCameraPathType::Cable: return "Cable";
    case BroadcastCameraPathType::Drone: return "Drone";
    }
    return "Unknown";
}

const char* coneRoleName(ConeRole role)
{
    switch (role)
    {
    case ConeRole::TrafficGuide: return "Traffic Guide";
    case ConeRole::Boundary: return "Boundary";
    case ConeRole::GateLeft: return "Gate Left";
    case ConeRole::GateRight: return "Gate Right";
    case ConeRole::SlalomLeft: return "Slalom Left";
    case ConeRole::SlalomRight: return "Slalom Right";
    case ConeRole::Start: return "Start";
    case ConeRole::Finish: return "Finish";
    case ConeRole::Turnaround: return "Turnaround";
    case ConeRole::StopBox: return "Stop Box";
    case ConeRole::Chicane: return "Chicane";
    case ConeRole::Pointer: return "Pointer";
    case ConeRole::NoGoBoundary: return "No-Go Boundary";
    case ConeRole::RoadClosure: return "Road Closure";
    }
    return "Unknown";
}

const char* coneTrafficModeName(ConeTrafficMode mode)
{
    switch (mode)
    {
    case ConeTrafficMode::None: return "None";
    case ConeTrafficMode::Guide: return "Guide / Discourage";
    case ConeTrafficMode::Slow: return "Slow";
    case ConeTrafficMode::CloseLane: return "Close Lane";
    case ConeTrafficMode::CloseRoad: return "Close Road";
    }
    return "Unknown";
}

const char* conePenaltyModeName(ConePenaltyMode mode)
{
    switch (mode)
    {
    case ConePenaltyMode::None: return "None";
    case ConePenaltyMode::Contact: return "Contact";
    case ConePenaltyMode::Displaced: return "Displaced";
    case ConePenaltyMode::KnockedDown: return "Knocked Down";
    }
    return "Unknown";
}

const char* coneCourseGateTypeName(ConeCourseGateType type)
{
    switch (type)
    {
    case ConeCourseGateType::Gate: return "Gate";
    case ConeCourseGateType::SlalomLeft: return "Slalom Left";
    case ConeCourseGateType::SlalomRight: return "Slalom Right";
    case ConeCourseGateType::TurnaroundLeft: return "Turnaround Left";
    case ConeCourseGateType::TurnaroundRight: return "Turnaround Right";
    case ConeCourseGateType::StopBox: return "Stop Box";
    case ConeCourseGateType::Finish: return "Finish";
    case ConeCourseGateType::CircleLeft: return "360 Circle Left";
    case ConeCourseGateType::CircleRight: return "360 Circle Right";
    }
    return "Unknown";
}

const char* gridTemplateName(GridTemplate value)
{
    switch (value)
    {
    case GridTemplate::StaggeredTwoWide: return "Staggered 2-wide";
    case GridTemplate::TwoWide: return "2-wide";
    case GridTemplate::ThreeWide: return "3-wide";
    case GridTemplate::SingleFile: return "Single file";
    case GridTemplate::EnduranceAngled: return "Endurance angled";
    }
    return "Unknown";
}

const char* raceRouteTypeName(RaceRouteType type)
{
    switch (type)
    {
    case RaceRouteType::MainCircuit: return "Main Circuit";
    case RaceRouteType::PitLane: return "Pit Lane";
    case RaceRouteType::SafetyCar: return "Safety Car";
    case RaceRouteType::Formation: return "Formation";
    case RaceRouteType::AlternateLayout: return "Alternate Layout";
    case RaceRouteType::Sprint: return "Sprint";
    case RaceRouteType::Hillclimb: return "Hillclimb";
    case RaceRouteType::Drag: return "Drag";
    }
    return "Unknown";
}

const char* raceSessionTypeName(RaceSessionType type)
{
    switch (type)
    {
    case RaceSessionType::Practice: return "Practice";
    case RaceSessionType::Qualifying: return "Qualifying";
    case RaceSessionType::Warmup: return "Warm-up";
    case RaceSessionType::Race: return "Race";
    case RaceSessionType::TimeAttack: return "Time Attack";
    case RaceSessionType::TestSession: return "Test Session";
    }
    return "Unknown";
}

const char* sessionGridSourceName(SessionGridSource value)
{
    switch (value)
    {
    case SessionGridSource::EventOrder: return "Event order";
    case SessionGridSource::PreviousSession: return "Previous session";
    case SessionGridSource::Qualifying: return "Qualifying";
    case SessionGridSource::Championship: return "Championship";
    case SessionGridSource::ReverseTopN: return "Reverse top N";
    }
    return "Unknown";
}

const char* raceSupportPointTypeName(RaceSupportPointType type)
{
    switch (type)
    {
    case RaceSupportPointType::MarshalPost: return "Marshal Post";
    case RaceSupportPointType::RecoveryVehicle: return "Recovery Vehicle";
    case RaceSupportPointType::TowTruck: return "Tow Truck";
    case RaceSupportPointType::Medical: return "Medical";
    case RaceSupportPointType::FireCrew: return "Fire Crew";
    case RaceSupportPointType::RaceControl: return "Race Control";
    case RaceSupportPointType::SafetyCarStandby: return "Safety Car Standby";
    case RaceSupportPointType::TimingEquipment: return "Timing Equipment";
    }
    return "Unknown";
}

const char* trafficNodeTypeName(TrafficNodeType type)
{
    switch (type)
    {
    case TrafficNodeType::LaneNode: return "Lane Node";
    case TrafficNodeType::Intersection: return "Intersection";
    case TrafficNodeType::Stop: return "Stop";
    case TrafficNodeType::Yield: return "Yield";
    case TrafficNodeType::TrafficLight: return "Traffic Light";
    case TrafficNodeType::Parking: return "Parking";
    case TrafficNodeType::Spawn: return "Spawn";
    case TrafficNodeType::Despawn: return "Despawn";
    case TrafficNodeType::Destination: return "Destination";
    }
    return "Unknown";
}


const char* trafficLinkTypeName(TrafficLinkType type)
{
    switch (type)
    {
    case TrafficLinkType::Travel: return "Travel";
    case TrafficLinkType::LaneChange: return "Lane Change";
    case TrafficLinkType::Merge: return "Merge";
    case TrafficLinkType::JunctionTurn: return "Junction Turn";
    case TrafficLinkType::ParkingAccess: return "Parking Access";
    case TrafficLinkType::SpawnAccess: return "Spawn Access";
    }
    return "Unknown";
}

const char* roadClassName(RoadClass value)
{
    switch (value)
    {
    case RoadClass::Motorway: return "Motorway";
    case RoadClass::Arterial: return "Arterial";
    case RoadClass::Collector: return "Collector";
    case RoadClass::Local: return "Local";
    case RoadClass::Residential: return "Residential";
    case RoadClass::Service: return "Service";
    case RoadClass::Mountain: return "Mountain";
    case RoadClass::Gravel: return "Gravel";
    case RoadClass::Dirt: return "Dirt";
    }
    return "Unknown";
}

const char* junctionPriorityName(JunctionPriority value)
{
    switch (value)
    {
    case JunctionPriority::PriorityRoad: return "Priority Road";
    case JunctionPriority::Yield: return "Yield";
    case JunctionPriority::Stop: return "Stop";
    case JunctionPriority::Signalized: return "Signalized";
    case JunctionPriority::Roundabout: return "Roundabout";
    case JunctionPriority::Uncontrolled: return "Uncontrolled";
    }
    return "Unknown";
}


const char* drivingSideName(DrivingSide value)
{
    return value == DrivingSide::Left ? "Left" : "Right";
}

const char* signalControlModeName(SignalControlMode value)
{
    switch (value)
    {
    case SignalControlMode::FixedTime: return "Fixed Time";
    case SignalControlMode::Actuated: return "Actuated";
    case SignalControlMode::Adaptive: return "Adaptive";
    }
    return "Unknown";
}

const char* roadRestrictionTypeName(RoadRestrictionType value)
{
    switch (value)
    {
    case RoadRestrictionType::Closure: return "Closure";
    case RoadRestrictionType::Incident: return "Incident";
    case RoadRestrictionType::Construction: return "Construction";
    case RoadRestrictionType::EventClosure: return "Event Closure";
    case RoadRestrictionType::Toll: return "Toll";
    case RoadRestrictionType::LowEmission: return "Low Emission";
    case RoadRestrictionType::WeightLimit: return "Weight Limit";
    case RoadRestrictionType::HeightLimit: return "Height Limit";
    }
    return "Unknown";
}

const char* trafficAgentClassName(TrafficAgentClass value)
{
    switch (value)
    {
    case TrafficAgentClass::Compact: return "Compact";
    case TrafficAgentClass::Sedan: return "Sedan";
    case TrafficAgentClass::Sport: return "Sport";
    case TrafficAgentClass::Van: return "Van";
    case TrafficAgentClass::Truck: return "Truck";
    case TrafficAgentClass::Motorcycle: return "Motorcycle";
    case TrafficAgentClass::Emergency: return "Emergency";
    }
    return "Unknown";
}

const char* trafficPortalModeName(TrafficPortalMode value)
{
    switch (value)
    {
    case TrafficPortalMode::SpawnAndDespawn: return "Spawn + Despawn";
    case TrafficPortalMode::SpawnOnly: return "Spawn Only";
    case TrafficPortalMode::DespawnOnly: return "Despawn Only";
    }
    return "Unknown";
}

const char* trafficIncidentTypeName(TrafficIncidentType value)
{
    switch (value)
    {
    case TrafficIncidentType::Breakdown: return "Breakdown";
    case TrafficIncidentType::Collision: return "Collision";
    case TrafficIncidentType::Roadworks: return "Roadworks";
    case TrafficIncidentType::PoliceStop: return "Police Stop";
    case TrafficIncidentType::Debris: return "Debris";
    case TrafficIncidentType::Flooding: return "Flooding";
    }
    return "Unknown";
}

const char* gameEventTypeName(GameEventType type)
{
    switch (type)
    {
    case GameEventType::CircuitRace: return "Circuit Race";
    case GameEventType::Sprint: return "Sprint";
    case GameEventType::TimeTrial: return "Time Trial";
    case GameEventType::TimeAttack: return "Time Attack";
    case GameEventType::Drag: return "Drag";
    case GameEventType::Drift: return "Drift";
    case GameEventType::Touge: return "Touge";
    case GameEventType::ClandestineCircuit: return "Clandestine Circuit";
    case GameEventType::ClandestineSprint: return "Clandestine Sprint";
    case GameEventType::Autoslalom: return "Autoslalom";
    case GameEventType::Gymkhana: return "Gymkhana";
    case GameEventType::Cruise: return "Cruise";
    case GameEventType::TestDrive: return "Test Drive";
    }
    return "Unknown";
}

const char* worldPointTypeName(WorldPointType type)
{
    switch (type)
    {
    case WorldPointType::Garage: return "Garage";
    case WorldPointType::Dealership: return "Dealership";
    case WorldPointType::FuelStation: return "Fuel Station";
    case WorldPointType::RepairShop: return "Repair Shop";
    case WorldPointType::CarWash: return "Car Wash";
    case WorldPointType::MeetSpot: return "Meet Spot";
    case WorldPointType::EventHub: return "Event Hub";
    case WorldPointType::Safehouse: return "Safehouse";
    case WorldPointType::PoliceStation: return "Police Station";
    case WorldPointType::SpeedCamera: return "Speed Camera";
    case WorldPointType::SpeedTrap: return "Speed Trap";
    case WorldPointType::FastTravel: return "Fast Travel";
    case WorldPointType::Landmark: return "Landmark";
    case WorldPointType::ParkingArea: return "Parking Area";
    }
    return "Unknown";
}

std::uint32_t StudioAuthoringData::allocateId()
{
    return m_nextId++;
}

void StudioAuthoringData::resetDefaults()
{
    sceneObjects.clear();
    raceMarkers.clear();
    raceRoutes.clear();
    raceRouteNodes.clear();
    raceLayouts.clear();
    raceSessions.clear();
    raceSupportPoints.clear();
    broadcastCameraPaths.clear();
    broadcastCameraNodes.clear();
    courseCones.clear();
    coneCourseGates.clear();
    trafficNodes.clear();
    trafficLinks.clear();
    roadSplines.clear();
    roadSplineNodes.clear();
    roadIntersections.clear();
    turnConnectors.clear();
    trafficSignalPhases.clear();
    parkingStrips.clear();
    intersectionControllers.clear();
    roadRestrictions.clear();
    trafficAgentProfiles.clear();
    trafficSpawnPortals.clear();
    trafficDensityRegions.clear();
    trafficIncidents.clear();
    gameEvents.clear();
    worldPoints.clear();
    policeGameplay = PoliceGameplayConfiguration{};
    policePatrolZones.clear();
    policeRoadblockSites.clear();
    policeEscapeZones.clear();
    clandestineMeets.clear();
    motorsportClasses.clear();
    motorsportEntrants.clear();
    motorsportChampionships.clear();
    motorsportRounds.clear();
    race = RaceConfiguration{};
    raceControl = RaceControlConfiguration{};
    trafficPopulation = TrafficPopulationConfiguration{};
    navigationBuild = NavigationBuildConfiguration{};
    trafficRules = TrafficRulesConfiguration{};
    trafficStreaming = TrafficStreamingConfiguration{};
    trafficAgentSimulation = TrafficAgentSimulationConfiguration{};
    trafficEnvironment = TrafficEnvironmentConfiguration{};
    trafficBehavior = TrafficBehaviorConfiguration{};
    trafficDebug = TrafficDebugConfiguration{};
    policeGameplay = PoliceGameplayConfiguration{};
    motorsport = MotorsportConfiguration{};
    motorsportAi = MotorsportAiConfiguration{};
    motorsportReplay = MotorsportReplayConfiguration{};
    coneCourse = ConeCourseConfiguration{};
    eventExecution = EventExecutionConfiguration{};
    weather = WeatherWorld{};
    vehicle = VehicleStudioProfile{};
    m_nextId = 1;

    addSceneObject(SceneObjectType::Empty, "Scene Root");
    addSceneObject(SceneObjectType::Mesh, "Track Geometry");
    addSceneObject(SceneObjectType::PlayerSpawn, "Player Spawn");
    auto& start = addRaceMarker(RaceMarkerType::StartFinish, "Start / Finish");
    auto& mainRoute = addRaceRoute(RaceRouteType::MainCircuit, "Main Circuit");
    auto& layout = addRaceLayout("Grand Prix");
    layout.routeId = mainRoute.id;
    layout.startFinishMarkerId = start.id;
    addRaceSession(RaceSessionType::Practice, "Practice").durationMinutes = 20;
    addRaceSession(RaceSessionType::Qualifying, "Qualifying").durationMinutes = 15;
    addRaceSession(RaceSessionType::Race, "Race").laps = race.laps;
    addMotorsportClass("Open");
    auto& defaultTrafficSpawn = addTrafficNode(TrafficNodeType::Spawn, "Traffic Spawn");
    auto& defaultPortal = addTrafficSpawnPortal("Primary Traffic Portal");
    defaultPortal.nodeId = defaultTrafficSpawn.id;
    addRoadSpline(RoadClass::Local, "Main Free-Roam Road");
    auto& compact = addTrafficAgentProfile(TrafficAgentClass::Compact, "Everyday Compact");
    compact.spawnWeight = 1.35f; compact.lengthM = 4.05f; compact.widthM = 1.76f; compact.speedCompliance = 0.95f;
    auto& sedan = addTrafficAgentProfile(TrafficAgentClass::Sedan, "Calm Sedan");
    sedan.spawnWeight = 1.0f; sedan.desiredTimeGapS = 1.9f; sedan.courtesy = 0.78f; sedan.laneChangeAggression = 0.30f;
    auto& van = addTrafficAgentProfile(TrafficAgentClass::Van, "Delivery Van");
    van.spawnWeight = 0.45f; van.lengthM = 5.15f; van.widthM = 2.0f; van.maxSpeedFactor = 0.92f; van.accelerationFactor = 0.75f;
    auto& sport = addTrafficAgentProfile(TrafficAgentClass::Sport, "Assertive Sport");
    sport.spawnWeight = 0.20f; sport.maxSpeedFactor = 1.12f; sport.accelerationFactor = 1.25f; sport.laneChangeAggression = 0.82f; sport.courtesy = 0.35f; sport.speedCompliance = 0.80f;
    auto& truck = addTrafficAgentProfile(TrafficAgentClass::Truck, "Heavy Truck");
    truck.spawnWeight = 0.18f; truck.lengthM = 10.5f; truck.widthM = 2.5f; truck.maxSpeedFactor = 0.78f; truck.accelerationFactor = 0.45f; truck.brakingFactor = 0.75f; truck.desiredTimeGapS = 2.4f; truck.minimumGapM = 4.0f;
}

SceneObject& StudioAuthoringData::addSceneObject(SceneObjectType type, const char* preferredName)
{
    SceneObject value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : sceneObjectTypeName(type);
    sceneObjects.push_back(value);
    return sceneObjects.back();
}

RaceMarker& StudioAuthoringData::addRaceMarker(RaceMarkerType type, const char* preferredName)
{
    RaceMarker value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : raceMarkerTypeName(type);
    value.order = static_cast<int>(raceMarkers.size());
    if (type == RaceMarkerType::GridSlot || type == RaceMarkerType::PitBox)
        value.slot = 0;
    if (type == RaceMarkerType::ReplayCamera)
        value.radiusM = 120.0f;
    raceMarkers.push_back(value);
    return raceMarkers.back();
}

BroadcastCameraPath& StudioAuthoringData::addBroadcastCameraPath(BroadcastCameraPathType type, const char* preferredName)
{
    BroadcastCameraPath value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : (std::string(broadcastCameraPathTypeName(type)) + " Camera Move");
    broadcastCameraPaths.push_back(value);
    return broadcastCameraPaths.back();
}

BroadcastCameraNode& StudioAuthoringData::addBroadcastCameraNode(std::uint32_t pathId)
{
    BroadcastCameraNode value;
    value.id = allocateId();
    value.pathId = pathId;
    int nextOrder = 0;
    for (const auto& node : broadcastCameraNodes) if (node.pathId == pathId) nextOrder = std::max(nextOrder, node.order + 1);
    value.order = nextOrder;
    broadcastCameraNodes.push_back(value);
    return broadcastCameraNodes.back();
}

CourseCone& StudioAuthoringData::addCourseCone(ConeRole role, const char* preferredName)
{
    CourseCone value;
    value.id = allocateId();
    value.role = role;
    value.name = preferredName ? preferredName : coneRoleName(role);
    value.hitPenaltySeconds = coneCourse.defaultHitPenaltySeconds;
    value.displacementToleranceM = coneCourse.defaultDisplacementToleranceM;
    if (role == ConeRole::TrafficGuide) value.trafficMode = ConeTrafficMode::Guide;
    if (role == ConeRole::RoadClosure) value.trafficMode = ConeTrafficMode::CloseRoad;
    if (role == ConeRole::Pointer) value.physical = false;
    courseCones.push_back(value);
    return courseCones.back();
}

ConeCourseGate& StudioAuthoringData::addConeCourseGate(ConeCourseGateType type, const char* preferredName)
{
    ConeCourseGate value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : coneCourseGateTypeName(type);
    value.order = static_cast<int>(coneCourseGates.size());
    value.wrongElementPenaltySeconds = coneCourse.wrongElementPenaltySeconds;
    value.dnfOnMiss = coneCourse.missedElementDnf;
    if (type == ConeCourseGateType::StopBox) { value.widthM = 3.0f; value.lengthM = 5.0f; value.directionRequired = false; }
    if (type == ConeCourseGateType::CircleLeft || type == ConeCourseGateType::CircleRight) { value.widthM = 8.0f; value.lengthM = 2.0f; value.directionRequired = false; }
    coneCourseGates.push_back(value);
    return coneCourseGates.back();
}

TrafficNode& StudioAuthoringData::addTrafficNode(TrafficNodeType type, const char* preferredName)
{
    TrafficNode value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : trafficNodeTypeName(type);
    trafficNodes.push_back(value);
    return trafficNodes.back();
}

TrafficLink& StudioAuthoringData::addTrafficLink(std::uint32_t fromNodeId, std::uint32_t toNodeId)
{
    TrafficLink value;
    value.id = allocateId();
    value.fromNodeId = fromNodeId;
    value.toNodeId = toNodeId;
    trafficLinks.push_back(value);
    return trafficLinks.back();
}

RoadSpline& StudioAuthoringData::addRoadSpline(RoadClass roadClass, const char* preferredName)
{
    RoadSpline value;
    value.id = allocateId();
    value.roadClass = roadClass;
    value.name = preferredName ? preferredName : roadClassName(roadClass);
    if (roadClass == RoadClass::Motorway) { value.lanesForward = 2; value.lanesBackward = 2; value.laneWidthM = 3.65f; value.speedLimitKmh = 130.0f; value.shoulderLeftM = 1.0f; value.shoulderRightM = 2.5f; value.medianWidthM = 2.0f; }
    else if (roadClass == RoadClass::Arterial) { value.lanesForward = 2; value.lanesBackward = 2; value.speedLimitKmh = 70.0f; value.laneWidthM = 3.4f; }
    else if (roadClass == RoadClass::Residential) { value.speedLimitKmh = 30.0f; value.laneWidthM = 3.0f; value.sidewalkLeft = true; value.sidewalkRight = true; value.parkingLeft = true; value.parkingRight = true; }
    else if (roadClass == RoadClass::Service) { value.oneWay = true; value.lanesBackward = 0; value.speedLimitKmh = 20.0f; value.laneWidthM = 3.0f; }
    else if (roadClass == RoadClass::Mountain) { value.speedLimitKmh = 60.0f; value.laneWidthM = 3.1f; value.shoulderLeftM = 0.25f; value.shoulderRightM = 0.25f; }
    else if (roadClass == RoadClass::Gravel || roadClass == RoadClass::Dirt) { value.speedLimitKmh = 50.0f; value.laneWidthM = 3.0f; value.shoulderLeftM = value.shoulderRightM = 0.0f; }
    roadSplines.push_back(value);
    return roadSplines.back();
}

RoadSplineNode& StudioAuthoringData::addRoadSplineNode(std::uint32_t roadId)
{
    RoadSplineNode value;
    value.id = allocateId();
    value.roadId = roadId;
    int nextOrder = 0;
    for (const auto& node : roadSplineNodes) if (node.roadId == roadId) nextOrder = std::max(nextOrder, node.order + 1);
    value.order = nextOrder;
    roadSplineNodes.push_back(value);
    return roadSplineNodes.back();
}

RoadIntersection& StudioAuthoringData::addRoadIntersection(const char* preferredName)
{
    RoadIntersection value;
    value.id = allocateId();
    value.name = preferredName ? preferredName : "Intersection";
    roadIntersections.push_back(value);
    IntersectionController controller; controller.intersectionId = value.id; intersectionControllers.push_back(controller);
    return roadIntersections.back();
}

TurnConnector& StudioAuthoringData::addTurnConnector(std::uint32_t intersectionId, std::uint32_t fromRoadId, std::uint32_t toRoadId)
{
    TurnConnector value;
    value.id = allocateId();
    value.intersectionId = intersectionId; value.fromRoadId = fromRoadId; value.toRoadId = toRoadId;
    turnConnectors.push_back(value);
    return turnConnectors.back();
}

TrafficSignalPhase& StudioAuthoringData::addTrafficSignalPhase(std::uint32_t intersectionId, const char* preferredName)
{
    TrafficSignalPhase value;
    value.id = allocateId(); value.intersectionId = intersectionId;
    value.name = preferredName ? preferredName : "Signal Phase";
    int nextOrder = 0; for (const auto& phase : trafficSignalPhases) if (phase.intersectionId == intersectionId) nextOrder = std::max(nextOrder, phase.order + 1);
    value.order = nextOrder; trafficSignalPhases.push_back(value); return trafficSignalPhases.back();
}

ParkingStrip& StudioAuthoringData::addParkingStrip(const char* preferredName)
{
    ParkingStrip value; value.id = allocateId(); value.name = preferredName ? preferredName : "Parking Strip";
    parkingStrips.push_back(value); return parkingStrips.back();
}

RoadRestriction& StudioAuthoringData::addRoadRestriction(RoadRestrictionType type, const char* preferredName)
{
    RoadRestriction value; value.id = allocateId(); value.type = type;
    value.name = preferredName ? preferredName : roadRestrictionTypeName(type);
    if (type == RoadRestrictionType::Toll || type == RoadRestrictionType::LowEmission) { value.blockTraffic = false; value.routeCostMultiplier = 1.5f; }
    else if (type == RoadRestrictionType::WeightLimit || type == RoadRestrictionType::HeightLimit) { value.blockTraffic = false; value.routeCostMultiplier = 1.0f; }
    roadRestrictions.push_back(value); return roadRestrictions.back();
}

TrafficAgentProfile& StudioAuthoringData::addTrafficAgentProfile(TrafficAgentClass vehicleClass, const char* preferredName)
{
    TrafficAgentProfile value; value.id = allocateId(); value.vehicleClass = vehicleClass;
    value.name = preferredName ? preferredName : trafficAgentClassName(vehicleClass);
    if (vehicleClass == TrafficAgentClass::Compact) { value.lengthM = 4.05f; value.widthM = 1.76f; }
    else if (vehicleClass == TrafficAgentClass::Sport) { value.lengthM = 4.35f; value.widthM = 1.86f; value.maxSpeedFactor = 1.08f; value.accelerationFactor = 1.20f; value.laneChangeAggression = 0.75f; }
    else if (vehicleClass == TrafficAgentClass::Van) { value.lengthM = 5.1f; value.widthM = 2.0f; value.maxSpeedFactor = 0.90f; value.accelerationFactor = 0.75f; }
    else if (vehicleClass == TrafficAgentClass::Truck) { value.lengthM = 10.5f; value.widthM = 2.5f; value.maxSpeedFactor = 0.78f; value.accelerationFactor = 0.45f; value.brakingFactor = 0.75f; value.desiredTimeGapS = 2.4f; value.minimumGapM = 4.0f; }
    else if (vehicleClass == TrafficAgentClass::Motorcycle) { value.lengthM = 2.2f; value.widthM = 0.8f; value.maxSpeedFactor = 1.05f; value.accelerationFactor = 1.15f; value.minimumGapM = 1.5f; }
    else if (vehicleClass == TrafficAgentClass::Emergency) { value.lengthM = 4.8f; value.widthM = 1.95f; value.maxSpeedFactor = 1.25f; value.accelerationFactor = 1.25f; value.brakingFactor = 1.25f; value.laneChangeAggression = 0.9f; value.courtesy = 0.1f; value.speedCompliance = 0.2f; }
    trafficAgentProfiles.push_back(value); return trafficAgentProfiles.back();
}

TrafficSpawnPortal& StudioAuthoringData::addTrafficSpawnPortal(const char* preferredName)
{
    TrafficSpawnPortal value; value.id = allocateId(); value.name = preferredName ? preferredName : "Traffic Portal";
    trafficSpawnPortals.push_back(value); return trafficSpawnPortals.back();
}

TrafficDensityRegion& StudioAuthoringData::addTrafficDensityRegion(const char* preferredName)
{
    TrafficDensityRegion value; value.id = allocateId(); value.name = preferredName ? preferredName : "Traffic Density Region";
    trafficDensityRegions.push_back(value); return trafficDensityRegions.back();
}

TrafficIncident& StudioAuthoringData::addTrafficIncident(TrafficIncidentType type, const char* preferredName)
{
    TrafficIncident value; value.id = allocateId(); value.type = type; value.name = preferredName ? preferredName : trafficIncidentTypeName(type);
    if (type == TrafficIncidentType::Roadworks) { value.clearAfterS = 1800.0f; value.routeCostMultiplier = 5.0f; value.speedLimitKmh = 15.0f; }
    else if (type == TrafficIncidentType::PoliceStop) { value.blockedLaneFraction = 0.25f; value.routeCostMultiplier = 1.8f; value.clearAfterS = 600.0f; }
    else if (type == TrafficIncidentType::Flooding) { value.severity = 0.75f; value.routeCostMultiplier = 6.0f; value.speedLimitKmh = 10.0f; }
    trafficIncidents.push_back(value); return trafficIncidents.back();
}

RaceRoute& StudioAuthoringData::addRaceRoute(RaceRouteType type, const char* preferredName)
{
    RaceRoute value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : raceRouteTypeName(type);
    value.closedLoop = type == RaceRouteType::MainCircuit || type == RaceRouteType::AlternateLayout;
    if (type == RaceRouteType::PitLane)
    {
        value.closedLoop = false;
        value.defaultLeftWidthM = 3.0f;
        value.defaultRightWidthM = 3.0f;
    }
    raceRoutes.push_back(value);
    return raceRoutes.back();
}

RaceRouteNode& StudioAuthoringData::addRaceRouteNode(std::uint32_t routeId)
{
    RaceRouteNode value;
    value.id = allocateId();
    value.routeId = routeId;
    for (const auto& route : raceRoutes)
    {
        if (route.id == routeId)
        {
            value.leftWidthM = route.defaultLeftWidthM;
            value.rightWidthM = route.defaultRightWidthM;
            break;
        }
    }
    int nextOrder = 0;
    for (const auto& node : raceRouteNodes)
        if (node.routeId == routeId) nextOrder = std::max(nextOrder, node.order + 1);
    value.order = nextOrder;
    raceRouteNodes.push_back(value);
    return raceRouteNodes.back();
}

RaceLayout& StudioAuthoringData::addRaceLayout(const char* preferredName)
{
    RaceLayout value;
    value.id = allocateId();
    value.name = preferredName ? preferredName : "Layout";
    value.defaultLaps = race.laps;
    raceLayouts.push_back(value);
    return raceLayouts.back();
}

RaceSession& StudioAuthoringData::addRaceSession(RaceSessionType type, const char* preferredName)
{
    RaceSession value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : raceSessionTypeName(type);
    value.order = static_cast<int>(raceSessions.size());
    if (type == RaceSessionType::Race)
        value.laps = race.laps;
    raceSessions.push_back(value);
    return raceSessions.back();
}

RaceSupportPoint& StudioAuthoringData::addRaceSupportPoint(RaceSupportPointType type, const char* preferredName)
{
    RaceSupportPoint value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : raceSupportPointTypeName(type);
    raceSupportPoints.push_back(value);
    return raceSupportPoints.back();
}

GameEvent& StudioAuthoringData::addGameEvent(GameEventType type, const char* preferredName)
{
    GameEvent value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : gameEventTypeName(type);
    if (type == GameEventType::Sprint || type == GameEventType::ClandestineSprint || type == GameEventType::Drag ||
        type == GameEventType::Drift || type == GameEventType::Touge || type == GameEventType::TimeTrial ||
        type == GameEventType::TimeAttack || type == GameEventType::Cruise || type == GameEventType::TestDrive)
        value.laps = 1;
    if (type == GameEventType::ClandestineCircuit || type == GameEventType::ClandestineSprint)
    {
        value.trafficEnabled = true;
        value.policeEnabled = true;
        value.nightOnly = true;
        value.heat = 0.35f;
    }
    gameEvents.push_back(value);
    return gameEvents.back();
}

WorldPoint& StudioAuthoringData::addWorldPoint(WorldPointType type, const char* preferredName)
{
    WorldPoint value;
    value.id = allocateId();
    value.type = type;
    value.name = preferredName ? preferredName : worldPointTypeName(type);
    value.fastTravelEnabled = type == WorldPointType::Garage || type == WorldPointType::Safehouse || type == WorldPointType::FastTravel;
    worldPoints.push_back(value);
    return worldPoints.back();
}

PolicePatrolZone& StudioAuthoringData::addPolicePatrolZone(const char* preferredName)
{
    PolicePatrolZone value; value.id = allocateId(); value.name = preferredName ? preferredName : "Police Patrol Zone";
    policePatrolZones.push_back(value); return policePatrolZones.back();
}

PoliceRoadblockSite& StudioAuthoringData::addPoliceRoadblockSite(const char* preferredName)
{
    PoliceRoadblockSite value; value.id = allocateId(); value.name = preferredName ? preferredName : "Roadblock Site";
    policeRoadblockSites.push_back(value); return policeRoadblockSites.back();
}

PoliceEscapeZone& StudioAuthoringData::addPoliceEscapeZone(const char* preferredName)
{
    PoliceEscapeZone value; value.id = allocateId(); value.name = preferredName ? preferredName : "Escape / Cooldown Zone";
    policeEscapeZones.push_back(value); return policeEscapeZones.back();
}

ClandestineMeet& StudioAuthoringData::addClandestineMeet(const char* preferredName)
{
    ClandestineMeet value; value.id = allocateId(); value.name = preferredName ? preferredName : "Clandestine Meet";
    clandestineMeets.push_back(value); return clandestineMeets.back();
}

MotorsportClass& StudioAuthoringData::addMotorsportClass(const char* preferredName)
{
    MotorsportClass value; value.id = allocateId(); value.name = preferredName ? preferredName : "Open"; value.code = "OPEN";
    motorsportClasses.push_back(value); return motorsportClasses.back();
}

MotorsportEntrant& StudioAuthoringData::addMotorsportEntrant(const char* preferredName)
{
    MotorsportEntrant value; value.id = allocateId(); value.driverName = preferredName ? preferredName : "AI Driver";
    if (!motorsportClasses.empty()) value.classId = motorsportClasses.front().id;
    value.raceNumber = static_cast<int>(motorsportEntrants.size()) + 1;
    motorsportEntrants.push_back(value); return motorsportEntrants.back();
}

MotorsportChampionship& StudioAuthoringData::addMotorsportChampionship(const char* preferredName)
{
    MotorsportChampionship value; value.id = allocateId(); value.name = preferredName ? preferredName : "Championship";
    if (!motorsportClasses.empty()) value.classId = motorsportClasses.front().id;
    motorsportChampionships.push_back(value); return motorsportChampionships.back();
}

MotorsportRound& StudioAuthoringData::addMotorsportRound(std::uint32_t championshipId, const char* preferredName)
{
    MotorsportRound value; value.id = allocateId(); value.championshipId = championshipId; value.name = preferredName ? preferredName : "Round";
    int nextOrder = 0; for (const auto& round : motorsportRounds) if (round.championshipId == championshipId) nextOrder = std::max(nextOrder, round.order + 1); value.order = nextOrder;
    motorsportRounds.push_back(value); return motorsportRounds.back();
}

bool StudioAuthoringData::removeSceneObject(std::size_t index)
{
    if (index >= sceneObjects.size())
        return false;
    sceneObjects.erase(sceneObjects.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeRaceMarker(std::size_t index)
{
    if (index >= raceMarkers.size())
        return false;
    raceMarkers.erase(raceMarkers.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeBroadcastCameraPath(std::size_t index)
{
    if (index >= broadcastCameraPaths.size()) return false;
    const auto id = broadcastCameraPaths[index].id;
    broadcastCameraPaths.erase(broadcastCameraPaths.begin() + static_cast<std::ptrdiff_t>(index));
    broadcastCameraNodes.erase(std::remove_if(broadcastCameraNodes.begin(), broadcastCameraNodes.end(), [id](const BroadcastCameraNode& node) { return node.pathId == id; }), broadcastCameraNodes.end());
    return true;
}

bool StudioAuthoringData::removeBroadcastCameraNode(std::size_t index)
{
    if (index >= broadcastCameraNodes.size()) return false;
    broadcastCameraNodes.erase(broadcastCameraNodes.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeCourseCone(std::size_t index)
{
    if (index >= courseCones.size()) return false;
    const auto id = courseCones[index].id;
    courseCones.erase(courseCones.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto& gate : coneCourseGates)
    {
        if (gate.leftConeId == id) gate.leftConeId = 0;
        if (gate.rightConeId == id) gate.rightConeId = 0;
    }
    return true;
}

bool StudioAuthoringData::removeConeCourseGate(std::size_t index)
{
    if (index >= coneCourseGates.size()) return false;
    coneCourseGates.erase(coneCourseGates.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeTrafficNode(std::size_t index)
{
    if (index >= trafficNodes.size())
        return false;
    const std::uint32_t removedId = trafficNodes[index].id;
    trafficNodes.erase(trafficNodes.begin() + static_cast<std::ptrdiff_t>(index));
    trafficLinks.erase(std::remove_if(trafficLinks.begin(), trafficLinks.end(), [removedId](const TrafficLink& link)
    {
        return link.fromNodeId == removedId || link.toNodeId == removedId;
    }), trafficLinks.end());
    return true;
}

bool StudioAuthoringData::removeTrafficLink(std::size_t index)
{
    if (index >= trafficLinks.size())
        return false;
    const std::uint32_t removedId = trafficLinks[index].id;
    trafficLinks.erase(trafficLinks.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto& restriction : roadRestrictions) if (restriction.linkId == removedId) restriction.linkId = 0;
    return true;
}

bool StudioAuthoringData::removeRoadSpline(std::size_t index)
{
    if (index >= roadSplines.size()) return false;
    const auto id = roadSplines[index].id; roadSplines.erase(roadSplines.begin() + static_cast<std::ptrdiff_t>(index));
    roadSplineNodes.erase(std::remove_if(roadSplineNodes.begin(), roadSplineNodes.end(), [id](const RoadSplineNode& n){ return n.roadId == id; }), roadSplineNodes.end());
    turnConnectors.erase(std::remove_if(turnConnectors.begin(), turnConnectors.end(), [id](const TurnConnector& c){ return c.fromRoadId == id || c.toRoadId == id; }), turnConnectors.end());
    for (auto& p : parkingStrips) if (p.roadId == id) p.roadId = 0;
    for (auto& restriction : roadRestrictions) if (restriction.roadId == id) restriction.roadId = 0;
    return true;
}

bool StudioAuthoringData::removeRoadSplineNode(std::size_t index) { if (index >= roadSplineNodes.size()) return false; roadSplineNodes.erase(roadSplineNodes.begin() + static_cast<std::ptrdiff_t>(index)); return true; }

bool StudioAuthoringData::removeRoadIntersection(std::size_t index)
{
    if (index >= roadIntersections.size()) return false; const auto id = roadIntersections[index].id;
    roadIntersections.erase(roadIntersections.begin() + static_cast<std::ptrdiff_t>(index));
    turnConnectors.erase(std::remove_if(turnConnectors.begin(), turnConnectors.end(), [id](const TurnConnector& c){ return c.intersectionId == id; }), turnConnectors.end());
    trafficSignalPhases.erase(std::remove_if(trafficSignalPhases.begin(), trafficSignalPhases.end(), [id](const TrafficSignalPhase& p){ return p.intersectionId == id; }), trafficSignalPhases.end());
    intersectionControllers.erase(std::remove_if(intersectionControllers.begin(), intersectionControllers.end(), [id](const IntersectionController& c){ return c.intersectionId == id; }), intersectionControllers.end());
    return true;
}

bool StudioAuthoringData::removeTurnConnector(std::size_t index) { if (index >= turnConnectors.size()) return false; turnConnectors.erase(turnConnectors.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeTrafficSignalPhase(std::size_t index) { if (index >= trafficSignalPhases.size()) return false; trafficSignalPhases.erase(trafficSignalPhases.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeParkingStrip(std::size_t index) { if (index >= parkingStrips.size()) return false; parkingStrips.erase(parkingStrips.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeRoadRestriction(std::size_t index) { if (index >= roadRestrictions.size()) return false; roadRestrictions.erase(roadRestrictions.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeTrafficAgentProfile(std::size_t index) { if (index >= trafficAgentProfiles.size()) return false; trafficAgentProfiles.erase(trafficAgentProfiles.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeTrafficSpawnPortal(std::size_t index) { if (index >= trafficSpawnPortals.size()) return false; trafficSpawnPortals.erase(trafficSpawnPortals.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeTrafficDensityRegion(std::size_t index) { if (index >= trafficDensityRegions.size()) return false; trafficDensityRegions.erase(trafficDensityRegions.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::removeTrafficIncident(std::size_t index) { if (index >= trafficIncidents.size()) return false; trafficIncidents.erase(trafficIncidents.begin() + static_cast<std::ptrdiff_t>(index)); return true; }


void StudioAuthoringData::compileRoadSplinesToLaneGraph(int& createdNodes, int& updatedNodes, int& createdLinks)
{
    createdNodes = 0; updatedNodes = 0; createdLinks = 0;
    const auto normalized = [](const Vec3& value)
    {
        const float length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
        if (length <= 0.00001f) return Vec3{ 0.0f, 0.0f, 1.0f };
        return Vec3{ value.x / length, value.y / length, value.z / length };
    };
    const auto findNodeByName = [&](const std::string& name) -> TrafficNode*
    {
        for (auto& node : trafficNodes) if (node.name == name) return &node;
        return nullptr;
    };
    const auto ensureLink = [&](std::uint32_t from, std::uint32_t to, float speed, float density, TrafficLinkType type, float cost)
    {
        for (auto& link : trafficLinks)
        {
            if (link.fromNodeId != from || link.toNodeId != to) continue;
            if (!link.generated) return; // A manual edge with the same movement is an intentional override.
            link.speedLimitKmh = speed; link.density = density; link.type = type; link.routeCostMultiplier = cost;
            link.enabled = true; link.bidirectional = false; link.overtakingAllowed = type == TrafficLinkType::LaneChange;
            return;
        }
        auto& link = addTrafficLink(from, to);
        link.lanes = 1; link.speedLimitKmh = speed; link.bidirectional = false; link.density = density;
        link.type = type; link.routeCostMultiplier = cost; link.enabled = true; link.generated = true;
        link.overtakingAllowed = type == TrafficLinkType::LaneChange; ++createdLinks;
    };

    for (const auto& road : roadSplines)
    {
        if (!road.enabled) continue;
        std::vector<const RoadSplineNode*> points;
        for (const auto& node : roadSplineNodes) if (node.roadId == road.id) points.push_back(&node);
        std::stable_sort(points.begin(), points.end(), [](const auto* a, const auto* b){ return a->order < b->order; });
        if (points.size() < 2) continue;
        const int backwardCount = road.oneWay ? 0 : std::max(0, road.lanesBackward);
        for (int directionIndex = 0; directionIndex < 2; ++directionIndex)
        {
            const int direction = directionIndex == 0 ? 1 : -1;
            const int laneCount = direction > 0 ? std::max(0, road.lanesForward) : backwardCount;
            std::vector<std::vector<std::uint32_t>> laneNodeMatrix(static_cast<std::size_t>(laneCount));
            for (int lane = 1; lane <= laneCount; ++lane)
            {
                auto& laneNodeIds = laneNodeMatrix[static_cast<std::size_t>(lane - 1)];
                for (std::size_t pointIndex = 0; pointIndex < points.size(); ++pointIndex)
                {
                    const auto* point = points[pointIndex];
                    const auto& previous = points[pointIndex == 0 ? pointIndex : pointIndex - 1]->position;
                    const auto& next = points[pointIndex + 1 < points.size() ? pointIndex + 1 : pointIndex]->position;
                    const auto tangent = normalized({ next.x - previous.x, next.y - previous.y, next.z - previous.z });
                    const Vec3 side{ tangent.z, 0.0f, -tangent.x };
                    const float lateral = (road.medianWidthM * 0.5f + road.laneWidthM * (static_cast<float>(lane) - 0.5f)) * static_cast<float>(direction) * point->widthScale;
                    const Vec3 position{ point->position.x + side.x * lateral, point->position.y, point->position.z + side.z * lateral };
                    const std::string name = "AUTO_LANE_" + std::to_string(road.id) + (direction > 0 ? "_F" : "_B") + std::to_string(lane) + "_N" + std::to_string(point->order);
                    auto* graphNode = findNodeByName(name);
                    if (!graphNode) { graphNode = &addTrafficNode(TrafficNodeType::LaneNode, name.c_str()); ++createdNodes; }
                    else ++updatedNodes;
                    graphNode->type = TrafficNodeType::LaneNode; graphNode->position = position; graphNode->speedLimitKmh = road.speedLimitKmh;
                    graphNode->lanes = 1; graphNode->bidirectional = false; graphNode->overtakingAllowed = false; graphNode->density = road.trafficDensity;
                    graphNode->roadId = road.id; graphNode->laneIndex = lane; graphNode->laneDirection = direction; graphNode->generated = true;
                    laneNodeIds.push_back(graphNode->id);
                }
                if (direction > 0)
                    for (std::size_t i = 1; i < laneNodeIds.size(); ++i) ensureLink(laneNodeIds[i - 1], laneNodeIds[i], road.speedLimitKmh, road.trafficDensity, TrafficLinkType::Travel, 1.0f);
                else
                    for (std::size_t i = laneNodeIds.size(); i > 1; --i) ensureLink(laneNodeIds[i - 1], laneNodeIds[i - 2], road.speedLimitKmh, road.trafficDensity, TrafficLinkType::Travel, 1.0f);
            }

            // Adjacent lane-change edges are generated at internal control nodes. They are deliberately
            // semantic graph edges rather than fake longitudinal links, so routing/AI can price them separately.
            if (laneCount > 1)
            {
                for (int lane = 0; lane < laneCount - 1; ++lane)
                {
                    const auto& a = laneNodeMatrix[static_cast<std::size_t>(lane)];
                    const auto& b = laneNodeMatrix[static_cast<std::size_t>(lane + 1)];
                    const std::size_t count = std::min(a.size(), b.size());
                    for (std::size_t i = 1; i + 1 < count; ++i)
                    {
                        ensureLink(a[i], b[i], road.speedLimitKmh, road.trafficDensity, TrafficLinkType::LaneChange, trafficRules.laneChangeRouteCost);
                        ensureLink(b[i], a[i], road.speedLimitKmh, road.trafficDensity, TrafficLinkType::LaneChange, trafficRules.laneChangeRouteCost);
                    }
                }
            }
        }
    }
}

bool StudioAuthoringData::removeRaceRoute(std::size_t index)
{
    if (index >= raceRoutes.size())
        return false;
    const std::uint32_t removedId = raceRoutes[index].id;
    raceRoutes.erase(raceRoutes.begin() + static_cast<std::ptrdiff_t>(index));
    raceRouteNodes.erase(std::remove_if(raceRouteNodes.begin(), raceRouteNodes.end(), [removedId](const RaceRouteNode& node)
    {
        return node.routeId == removedId;
    }), raceRouteNodes.end());
    for (auto& layout : raceLayouts)
    {
        if (layout.routeId == removedId) layout.routeId = 0;
        if (layout.pitRouteId == removedId) layout.pitRouteId = 0;
    }
    if (raceControl.safetyCarRouteId == removedId) raceControl.safetyCarRouteId = 0;
    return true;
}

bool StudioAuthoringData::removeRaceRouteNode(std::size_t index)
{
    if (index >= raceRouteNodes.size())
        return false;
    raceRouteNodes.erase(raceRouteNodes.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeRaceLayout(std::size_t index)
{
    if (index >= raceLayouts.size()) return false;
    raceLayouts.erase(raceLayouts.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeRaceSession(std::size_t index)
{
    if (index >= raceSessions.size()) return false;
    raceSessions.erase(raceSessions.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeRaceSupportPoint(std::size_t index)
{
    if (index >= raceSupportPoints.size()) return false;
    raceSupportPoints.erase(raceSupportPoints.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeGameEvent(std::size_t index)
{
    if (index >= gameEvents.size())
        return false;
    gameEvents.erase(gameEvents.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removeWorldPoint(std::size_t index)
{
    if (index >= worldPoints.size())
        return false;
    worldPoints.erase(worldPoints.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool StudioAuthoringData::removePolicePatrolZone(std::size_t index)
{
    if (index >= policePatrolZones.size()) return false;
    policePatrolZones.erase(policePatrolZones.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}

bool StudioAuthoringData::removePoliceRoadblockSite(std::size_t index)
{
    if (index >= policeRoadblockSites.size()) return false;
    policeRoadblockSites.erase(policeRoadblockSites.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}

bool StudioAuthoringData::removePoliceEscapeZone(std::size_t index)
{
    if (index >= policeEscapeZones.size()) return false;
    policeEscapeZones.erase(policeEscapeZones.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}

bool StudioAuthoringData::removeClandestineMeet(std::size_t index)
{
    if (index >= clandestineMeets.size()) return false;
    clandestineMeets.erase(clandestineMeets.begin() + static_cast<std::ptrdiff_t>(index)); return true;
}


bool StudioAuthoringData::removeMotorsportClass(std::size_t index)
{
    if (index >= motorsportClasses.size()) return false;
    const auto id = motorsportClasses[index].id; motorsportClasses.erase(motorsportClasses.begin() + static_cast<std::ptrdiff_t>(index));
    for (auto& entrant : motorsportEntrants) if (entrant.classId == id) entrant.classId = 0;
    for (auto& championship : motorsportChampionships) if (championship.classId == id) championship.classId = 0;
    return true;
}

bool StudioAuthoringData::removeMotorsportEntrant(std::size_t index)
{ if (index >= motorsportEntrants.size()) return false; motorsportEntrants.erase(motorsportEntrants.begin() + static_cast<std::ptrdiff_t>(index)); return true; }

bool StudioAuthoringData::removeMotorsportChampionship(std::size_t index)
{
    if (index >= motorsportChampionships.size()) return false; const auto id = motorsportChampionships[index].id;
    motorsportChampionships.erase(motorsportChampionships.begin() + static_cast<std::ptrdiff_t>(index));
    motorsportRounds.erase(std::remove_if(motorsportRounds.begin(), motorsportRounds.end(), [id](const MotorsportRound& round){ return round.championshipId == id; }), motorsportRounds.end());
    return true;
}

bool StudioAuthoringData::removeMotorsportRound(std::size_t index)
{ if (index >= motorsportRounds.size()) return false; motorsportRounds.erase(motorsportRounds.begin() + static_cast<std::ptrdiff_t>(index)); return true; }
bool StudioAuthoringData::saveAll(const std::filesystem::path& root, std::string& message) const
{
    std::string local;
    if (!saveScene(root / "scene.hscene", local) ||
        !saveRace(root / "race.hrace", local) ||
        !saveTraffic(root / "traffic.hroad", local) ||
        !saveWeather(root / "weather.hweather", local) ||
        !saveVehicle(root / "vehicle.hvehicleauthor", local) ||
        !saveGameplay(root / "gameplay.hgame", local))
    {
        message = local;
        return false;
    }
    message = "Saved Studio authoring set to " + root.string();
    return true;
}

bool StudioAuthoringData::loadAll(const std::filesystem::path& root, std::string& message)
{
    bool loadedAnything = false;
    std::string local;
    if (std::filesystem::exists(root / "scene.hscene")) { loadedAnything |= loadScene(root / "scene.hscene", local); }
    if (std::filesystem::exists(root / "race.hrace")) { loadedAnything |= loadRace(root / "race.hrace", local); }
    if (std::filesystem::exists(root / "traffic.hroad")) { loadedAnything |= loadTraffic(root / "traffic.hroad", local); }
    if (std::filesystem::exists(root / "weather.hweather")) { loadedAnything |= loadWeather(root / "weather.hweather", local); }
    if (std::filesystem::exists(root / "vehicle.hvehicleauthor")) { loadedAnything |= loadVehicle(root / "vehicle.hvehicleauthor", local); }
    if (std::filesystem::exists(root / "gameplay.hgame")) { loadedAnything |= loadGameplay(root / "gameplay.hgame", local); }
    message = loadedAnything ? "Loaded Studio authoring set from " + root.string() : "No Studio authoring files exist yet.";
    return loadedAnything;
}

bool StudioAuthoringData::saveScene(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    out << "HSCENE 1\n";
    for (const auto& item : sceneObjects)
    {
        out << "OBJECT " << item.id << ' ' << static_cast<int>(item.type) << ' ' << (item.enabled ? 1 : 0) << ' '
            << quote(item.name) << ' ' << quote(item.tag) << ' ' << quote(item.assetPath) << ' ';
        writeVec3(out, item.position); out << ' '; writeVec3(out, item.rotation); out << ' '; writeVec3(out, item.scale); out << '\n';
    }
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadScene(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HSCENE") { message = "Invalid HSCENE file."; return false; }
    sceneObjects.clear();
    std::string keyword;
    std::uint32_t maxId = 0;
    while (in >> keyword)
    {
        if (keyword != "OBJECT") { std::string ignore; std::getline(in, ignore); continue; }
        SceneObject item; int type = 0; int enabled = 1;
        if (!(in >> item.id >> type >> enabled >> std::quoted(item.name) >> std::quoted(item.tag) >> std::quoted(item.assetPath))) break;
        if (!readVec3(in, item.position) || !readVec3(in, item.rotation) || !readVec3(in, item.scale)) break;
        item.type = clampEnum<SceneObjectType>(type, static_cast<int>(SceneObjectType::Trigger));
        item.enabled = enabled != 0;
        maxId = std::max(maxId, item.id);
        sceneObjects.push_back(item);
    }
    m_nextId = std::max(m_nextId, maxId + 1);
    message = "Loaded " + file.string();
    return true;
}

// Backward compatibility note: HRACE 1 and HRACE 2 files remain accepted.
// HRACE 7 adds STUDIO28 semantic/physical cone courses while HRACE 1-6 remain readable.
// HRACE 6 adds STUDIO27 authored moving broadcast-camera paths while HRACE 1-5 remain readable.
// HRACE 5 keeps the STUDIO18 layout-scoped venue package and adds complete motorsport weekend/session rules.
// HRACE 4 keeps the STUDIO11 venue/race package and adds optional layout
// scoping to race markers so alternate circuit/street layouts can own independent
// checkpoint, grid, pit and timing-gate sets. HRACE 1/2/3 remain accepted.
bool StudioAuthoringData::saveRace(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    out << "HRACE 7\n";
    out << "CONFIG " << race.laps << ' ' << race.gridSlots << ' ' << race.pitSpeedKmh << ' '
        << (race.formationLap ? 1 : 0) << ' ' << (race.standingStart ? 1 : 0) << ' '
        << (race.falseStartPenalty ? 1 : 0) << ' ' << (race.trackLimitsEnabled ? 1 : 0) << ' '
        << (race.penaltiesEnabled ? 1 : 0) << ' ' << static_cast<int>(race.gridTemplate) << ' '
        << race.gridRowSpacingM << ' ' << race.gridLateralSpacingM << ' ' << race.gridBackOffsetM << '\n';
    for (const auto& item : raceMarkers)
    {
        out << "MARKER " << item.id << ' ' << static_cast<int>(item.type) << ' ' << item.order << ' ' << item.slot << ' '
            << item.headingDeg << ' ' << item.radiusM << ' ' << item.speedLimitKmh << ' '
            << item.gateWidthM << ' ' << item.gateHeightM << ' ' << (item.directionRequired ? 1 : 0) << ' '
            << item.layoutId << ' ' << quote(item.name) << ' ';
        writeVec3(out, item.position); out << '\n';
    }
    for (const auto& route : raceRoutes)
    {
        out << "ROUTE " << route.id << ' ' << static_cast<int>(route.type) << ' ' << (route.enabled ? 1 : 0) << ' '
            << (route.closedLoop ? 1 : 0) << ' ' << (route.reverseAllowed ? 1 : 0) << ' '
            << route.defaultLeftWidthM << ' ' << route.defaultRightWidthM << ' ' << quote(route.name) << '\n';
    }
    for (const auto& node : raceRouteNodes)
    {
        out << "ROUTE_NODE " << node.id << ' ' << node.routeId << ' ' << node.order << ' '
            << (node.automaticTangents ? 1 : 0) << ' ' << node.leftWidthM << ' ' << node.rightWidthM << ' '
            << node.targetSpeedKmh << ' ' << node.bankingDeg << ' ' << (node.overtakingPreferred ? 1 : 0) << ' ';
        writeVec3(out, node.position); out << ' '; writeVec3(out, node.handleIn); out << ' '; writeVec3(out, node.handleOut); out << '\n';
    }
    for (const auto& layout : raceLayouts)
    {
        out << "LAYOUT " << layout.id << ' ' << (layout.enabled ? 1 : 0) << ' '
            << layout.routeId << ' ' << layout.pitRouteId << ' ' << layout.startFinishMarkerId << ' '
            << layout.defaultLaps << ' ' << (layout.reverse ? 1 : 0) << ' ' << (layout.pitsEnabled ? 1 : 0) << ' '
            << quote(layout.name) << '\n';
    }
    for (const auto& session : raceSessions)
    {
        out << "SESSION " << session.id << ' ' << static_cast<int>(session.type) << ' ' << (session.enabled ? 1 : 0) << ' '
            << session.order << ' ' << session.durationMinutes << ' ' << session.laps << ' ' << session.mandatoryPitStops << ' '
            << (session.formationLap ? 1 : 0) << ' ' << (session.rollingStart ? 1 : 0) << ' '
            << (session.weatherChangeAllowed ? 1 : 0) << ' ' << session.startingFuelPercent << ' ' << (session.timedRace ? 1 : 0) << ' '
            << (session.timePlusOneLap ? 1 : 0) << ' ' << session.maximumStintMinutes << ' ' << (session.refuelingAllowed ? 1 : 0) << ' '
            << (session.tireChangesAllowed ? 1 : 0) << ' ' << (session.mandatoryTireChange ? 1 : 0) << ' ' << session.minimumPitServiceSeconds << ' '
            << session.classificationPercent << ' ' << static_cast<int>(session.gridSource) << ' ' << session.reverseTopN << ' ' << quote(session.name) << '\n';
    }
    out << "CONTROL " << (raceControl.localYellow ? 1 : 0) << ' ' << (raceControl.fullCourseYellow ? 1 : 0) << ' '
        << (raceControl.virtualSafetyCar ? 1 : 0) << ' ' << (raceControl.safetyCar ? 1 : 0) << ' '
        << (raceControl.redFlag ? 1 : 0) << ' ' << (raceControl.blueFlags ? 1 : 0) << ' '
        << (raceControl.pitLaneOpenDuringSafetyCar ? 1 : 0) << ' ' << raceControl.maxTrackLimitWarnings << ' '
        << raceControl.driveThroughAfterWarnings << ' ' << raceControl.pitWindowStartLap << ' ' << raceControl.pitWindowEndLap << ' '
        << raceControl.safetyCarRouteId << ' ' << raceControl.restartMarkerId << '\n';
    for (const auto& point : raceSupportPoints)
    {
        out << "SUPPORT " << point.id << ' ' << static_cast<int>(point.type) << ' ' << (point.enabled ? 1 : 0) << ' '
            << point.headingDeg << ' ' << point.serviceRadiusM << ' ' << point.sector << ' ' << quote(point.name) << ' ';
        writeVec3(out, point.position); out << '\n';
    }
    for (const auto& path : broadcastCameraPaths)
    {
        out << "CAMERA_PATH " << path.id << ' ' << static_cast<int>(path.type) << ' ' << (path.enabled ? 1 : 0) << ' '
            << path.layoutId << ' ' << path.activationRadiusM << ' ' << path.durationSeconds << ' ' << path.easing << ' '
            << (path.reverse ? 1 : 0) << ' ' << quote(path.name) << '\n';
    }
    for (const auto& node : broadcastCameraNodes)
    {
        out << "CAMERA_NODE " << node.id << ' ' << node.pathId << ' ' << node.order << ' ';
        writeVec3(out, node.position); out << '\n';
    }
    out << "CONE_CONFIG " << (coneCourse.enabled ? 1 : 0) << ' ' << quote(coneCourse.defaultAssetPath) << ' '
        << coneCourse.minimumContactImpulseNs << ' ' << coneCourse.defaultHitPenaltySeconds << ' '
        << coneCourse.defaultDisplacementToleranceM << ' ' << coneCourse.wrongElementPenaltySeconds << ' '
        << (coneCourse.missedElementDnf ? 1 : 0) << ' ' << (coneCourse.resetEventConesOnStart ? 1 : 0) << ' '
        << (coneCourse.recordConeHitsToReplay ? 1 : 0) << ' ' << (coneCourse.eventConesVisibleOnlyWhileActive ? 1 : 0) << '\n';
    for (const auto& cone : courseCones)
    {
        out << "CONE " << cone.id << ' ' << static_cast<int>(cone.role) << ' ' << (cone.enabled ? 1 : 0) << ' ' << cone.eventId << ' '
            << cone.headingDeg << ' ' << quote(cone.assetPath) << ' ' << cone.visualScale << ' ' << cone.baseRadiusM << ' ' << cone.heightM << ' '
            << cone.massKg << ' ' << cone.friction << ' ' << cone.restitution << ' ' << (cone.physical ? 1 : 0) << ' '
            << static_cast<int>(cone.penaltyMode) << ' ' << cone.hitPenaltySeconds << ' ' << cone.displacementToleranceM << ' '
            << static_cast<int>(cone.trafficMode) << ' ' << cone.roadId << ' ' << cone.linkId << ' ' << cone.laneIndex << ' '
            << cone.trafficSpeedLimitKmh << ' ' << cone.routeCostMultiplier << ' ' << quote(cone.name) << ' ';
        writeVec3(out, cone.position); out << '\n';
    }
    for (const auto& gate : coneCourseGates)
    {
        out << "CONE_GATE " << gate.id << ' ' << static_cast<int>(gate.type) << ' ' << (gate.enabled ? 1 : 0) << ' ' << gate.eventId << ' '
            << gate.order << ' ' << gate.headingDeg << ' ' << gate.widthM << ' ' << gate.lengthM << ' ' << (gate.directionRequired ? 1 : 0) << ' '
            << gate.sideClearanceM << ' ' << gate.stopSpeedKmh << ' ' << gate.stopDwellS << ' ' << gate.wrongElementPenaltySeconds << ' '
            << (gate.dnfOnMiss ? 1 : 0) << ' ' << gate.leftConeId << ' ' << gate.rightConeId << ' ' << quote(gate.name) << ' ';
        writeVec3(out, gate.position); out << '\n';
    }
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadRace(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HRACE") { message = "Invalid HRACE file."; return false; }
    raceMarkers.clear();
    raceRoutes.clear();
    raceRouteNodes.clear();
    raceLayouts.clear();
    raceSessions.clear();
    raceSupportPoints.clear();
    broadcastCameraPaths.clear();
    broadcastCameraNodes.clear();
    courseCones.clear();
    coneCourseGates.clear();
    coneCourse = ConeCourseConfiguration{};
    race = RaceConfiguration{};
    raceControl = RaceControlConfiguration{};
    std::string keyword; std::uint32_t maxId = 0;
    while (in >> keyword)
    {
        if (keyword == "CONFIG")
        {
            int formation = 0, standing = 1, falseStart = 1, limits = 1, penalties = 1;
            if (!(in >> race.laps >> race.gridSlots >> race.pitSpeedKmh >> formation >> standing >> falseStart >> limits >> penalties)) break;
            race.formationLap = formation != 0;
            race.standingStart = standing != 0;
            race.falseStartPenalty = falseStart != 0;
            race.trackLimitsEnabled = limits != 0;
            race.penaltiesEnabled = penalties != 0;
            if (version >= 3)
            {
                int gridTemplate = 0;
                if (!(in >> gridTemplate >> race.gridRowSpacingM >> race.gridLateralSpacingM >> race.gridBackOffsetM)) break;
                race.gridTemplate = clampEnum<GridTemplate>(gridTemplate, static_cast<int>(GridTemplate::EnduranceAngled));
            }
            continue;
        }
        if (keyword == "MARKER")
        {
            RaceMarker item; int type = 0;
            if (!(in >> item.id >> type >> item.order >> item.slot >> item.headingDeg >> item.radiusM >> item.speedLimitKmh)) break;
            if (version >= 3)
            {
                int direction = 1;
                if (!(in >> item.gateWidthM >> item.gateHeightM >> direction)) break;
                item.directionRequired = direction != 0;
                if (version >= 4)
                {
                    if (!(in >> item.layoutId >> std::quoted(item.name))) break;
                }
                else
                {
                    item.layoutId = 0;
                    if (!(in >> std::quoted(item.name))) break;
                }
            }
            else
            {
                if (!(in >> std::quoted(item.name))) break;
            }
            if (!readVec3(in, item.position)) break;
            item.type = clampEnum<RaceMarkerType>(type, version >= 3 ? static_cast<int>(RaceMarkerType::FormationLine) : static_cast<int>(RaceMarkerType::WetLineNode));
            maxId = std::max(maxId, item.id);
            raceMarkers.push_back(item);
            continue;
        }
        if (version >= 3 && keyword == "ROUTE")
        {
            RaceRoute route; int type = 0, enabled = 1, closed = 1, reverse = 0;
            if (!(in >> route.id >> type >> enabled >> closed >> reverse >> route.defaultLeftWidthM >> route.defaultRightWidthM >> std::quoted(route.name))) break;
            route.type = clampEnum<RaceRouteType>(type, static_cast<int>(RaceRouteType::Drag));
            route.enabled = enabled != 0; route.closedLoop = closed != 0; route.reverseAllowed = reverse != 0;
            maxId = std::max(maxId, route.id); raceRoutes.push_back(route); continue;
        }
        if (version >= 3 && keyword == "ROUTE_NODE")
        {
            RaceRouteNode node; int automatic = 1, overtake = 0;
            if (!(in >> node.id >> node.routeId >> node.order >> automatic >> node.leftWidthM >> node.rightWidthM
                >> node.targetSpeedKmh >> node.bankingDeg >> overtake)) break;
            if (!readVec3(in, node.position) || !readVec3(in, node.handleIn) || !readVec3(in, node.handleOut)) break;
            node.automaticTangents = automatic != 0; node.overtakingPreferred = overtake != 0;
            maxId = std::max(maxId, node.id); raceRouteNodes.push_back(node); continue;
        }
        if (version >= 3 && keyword == "LAYOUT")
        {
            RaceLayout layout; int enabled = 1, reverse = 0, pits = 1;
            if (!(in >> layout.id >> enabled >> layout.routeId >> layout.pitRouteId >> layout.startFinishMarkerId
                >> layout.defaultLaps >> reverse >> pits >> std::quoted(layout.name))) break;
            layout.enabled = enabled != 0; layout.reverse = reverse != 0; layout.pitsEnabled = pits != 0;
            maxId = std::max(maxId, layout.id); raceLayouts.push_back(layout); continue;
        }
        if (version >= 3 && keyword == "SESSION")
        {
            RaceSession session; int type = 0, enabled = 1, formation = 0, rolling = 0, weather = 1;
            if (!(in >> session.id >> type >> enabled >> session.order >> session.durationMinutes >> session.laps >> session.mandatoryPitStops
                >> formation >> rolling >> weather >> session.startingFuelPercent)) break;
            if (version >= 5)
            {
                int timed=0, plusOne=0, refuel=1, tires=1, mandatoryTires=0, gridSource=0;
                if (!(in >> timed >> plusOne >> session.maximumStintMinutes >> refuel >> tires >> mandatoryTires >> session.minimumPitServiceSeconds
                    >> session.classificationPercent >> gridSource >> session.reverseTopN >> std::quoted(session.name))) break;
                session.timedRace = timed != 0; session.timePlusOneLap = plusOne != 0; session.refuelingAllowed = refuel != 0;
                session.tireChangesAllowed = tires != 0; session.mandatoryTireChange = mandatoryTires != 0;
                session.gridSource = clampEnum<SessionGridSource>(gridSource, static_cast<int>(SessionGridSource::ReverseTopN));
            }
            else if (!(in >> std::quoted(session.name))) break;
            session.type = clampEnum<RaceSessionType>(type, static_cast<int>(RaceSessionType::TestSession));
            session.enabled = enabled != 0; session.formationLap = formation != 0; session.rollingStart = rolling != 0;
            session.weatherChangeAllowed = weather != 0; maxId = std::max(maxId, session.id); raceSessions.push_back(session); continue;
        }
        if (version >= 3 && keyword == "CONTROL")
        {
            int local = 1, fcy = 1, vsc = 1, sc = 1, red = 1, blue = 1, pitOpen = 1;
            if (!(in >> local >> fcy >> vsc >> sc >> red >> blue >> pitOpen >> raceControl.maxTrackLimitWarnings
                >> raceControl.driveThroughAfterWarnings >> raceControl.pitWindowStartLap >> raceControl.pitWindowEndLap
                >> raceControl.safetyCarRouteId >> raceControl.restartMarkerId)) break;
            raceControl.localYellow = local != 0; raceControl.fullCourseYellow = fcy != 0; raceControl.virtualSafetyCar = vsc != 0;
            raceControl.safetyCar = sc != 0; raceControl.redFlag = red != 0; raceControl.blueFlags = blue != 0;
            raceControl.pitLaneOpenDuringSafetyCar = pitOpen != 0; continue;
        }
        if (version >= 3 && keyword == "SUPPORT")
        {
            RaceSupportPoint point; int type = 0, enabled = 1;
            if (!(in >> point.id >> type >> enabled >> point.headingDeg >> point.serviceRadiusM >> point.sector >> std::quoted(point.name))) break;
            if (!readVec3(in, point.position)) break;
            point.type = clampEnum<RaceSupportPointType>(type, static_cast<int>(RaceSupportPointType::TimingEquipment));
            point.enabled = enabled != 0; maxId = std::max(maxId, point.id); raceSupportPoints.push_back(point); continue;
        }
        if (version >= 6 && keyword == "CAMERA_PATH")
        {
            BroadcastCameraPath path; int type = 0, enabled = 1, reverse = 0;
            if (!(in >> path.id >> type >> enabled >> path.layoutId >> path.activationRadiusM >> path.durationSeconds >> path.easing >> reverse >> std::quoted(path.name))) break;
            path.type = clampEnum<BroadcastCameraPathType>(type, static_cast<int>(BroadcastCameraPathType::Drone));
            path.enabled = enabled != 0; path.reverse = reverse != 0;
            maxId = std::max(maxId, path.id); broadcastCameraPaths.push_back(path); continue;
        }
        if (version >= 6 && keyword == "CAMERA_NODE")
        {
            BroadcastCameraNode node;
            if (!(in >> node.id >> node.pathId >> node.order)) break;
            if (!readVec3(in, node.position)) break;
            maxId = std::max(maxId, node.id); broadcastCameraNodes.push_back(node); continue;
        }
        if (version >= 7 && keyword == "CONE_CONFIG")
        {
            int enabled=1, missedDnf=1, reset=1, replay=1, activeOnly=1;
            if (!(in >> enabled >> std::quoted(coneCourse.defaultAssetPath) >> coneCourse.minimumContactImpulseNs
                >> coneCourse.defaultHitPenaltySeconds >> coneCourse.defaultDisplacementToleranceM >> coneCourse.wrongElementPenaltySeconds
                >> missedDnf >> reset >> replay >> activeOnly)) break;
            coneCourse.enabled=enabled!=0; coneCourse.missedElementDnf=missedDnf!=0; coneCourse.resetEventConesOnStart=reset!=0;
            coneCourse.recordConeHitsToReplay=replay!=0; coneCourse.eventConesVisibleOnlyWhileActive=activeOnly!=0; continue;
        }
        if (version >= 7 && keyword == "CONE")
        {
            CourseCone cone; int role=0, enabled=1, physical=1, penalty=0, traffic=0;
            if (!(in >> cone.id >> role >> enabled >> cone.eventId >> cone.headingDeg >> std::quoted(cone.assetPath) >> cone.visualScale
                >> cone.baseRadiusM >> cone.heightM >> cone.massKg >> cone.friction >> cone.restitution >> physical >> penalty
                >> cone.hitPenaltySeconds >> cone.displacementToleranceM >> traffic >> cone.roadId >> cone.linkId >> cone.laneIndex
                >> cone.trafficSpeedLimitKmh >> cone.routeCostMultiplier >> std::quoted(cone.name))) break;
            if (!readVec3(in, cone.position)) break;
            cone.role=clampEnum<ConeRole>(role, static_cast<int>(ConeRole::RoadClosure)); cone.enabled=enabled!=0; cone.physical=physical!=0;
            cone.penaltyMode=clampEnum<ConePenaltyMode>(penalty, static_cast<int>(ConePenaltyMode::KnockedDown));
            cone.trafficMode=clampEnum<ConeTrafficMode>(traffic, static_cast<int>(ConeTrafficMode::CloseRoad));
            maxId=std::max(maxId,cone.id); courseCones.push_back(cone); continue;
        }
        if (version >= 7 && keyword == "CONE_GATE")
        {
            ConeCourseGate gate; int type=0, enabled=1, direction=1, dnf=1;
            if (!(in >> gate.id >> type >> enabled >> gate.eventId >> gate.order >> gate.headingDeg >> gate.widthM >> gate.lengthM >> direction
                >> gate.sideClearanceM >> gate.stopSpeedKmh >> gate.stopDwellS >> gate.wrongElementPenaltySeconds >> dnf
                >> gate.leftConeId >> gate.rightConeId >> std::quoted(gate.name))) break;
            if (!readVec3(in, gate.position)) break;
            gate.type=clampEnum<ConeCourseGateType>(type, static_cast<int>(ConeCourseGateType::CircleRight)); gate.enabled=enabled!=0;
            gate.directionRequired=direction!=0; gate.dnfOnMiss=dnf!=0; maxId=std::max(maxId,gate.id); coneCourseGates.push_back(gate); continue;
        }
        std::string ignore; std::getline(in, ignore);
    }
    m_nextId = std::max(m_nextId, maxId + 1);
    if (version < 3 && raceRoutes.empty())
    {
        auto& route = addRaceRoute(RaceRouteType::MainCircuit, "Main Circuit");
        auto& layout = addRaceLayout("Grand Prix");
        layout.routeId = route.id;
        layout.defaultLaps = race.laps;
        for (const auto& marker : raceMarkers)
            if (marker.type == RaceMarkerType::StartFinish) { layout.startFinishMarkerId = marker.id; break; }
    }
    message = "Loaded " + file.string() + " (HRACE v" + std::to_string(version) + ")";
    return true;
}

// Backward compatibility note: HROAD 1/2/3/4/5/6 files remain accepted by loadTraffic;
// HROAD 7 adds advanced merge/roundabout/priority/overtake/parking/recovery/collision behavior policy on top of STUDIO15.
bool StudioAuthoringData::saveTraffic(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    out << "HROAD 7\n";
    for (const auto& item : trafficNodes)
    {
        out << "NODE " << item.id << ' ' << static_cast<int>(item.type) << ' ' << item.headingDeg << ' '
            << item.speedLimitKmh << ' ' << item.lanes << ' ' << item.priority << ' '
            << (item.bidirectional ? 1 : 0) << ' ' << (item.overtakingAllowed ? 1 : 0) << ' '
            << item.density << ' ' << item.roadId << ' ' << item.laneIndex << ' ' << item.laneDirection << ' ' << (item.generated ? 1 : 0) << ' '
            << quote(item.name) << ' ';
        writeVec3(out, item.position); out << '\n';
    }
    for (const auto& link : trafficLinks)
    {
        out << "LINK " << link.id << ' ' << link.fromNodeId << ' ' << link.toNodeId << ' '
            << link.lanes << ' ' << link.speedLimitKmh << ' ' << (link.bidirectional ? 1 : 0) << ' '
            << (link.overtakingAllowed ? 1 : 0) << ' ' << link.density << ' ' << static_cast<int>(link.type) << ' '
            << link.routeCostMultiplier << ' ' << (link.enabled ? 1 : 0) << ' ' << (link.generated ? 1 : 0) << '\n';
    }
    for (const auto& road : roadSplines)
    {
        out << "ROAD " << road.id << ' ' << static_cast<int>(road.roadClass) << ' ' << (road.enabled ? 1 : 0) << ' '
            << (road.oneWay ? 1 : 0) << ' ' << road.lanesForward << ' ' << road.lanesBackward << ' ' << road.laneWidthM << ' '
            << road.shoulderLeftM << ' ' << road.shoulderRightM << ' ' << road.medianWidthM << ' ' << road.speedLimitKmh << ' '
            << (road.sidewalkLeft ? 1 : 0) << ' ' << (road.sidewalkRight ? 1 : 0) << ' '
            << (road.parkingLeft ? 1 : 0) << ' ' << (road.parkingRight ? 1 : 0) << ' '
            << road.trafficDensity << ' ' << road.spawnWeight << ' ' << quote(road.name) << '\n';
    }
    for (const auto& node : roadSplineNodes)
    {
        out << "ROADNODE " << node.id << ' ' << node.roadId << ' ' << node.order << ' ' << (node.automaticTangents ? 1 : 0)
            << ' ' << node.widthScale << ' ' << node.bankingDeg << ' ';
        writeVec3(out, node.position); out << ' '; writeVec3(out, node.handleIn); out << ' '; writeVec3(out, node.handleOut); out << '\n';
    }
    for (const auto& junction : roadIntersections)
    {
        out << "JUNCTION " << junction.id << ' ' << static_cast<int>(junction.priority) << ' ' << junction.radiusM << ' '
            << (junction.trafficLights ? 1 : 0) << ' ' << (junction.pedestrianCrossing ? 1 : 0) << ' ' << junction.approachSpeedKmh
            << ' ' << quote(junction.name) << ' '; writeVec3(out, junction.position); out << '\n';
    }
    for (const auto& connector : turnConnectors)
    {
        out << "TURN " << connector.id << ' ' << connector.intersectionId << ' ' << connector.fromRoadId << ' ' << connector.toRoadId << ' '
            << connector.fromLane << ' ' << connector.toLane << ' ' << (connector.enabled ? 1 : 0) << ' ' << (connector.yield ? 1 : 0)
            << ' ' << (connector.uTurn ? 1 : 0) << ' ' << connector.speedLimitKmh << ' ' << connector.conflictGroup << ' ' << connector.reservationSeconds << '\n';
    }
    for (const auto& phase : trafficSignalPhases)
    {
        out << "SIGNALPHASE " << phase.id << ' ' << phase.intersectionId << ' ' << phase.order << ' ' << phase.greenSeconds << ' '
            << phase.yellowSeconds << ' ' << phase.allRedSeconds << ' ' << quote(phase.name) << ' ' << quote(phase.connectorIds) << '\n';
    }
    for (const auto& parking : parkingStrips)
    {
        out << "PARKINGSTRIP " << parking.id << ' ' << parking.roadId << ' ' << parking.headingDeg << ' ' << parking.spaces << ' '
            << parking.spacingM << ' ' << parking.angleDeg << ' ' << (parking.rightSide ? 1 : 0) << ' ' << parking.occupancy << ' '
            << quote(parking.name) << ' '; writeVec3(out, parking.position); out << '\n';
    }
    out << "POPULATION " << trafficPopulation.globalDensity << ' ' << trafficPopulation.parkedDensity << ' ' << trafficPopulation.rushHourMultiplier
        << ' ' << trafficPopulation.nightMultiplier << ' ' << trafficPopulation.heavyVehicleShare << ' ' << trafficPopulation.motorcycleShare
        << ' ' << trafficPopulation.commercialShare << ' ' << trafficPopulation.emergencyShare << ' ' << trafficPopulation.laneChangeAggression
        << ' ' << trafficPopulation.speedVariance << ' ' << trafficPopulation.maxActiveVehicles << '\n';
    out << "NAVBUILD " << (navigationBuild.enabled ? 1 : 0) << ' ' << (navigationBuild.rebuildOnSave ? 1 : 0) << ' '
        << navigationBuild.maxSlopeDeg << ' ' << navigationBuild.minimumTurnRadiusM << ' ' << navigationBuild.laneChangeLengthM << ' '
        << navigationBuild.junctionLookaheadM << ' ' << navigationBuild.mergeLookaheadM << '\n';
    out << "TRAFFICRULES " << static_cast<int>(trafficRules.drivingSide) << ' ' << (trafficRules.keepToDrivingSide ? 1 : 0) << ' '
        << (trafficRules.allowTurnOnRed ? 1 : 0) << ' ' << (trafficRules.emergencyCorridor ? 1 : 0) << ' '
        << trafficRules.desiredTimeGapS << ' ' << trafficRules.minimumGapM << ' ' << trafficRules.desiredAccelerationMps2 << ' '
        << trafficRules.comfortableBrakingMps2 << ' ' << trafficRules.laneChangeCooldownS << ' ' << trafficRules.laneChangeMinimumGapM << ' '
        << trafficRules.laneChangeRouteCost << ' ' << trafficRules.mergeRouteCost << ' ' << trafficRules.emergencyYieldRadiusM << ' '
        << trafficRules.roundaboutYieldDistanceM << '\n';
    out << "STREAMING " << trafficStreaming.fullSimulationRadiusM << ' ' << trafficStreaming.simplifiedSimulationRadiusM << ' '
        << trafficStreaming.dormantPersistenceRadiusM << ' ' << trafficStreaming.sectorSizeM << ' ' << trafficStreaming.maxSpawnsPerSecond << ' '
        << trafficStreaming.maxDespawnsPerSecond << ' ' << trafficStreaming.despawnBehindDistanceM << ' ' << (trafficStreaming.retainDormantState ? 1 : 0) << ' '
        << trafficStreaming.dormantStateMinutes << '\n';
    for (const auto& control : intersectionControllers)
        out << "INTERSECTIONCTRL " << control.intersectionId << ' ' << static_cast<int>(control.mode) << ' ' << control.phaseOffsetSeconds << ' '
            << control.minimumGreenSeconds << ' ' << control.maximumGreenSeconds << ' ' << control.detectorDistanceM << ' ' << control.gapOutSeconds << ' '
            << (control.queueAdaptive ? 1 : 0) << ' ' << (control.emergencyPreemption ? 1 : 0) << '\n';
    for (const auto& restriction : roadRestrictions)
        out << "RESTRICTION " << restriction.id << ' ' << static_cast<int>(restriction.type) << ' ' << (restriction.enabled ? 1 : 0) << ' '
            << restriction.roadId << ' ' << restriction.linkId << ' ' << (restriction.blockTraffic ? 1 : 0) << ' ' << (restriction.emergencyExempt ? 1 : 0) << ' '
            << restriction.speedLimitKmh << ' ' << restriction.routeCostMultiplier << ' ' << restriction.startHour << ' ' << restriction.endHour << ' '
            << restriction.vehicleMassLimitKg << ' ' << restriction.vehicleHeightLimitM << ' ' << quote(restriction.name) << '\n';
    out << "AGENTSIM " << (trafficAgentSimulation.enabled ? 1 : 0) << ' ' << (trafficAgentSimulation.createDebugProxyVehicles ? 1 : 0) << ' '
        << (trafficAgentSimulation.enableLaneChanges ? 1 : 0) << ' ' << (trafficAgentSimulation.enableMerges ? 1 : 0) << ' ' << (trafficAgentSimulation.enableParking ? 1 : 0) << ' '
        << trafficAgentSimulation.maxFullPhysicsAgents << ' ' << trafficAgentSimulation.routeLookaheadLinks << ' ' << trafficAgentSimulation.fullSimulationHz << ' '
        << trafficAgentSimulation.simplifiedSimulationHz << ' ' << trafficAgentSimulation.perceptionRangeM << ' ' << trafficAgentSimulation.stopLineBufferM << ' '
        << trafficAgentSimulation.intersectionCreepSpeedKmh << ' ' << trafficAgentSimulation.parkingApproachSpeedKmh << ' ' << trafficAgentSimulation.spawnMinDistancePlayerM << ' '
        << trafficAgentSimulation.spawnMaxDistancePlayerM << ' ' << trafficAgentSimulation.minimumSpawnGapM << ' ' << trafficAgentSimulation.stuckTimeoutS << ' '
        << trafficAgentSimulation.despawnGraceS << ' ' << (trafficAgentSimulation.useHeritageVehicleDynamics ? 1 : 0) << ' ' << trafficAgentSimulation.trafficVehicleHighRateHz << '\n';
    for (const auto& profile : trafficAgentProfiles)
        out << "AGENTPROFILE " << profile.id << ' ' << static_cast<int>(profile.vehicleClass) << ' ' << (profile.enabled ? 1 : 0) << ' '
            << profile.spawnWeight << ' ' << profile.lengthM << ' ' << profile.widthM << ' ' << profile.maxSpeedFactor << ' ' << profile.accelerationFactor << ' '
            << profile.brakingFactor << ' ' << profile.desiredTimeGapS << ' ' << profile.minimumGapM << ' ' << profile.reactionTimeS << ' '
            << profile.laneChangeAggression << ' ' << profile.courtesy << ' ' << profile.speedCompliance << ' ' << profile.illegalOvertakeChance << ' '
            << profile.parkingSkill << ' ' << quote(profile.name) << ' ' << quote(profile.vehiclePreset) << '\n';
    for (const auto& portal : trafficSpawnPortals)
    {
        out << "PORTAL " << portal.id << ' ' << (portal.enabled ? 1 : 0) << ' ' << portal.nodeId << ' ' << static_cast<int>(portal.mode) << ' '
            << portal.headingDeg << ' ' << portal.radiusM << ' ' << portal.spawnWeight << ' ' << portal.maxConcurrentAgents << ' '
            << portal.startHour << ' ' << portal.endHour << ' ' << portal.minimumPlayerDistanceM << ' ' << portal.maximumPlayerDistanceM << ' '
            << (portal.emergencyAllowed ? 1 : 0) << ' ' << quote(portal.name) << ' ' << quote(portal.allowedClasses) << ' ';
        writeVec3(out, portal.position); out << '\n';
    }
    for (const auto& region : trafficDensityRegions)
    {
        out << "DENSITYREGION " << region.id << ' ' << (region.enabled ? 1 : 0) << ' ' << region.radiusM << ' ' << region.densityMultiplier << ' '
            << region.speedMultiplier << ' ' << region.laneChangeAggressionOffset << ' ' << region.parkingMultiplier << ' ' << region.startHour << ' ' << region.endHour
            << ' ' << quote(region.name) << ' '; writeVec3(out, region.position); out << '\n';
    }
    for (const auto& incident : trafficIncidents)
    {
        out << "INCIDENT " << incident.id << ' ' << static_cast<int>(incident.type) << ' ' << (incident.enabled ? 1 : 0) << ' ' << incident.roadId << ' '
            << incident.linkId << ' ' << incident.radiusM << ' ' << incident.severity << ' ' << incident.blockedLaneFraction << ' ' << incident.speedLimitKmh << ' '
            << incident.routeCostMultiplier << ' ' << incident.responseDelayS << ' ' << incident.clearAfterS << ' ' << (incident.emergencyResponse ? 1 : 0) << ' '
            << (incident.hazardLights ? 1 : 0) << ' ' << quote(incident.name) << ' '; writeVec3(out, incident.position); out << '\n';
    }
    out << "ENVIRONMENT " << trafficEnvironment.wetSpeedFactor << ' ' << trafficEnvironment.heavyRainSpeedFactor << ' ' << trafficEnvironment.snowSpeedFactor << ' '
        << trafficEnvironment.iceSpeedFactor << ' ' << trafficEnvironment.nightSpeedFactor << ' ' << trafficEnvironment.wetFollowingGapFactor << ' '
        << trafficEnvironment.wetBrakingFactor << ' ' << trafficEnvironment.poorVisibilitySpeedFactor << ' ' << (trafficEnvironment.standingWaterAvoidance ? 1 : 0) << ' '
        << (trafficEnvironment.weatherAwareLaneChanges ? 1 : 0) << '\n';
    out << "ADVANCEDBEHAVIOR " << (trafficBehavior.zipperMerging ? 1 : 0) << ' ' << (trafficBehavior.roundaboutNegotiation ? 1 : 0) << ' '
        << (trafficBehavior.enforceStopDwell ? 1 : 0) << ' ' << (trafficBehavior.opportunisticOvertaking ? 1 : 0) << ' '
        << (trafficBehavior.queueDischargeReaction ? 1 : 0) << ' ' << (trafficBehavior.stagedParkingManeuvers ? 1 : 0) << ' '
        << (trafficBehavior.stuckRecovery ? 1 : 0) << ' ' << (trafficBehavior.collisionIncidentResponse ? 1 : 0) << ' '
        << (trafficBehavior.emergencyIncidentDispatch ? 1 : 0) << ' ' << trafficBehavior.zipperAlternationWindowS << ' '
        << trafficBehavior.mergeCourtesyGapS << ' ' << trafficBehavior.roundaboutEntryGapS << ' ' << trafficBehavior.stopDwellS << ' '
        << trafficBehavior.yieldCreepSpeedKmh << ' ' << trafficBehavior.overtakeMinimumGainKmh << ' ' << trafficBehavior.overtakeReturnGapM << ' '
        << trafficBehavior.queueReactionSpreadS << ' ' << trafficBehavior.parkingReverseSpeedKmh << ' ' << trafficBehavior.recoveryReverseSeconds << ' '
        << trafficBehavior.recoveryRerouteSeconds << ' ' << trafficBehavior.recoveryTeleportSeconds << ' ' << trafficBehavior.collisionDistanceM << ' '
        << trafficBehavior.emergencyIncidentLookaheadM << '\n';
    out << "TRAFFICDEBUG " << (trafficDebug.enabled ? 1 : 0) << ' ' << (trafficDebug.showAgentIds ? 1 : 0) << ' ' << (trafficDebug.showRoutes ? 1 : 0) << ' '
        << (trafficDebug.showIntentions ? 1 : 0) << ' ' << (trafficDebug.showPerception ? 1 : 0) << ' ' << (trafficDebug.showFollowingGaps ? 1 : 0) << ' '
        << (trafficDebug.showLaneChangeScores ? 1 : 0) << ' ' << (trafficDebug.showWaitReasons ? 1 : 0) << ' ' << (trafficDebug.showStreamingTiers ? 1 : 0) << ' '
        << (trafficDebug.showIncidentInfluence ? 1 : 0) << ' ' << trafficDebug.maxDetailedAgents << '\n';
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadTraffic(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HROAD") { message = "Invalid HROAD file."; return false; }
    trafficNodes.clear(); trafficLinks.clear(); roadSplines.clear(); roadSplineNodes.clear(); roadIntersections.clear();
    turnConnectors.clear(); trafficSignalPhases.clear(); parkingStrips.clear(); intersectionControllers.clear(); roadRestrictions.clear(); trafficAgentProfiles.clear();
    trafficSpawnPortals.clear(); trafficDensityRegions.clear(); trafficIncidents.clear();
    trafficPopulation = TrafficPopulationConfiguration{}; navigationBuild = NavigationBuildConfiguration{};
    trafficRules = TrafficRulesConfiguration{}; trafficStreaming = TrafficStreamingConfiguration{}; trafficAgentSimulation = TrafficAgentSimulationConfiguration{};
    trafficEnvironment = TrafficEnvironmentConfiguration{}; trafficBehavior = TrafficBehaviorConfiguration{}; trafficDebug = TrafficDebugConfiguration{};
    std::string keyword; std::uint32_t maxId = 0;
    while (in >> keyword)
    {
        if (keyword == "NODE")
        {
            TrafficNode item; int type = 0, bidi = 0, overtake = 1, generated = 0;
            if (!(in >> item.id >> type >> item.headingDeg >> item.speedLimitKmh >> item.lanes >> item.priority >> bidi >> overtake >> item.density)) break;
            if (version >= 4) { if (!(in >> item.roadId >> item.laneIndex >> item.laneDirection >> generated)) break; }
            if (!(in >> std::quoted(item.name))) break;
            if (!readVec3(in, item.position)) break;
            item.type = clampEnum<TrafficNodeType>(type, static_cast<int>(TrafficNodeType::Destination)); item.bidirectional = bidi != 0; item.overtakingAllowed = overtake != 0;
            item.generated = version >= 4 ? generated != 0 : item.name.rfind("AUTO_LANE_", 0) == 0;
            maxId = std::max(maxId, item.id); trafficNodes.push_back(item); continue;
        }
        if (keyword == "LINK")
        {
            TrafficLink link; int bidi = 0, overtake = 1, linkType = 0, enabled = 1, generated = 0;
            if (!(in >> link.id >> link.fromNodeId >> link.toNodeId >> link.lanes >> link.speedLimitKmh >> bidi >> overtake >> link.density)) break;
            if (version >= 4) { if (!(in >> linkType >> link.routeCostMultiplier >> enabled >> generated)) break; }
            link.bidirectional = bidi != 0; link.overtakingAllowed = overtake != 0;
            link.type = clampEnum<TrafficLinkType>(linkType, static_cast<int>(TrafficLinkType::SpawnAccess)); link.enabled = enabled != 0; link.generated = generated != 0;
            maxId = std::max(maxId, link.id); trafficLinks.push_back(link); continue;
        }
        if (version >= 3 && keyword == "ROAD")
        {
            RoadSpline road; int roadClass = 0, enabled = 1, oneWay = 0, sidewalkL = 0, sidewalkR = 0, parkingL = 0, parkingR = 0;
            if (!(in >> road.id >> roadClass >> enabled >> oneWay >> road.lanesForward >> road.lanesBackward >> road.laneWidthM
                >> road.shoulderLeftM >> road.shoulderRightM >> road.medianWidthM >> road.speedLimitKmh >> sidewalkL >> sidewalkR
                >> parkingL >> parkingR >> road.trafficDensity >> road.spawnWeight >> std::quoted(road.name))) break;
            road.roadClass = clampEnum<RoadClass>(roadClass, static_cast<int>(RoadClass::Dirt)); road.enabled = enabled != 0; road.oneWay = oneWay != 0;
            road.sidewalkLeft = sidewalkL != 0; road.sidewalkRight = sidewalkR != 0; road.parkingLeft = parkingL != 0; road.parkingRight = parkingR != 0;
            maxId = std::max(maxId, road.id); roadSplines.push_back(road); continue;
        }
        if (version >= 3 && keyword == "ROADNODE")
        {
            RoadSplineNode node; int automatic = 1;
            if (!(in >> node.id >> node.roadId >> node.order >> automatic >> node.widthScale >> node.bankingDeg)) break;
            if (!readVec3(in, node.position) || !readVec3(in, node.handleIn) || !readVec3(in, node.handleOut)) break;
            node.automaticTangents = automatic != 0; maxId = std::max(maxId, node.id); roadSplineNodes.push_back(node); continue;
        }
        if (version >= 3 && keyword == "JUNCTION")
        {
            RoadIntersection junction; int priority = 0, lights = 0, crossing = 0;
            if (!(in >> junction.id >> priority >> junction.radiusM >> lights >> crossing >> junction.approachSpeedKmh >> std::quoted(junction.name))) break;
            if (!readVec3(in, junction.position)) break;
            junction.priority = clampEnum<JunctionPriority>(priority, static_cast<int>(JunctionPriority::Uncontrolled)); junction.trafficLights = lights != 0; junction.pedestrianCrossing = crossing != 0;
            maxId = std::max(maxId, junction.id); roadIntersections.push_back(junction); continue;
        }
        if (version >= 3 && keyword == "TURN")
        {
            TurnConnector connector; int enabled = 1, yield = 0, uturn = 0;
            if (!(in >> connector.id >> connector.intersectionId >> connector.fromRoadId >> connector.toRoadId >> connector.fromLane >> connector.toLane
                >> enabled >> yield >> uturn >> connector.speedLimitKmh)) break;
            if (version >= 4) { if (!(in >> connector.conflictGroup >> connector.reservationSeconds)) break; }
            connector.enabled = enabled != 0; connector.yield = yield != 0; connector.uTurn = uturn != 0; maxId = std::max(maxId, connector.id); turnConnectors.push_back(connector); continue;
        }
        if (version >= 3 && keyword == "SIGNALPHASE")
        {
            TrafficSignalPhase phase;
            if (!(in >> phase.id >> phase.intersectionId >> phase.order >> phase.greenSeconds >> phase.yellowSeconds >> phase.allRedSeconds >> std::quoted(phase.name) >> std::quoted(phase.connectorIds))) break;
            maxId = std::max(maxId, phase.id); trafficSignalPhases.push_back(phase); continue;
        }
        if (version >= 3 && keyword == "PARKINGSTRIP")
        {
            ParkingStrip parking; int right = 1;
            if (!(in >> parking.id >> parking.roadId >> parking.headingDeg >> parking.spaces >> parking.spacingM >> parking.angleDeg >> right >> parking.occupancy >> std::quoted(parking.name))) break;
            if (!readVec3(in, parking.position)) break; parking.rightSide = right != 0; maxId = std::max(maxId, parking.id); parkingStrips.push_back(parking); continue;
        }
        if (version >= 3 && keyword == "POPULATION")
        {
            if (!(in >> trafficPopulation.globalDensity >> trafficPopulation.parkedDensity >> trafficPopulation.rushHourMultiplier >> trafficPopulation.nightMultiplier
                >> trafficPopulation.heavyVehicleShare >> trafficPopulation.motorcycleShare >> trafficPopulation.commercialShare >> trafficPopulation.emergencyShare
                >> trafficPopulation.laneChangeAggression >> trafficPopulation.speedVariance >> trafficPopulation.maxActiveVehicles)) break;
            continue;
        }
        if (version >= 3 && keyword == "NAVBUILD")
        {
            int enabled = 1, rebuild = 1;
            if (!(in >> enabled >> rebuild >> navigationBuild.maxSlopeDeg >> navigationBuild.minimumTurnRadiusM >> navigationBuild.laneChangeLengthM
                >> navigationBuild.junctionLookaheadM >> navigationBuild.mergeLookaheadM)) break;
            navigationBuild.enabled = enabled != 0; navigationBuild.rebuildOnSave = rebuild != 0; continue;
        }
        if (version >= 4 && keyword == "TRAFFICRULES")
        {
            int side = 0, keep = 1, turnOnRed = 0, corridor = 1;
            if (!(in >> side >> keep >> turnOnRed >> corridor >> trafficRules.desiredTimeGapS >> trafficRules.minimumGapM
                >> trafficRules.desiredAccelerationMps2 >> trafficRules.comfortableBrakingMps2 >> trafficRules.laneChangeCooldownS
                >> trafficRules.laneChangeMinimumGapM >> trafficRules.laneChangeRouteCost >> trafficRules.mergeRouteCost
                >> trafficRules.emergencyYieldRadiusM >> trafficRules.roundaboutYieldDistanceM)) break;
            trafficRules.drivingSide = clampEnum<DrivingSide>(side, static_cast<int>(DrivingSide::Left)); trafficRules.keepToDrivingSide = keep != 0;
            trafficRules.allowTurnOnRed = turnOnRed != 0; trafficRules.emergencyCorridor = corridor != 0; continue;
        }
        if (version >= 4 && keyword == "STREAMING")
        {
            int retain = 1;
            if (!(in >> trafficStreaming.fullSimulationRadiusM >> trafficStreaming.simplifiedSimulationRadiusM >> trafficStreaming.dormantPersistenceRadiusM
                >> trafficStreaming.sectorSizeM >> trafficStreaming.maxSpawnsPerSecond >> trafficStreaming.maxDespawnsPerSecond
                >> trafficStreaming.despawnBehindDistanceM >> retain >> trafficStreaming.dormantStateMinutes)) break;
            trafficStreaming.retainDormantState = retain != 0; continue;
        }
        if (version >= 4 && keyword == "INTERSECTIONCTRL")
        {
            IntersectionController control; int mode = 0, adaptive = 0, preempt = 1;
            if (!(in >> control.intersectionId >> mode >> control.phaseOffsetSeconds >> control.minimumGreenSeconds >> control.maximumGreenSeconds
                >> control.detectorDistanceM >> control.gapOutSeconds >> adaptive >> preempt)) break;
            control.mode = clampEnum<SignalControlMode>(mode, static_cast<int>(SignalControlMode::Adaptive)); control.queueAdaptive = adaptive != 0; control.emergencyPreemption = preempt != 0;
            intersectionControllers.push_back(control); continue;
        }
        if (version >= 4 && keyword == "RESTRICTION")
        {
            RoadRestriction restriction; int type = 0, enabled = 1, block = 1, exempt = 1;
            if (!(in >> restriction.id >> type >> enabled >> restriction.roadId >> restriction.linkId >> block >> exempt
                >> restriction.speedLimitKmh >> restriction.routeCostMultiplier >> restriction.startHour >> restriction.endHour
                >> restriction.vehicleMassLimitKg >> restriction.vehicleHeightLimitM >> std::quoted(restriction.name))) break;
            restriction.type = clampEnum<RoadRestrictionType>(type, static_cast<int>(RoadRestrictionType::HeightLimit)); restriction.enabled = enabled != 0;
            restriction.blockTraffic = block != 0; restriction.emergencyExempt = exempt != 0; maxId = std::max(maxId, restriction.id); roadRestrictions.push_back(restriction); continue;
        }
        if (version >= 5 && keyword == "AGENTSIM")
        {
            int enabled = 0, debugProxy = 1, laneChanges = 1, merges = 1, parking = 1;
            if (!(in >> enabled >> debugProxy >> laneChanges >> merges >> parking >> trafficAgentSimulation.maxFullPhysicsAgents
                >> trafficAgentSimulation.routeLookaheadLinks >> trafficAgentSimulation.fullSimulationHz >> trafficAgentSimulation.simplifiedSimulationHz
                >> trafficAgentSimulation.perceptionRangeM >> trafficAgentSimulation.stopLineBufferM >> trafficAgentSimulation.intersectionCreepSpeedKmh
                >> trafficAgentSimulation.parkingApproachSpeedKmh >> trafficAgentSimulation.spawnMinDistancePlayerM >> trafficAgentSimulation.spawnMaxDistancePlayerM
                >> trafficAgentSimulation.minimumSpawnGapM >> trafficAgentSimulation.stuckTimeoutS >> trafficAgentSimulation.despawnGraceS)) break;
            trafficAgentSimulation.enabled = enabled != 0; trafficAgentSimulation.createDebugProxyVehicles = debugProxy != 0;
            trafficAgentSimulation.enableLaneChanges = laneChanges != 0; trafficAgentSimulation.enableMerges = merges != 0; trafficAgentSimulation.enableParking = parking != 0;
            if (version >= 6)
            {
                int fullVehicle = 0;
                if (!(in >> fullVehicle >> trafficAgentSimulation.trafficVehicleHighRateHz)) break;
                trafficAgentSimulation.useHeritageVehicleDynamics = fullVehicle != 0;
            }
            continue;
        }
        if (version >= 5 && keyword == "AGENTPROFILE")
        {
            TrafficAgentProfile profile; int vehicleClass = 0, enabled = 1;
            if (!(in >> profile.id >> vehicleClass >> enabled >> profile.spawnWeight >> profile.lengthM >> profile.widthM >> profile.maxSpeedFactor
                >> profile.accelerationFactor >> profile.brakingFactor >> profile.desiredTimeGapS >> profile.minimumGapM >> profile.reactionTimeS
                >> profile.laneChangeAggression >> profile.courtesy >> profile.speedCompliance >> profile.illegalOvertakeChance >> profile.parkingSkill
                >> std::quoted(profile.name) >> std::quoted(profile.vehiclePreset))) break;
            profile.vehicleClass = clampEnum<TrafficAgentClass>(vehicleClass, static_cast<int>(TrafficAgentClass::Emergency)); profile.enabled = enabled != 0;
            maxId = std::max(maxId, profile.id); trafficAgentProfiles.push_back(profile); continue;
        }
        if (version >= 6 && keyword == "PORTAL")
        {
            TrafficSpawnPortal portal; int enabled = 1, mode = 0, emergency = 1;
            if (!(in >> portal.id >> enabled >> portal.nodeId >> mode >> portal.headingDeg >> portal.radiusM >> portal.spawnWeight >> portal.maxConcurrentAgents
                >> portal.startHour >> portal.endHour >> portal.minimumPlayerDistanceM >> portal.maximumPlayerDistanceM >> emergency
                >> std::quoted(portal.name) >> std::quoted(portal.allowedClasses))) break;
            if (!readVec3(in, portal.position)) break; portal.enabled = enabled != 0; portal.emergencyAllowed = emergency != 0;
            portal.mode = clampEnum<TrafficPortalMode>(mode, static_cast<int>(TrafficPortalMode::DespawnOnly)); maxId = std::max(maxId, portal.id); trafficSpawnPortals.push_back(portal); continue;
        }
        if (version >= 6 && keyword == "DENSITYREGION")
        {
            TrafficDensityRegion region; int enabled = 1;
            if (!(in >> region.id >> enabled >> region.radiusM >> region.densityMultiplier >> region.speedMultiplier >> region.laneChangeAggressionOffset
                >> region.parkingMultiplier >> region.startHour >> region.endHour >> std::quoted(region.name))) break;
            if (!readVec3(in, region.position)) break; region.enabled = enabled != 0; maxId = std::max(maxId, region.id); trafficDensityRegions.push_back(region); continue;
        }
        if (version >= 6 && keyword == "INCIDENT")
        {
            TrafficIncident incident; int type = 0, enabled = 1, response = 1, hazards = 1;
            if (!(in >> incident.id >> type >> enabled >> incident.roadId >> incident.linkId >> incident.radiusM >> incident.severity >> incident.blockedLaneFraction
                >> incident.speedLimitKmh >> incident.routeCostMultiplier >> incident.responseDelayS >> incident.clearAfterS >> response >> hazards >> std::quoted(incident.name))) break;
            if (!readVec3(in, incident.position)) break; incident.enabled = enabled != 0; incident.emergencyResponse = response != 0; incident.hazardLights = hazards != 0;
            incident.type = clampEnum<TrafficIncidentType>(type, static_cast<int>(TrafficIncidentType::Flooding)); maxId = std::max(maxId, incident.id); trafficIncidents.push_back(incident); continue;
        }
        if (version >= 6 && keyword == "ENVIRONMENT")
        {
            int water = 1, weatherLane = 1;
            if (!(in >> trafficEnvironment.wetSpeedFactor >> trafficEnvironment.heavyRainSpeedFactor >> trafficEnvironment.snowSpeedFactor
                >> trafficEnvironment.iceSpeedFactor >> trafficEnvironment.nightSpeedFactor >> trafficEnvironment.wetFollowingGapFactor
                >> trafficEnvironment.wetBrakingFactor >> trafficEnvironment.poorVisibilitySpeedFactor >> water >> weatherLane)) break;
            trafficEnvironment.standingWaterAvoidance = water != 0; trafficEnvironment.weatherAwareLaneChanges = weatherLane != 0; continue;
        }
        if (version >= 7 && keyword == "ADVANCEDBEHAVIOR")
        {
            int zipper = 1, roundabout = 1, stopDwell = 1, overtake = 1, queue = 1, parking = 1, recovery = 1, collision = 1, dispatch = 1;
            if (!(in >> zipper >> roundabout >> stopDwell >> overtake >> queue >> parking >> recovery >> collision >> dispatch
                >> trafficBehavior.zipperAlternationWindowS >> trafficBehavior.mergeCourtesyGapS >> trafficBehavior.roundaboutEntryGapS
                >> trafficBehavior.stopDwellS >> trafficBehavior.yieldCreepSpeedKmh >> trafficBehavior.overtakeMinimumGainKmh
                >> trafficBehavior.overtakeReturnGapM >> trafficBehavior.queueReactionSpreadS >> trafficBehavior.parkingReverseSpeedKmh
                >> trafficBehavior.recoveryReverseSeconds >> trafficBehavior.recoveryRerouteSeconds >> trafficBehavior.recoveryTeleportSeconds
                >> trafficBehavior.collisionDistanceM >> trafficBehavior.emergencyIncidentLookaheadM)) break;
            trafficBehavior.zipperMerging = zipper != 0; trafficBehavior.roundaboutNegotiation = roundabout != 0;
            trafficBehavior.enforceStopDwell = stopDwell != 0; trafficBehavior.opportunisticOvertaking = overtake != 0;
            trafficBehavior.queueDischargeReaction = queue != 0; trafficBehavior.stagedParkingManeuvers = parking != 0;
            trafficBehavior.stuckRecovery = recovery != 0; trafficBehavior.collisionIncidentResponse = collision != 0;
            trafficBehavior.emergencyIncidentDispatch = dispatch != 0; continue;
        }
        if (version >= 6 && keyword == "TRAFFICDEBUG")
        {
            int enabled = 1, ids = 1, routes = 1, intentions = 1, perception = 0, gaps = 1, scores = 1, waits = 1, tiers = 1, incidents = 1;
            if (!(in >> enabled >> ids >> routes >> intentions >> perception >> gaps >> scores >> waits >> tiers >> incidents >> trafficDebug.maxDetailedAgents)) break;
            trafficDebug.enabled = enabled != 0; trafficDebug.showAgentIds = ids != 0; trafficDebug.showRoutes = routes != 0; trafficDebug.showIntentions = intentions != 0;
            trafficDebug.showPerception = perception != 0; trafficDebug.showFollowingGaps = gaps != 0; trafficDebug.showLaneChangeScores = scores != 0;
            trafficDebug.showWaitReasons = waits != 0; trafficDebug.showStreamingTiers = tiers != 0; trafficDebug.showIncidentInfluence = incidents != 0; continue;
        }
        std::string ignore; std::getline(in, ignore);
    }
    if (version < 4)
    {
        const auto generatedNode = [&](std::uint32_t id) { for (const auto& node : trafficNodes) if (node.id == id) return node.generated; return false; };
        for (auto& link : trafficLinks) link.generated = generatedNode(link.fromNodeId) && generatedNode(link.toNodeId);
    }
    for (const auto& junction : roadIntersections)
    {
        bool exists = false; for (const auto& control : intersectionControllers) if (control.intersectionId == junction.id) { exists = true; break; }
        if (!exists) { IntersectionController control; control.intersectionId = junction.id; intersectionControllers.push_back(control); }
    }
    m_nextId = std::max(m_nextId, maxId + 1);
    if (version < 5 && trafficAgentProfiles.empty())
    {
        auto& compact = addTrafficAgentProfile(TrafficAgentClass::Compact, "Everyday Compact"); compact.spawnWeight = 1.35f; compact.lengthM = 4.05f; compact.widthM = 1.76f; compact.speedCompliance = 0.95f;
        auto& sedan = addTrafficAgentProfile(TrafficAgentClass::Sedan, "Calm Sedan"); sedan.desiredTimeGapS = 1.9f; sedan.courtesy = 0.78f; sedan.laneChangeAggression = 0.30f;
        auto& van = addTrafficAgentProfile(TrafficAgentClass::Van, "Delivery Van"); van.spawnWeight = 0.45f;
        auto& sport = addTrafficAgentProfile(TrafficAgentClass::Sport, "Assertive Sport"); sport.spawnWeight = 0.20f;
        auto& truck = addTrafficAgentProfile(TrafficAgentClass::Truck, "Heavy Truck"); truck.spawnWeight = 0.18f;
    }
    if (version < 6 && trafficSpawnPortals.empty())
    {
        for (const auto& node : trafficNodes)
        {
            if (node.type != TrafficNodeType::Spawn) continue;
            auto& portal = addTrafficSpawnPortal("Imported Traffic Spawn Portal"); portal.nodeId = node.id; portal.position = node.position; portal.headingDeg = node.headingDeg; break;
        }
    }
    if (version < 3 && roadSplines.empty()) addRoadSpline(RoadClass::Local, "Imported Legacy Road Layer");
    message = "Loaded " + file.string() + " (HROAD v" + std::to_string(version) + ")";
    return true;
}

bool StudioAuthoringData::saveWeather(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    out << "HWEATHER 1\n"
        << "latitude " << weather.latitude << '\n'
        << "longitude " << weather.longitude << '\n'
        << "elevationM " << weather.elevationM << '\n'
        << "startHour " << weather.startHour << '\n'
        << "cloudCoverage " << weather.cloudCoverage << '\n'
        << "rainIntensity " << weather.rainIntensity << '\n'
        << "temperatureC " << weather.temperatureC << '\n'
        << "humidity " << weather.humidity << '\n'
        << "surfaceWetness " << weather.surfaceWetness << '\n';
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadWeather(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HWEATHER") { message = "Invalid HWEATHER file."; return false; }
    std::string key;
    while (in >> key)
    {
        if (key == "latitude") in >> weather.latitude;
        else if (key == "longitude") in >> weather.longitude;
        else if (key == "elevationM") in >> weather.elevationM;
        else if (key == "startHour") in >> weather.startHour;
        else if (key == "cloudCoverage") in >> weather.cloudCoverage;
        else if (key == "rainIntensity") in >> weather.rainIntensity;
        else if (key == "temperatureC") in >> weather.temperatureC;
        else if (key == "humidity") in >> weather.humidity;
        else if (key == "surfaceWetness") in >> weather.surfaceWetness;
        else { std::string ignore; std::getline(in, ignore); }
    }
    message = "Loaded " + file.string();
    return true;
}

bool StudioAuthoringData::saveVehicle(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    out << "HVEHICLEAUTHOR 1\n"
        << "displayName " << quote(vehicle.displayName) << '\n'
        << "vehicleDefinition " << quote(vehicle.vehicleDefinition) << '\n'
        << "acousticProfile " << quote(vehicle.acousticProfile) << '\n'
        << "tireSet " << quote(vehicle.tireSet) << '\n'
        << "spawnHeightM " << vehicle.spawnHeightM << '\n'
        << "fuelLiters " << vehicle.fuelLiters << '\n'
        << "trafficEligible " << (vehicle.trafficEligible ? 1 : 0) << '\n'
        << "raceEligible " << (vehicle.raceEligible ? 1 : 0) << '\n';
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadVehicle(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HVEHICLEAUTHOR") { message = "Invalid HVEHICLEAUTHOR file."; return false; }
    std::string key;
    while (in >> key)
    {
        if (key == "displayName") in >> std::quoted(vehicle.displayName);
        else if (key == "vehicleDefinition") in >> std::quoted(vehicle.vehicleDefinition);
        else if (key == "acousticProfile") in >> std::quoted(vehicle.acousticProfile);
        else if (key == "tireSet") in >> std::quoted(vehicle.tireSet);
        else if (key == "spawnHeightM") in >> vehicle.spawnHeightM;
        else if (key == "fuelLiters") in >> vehicle.fuelLiters;
        else if (key == "trafficEligible") { int v = 0; in >> v; vehicle.trafficEligible = v != 0; }
        else if (key == "raceEligible") { int v = 0; in >> v; vehicle.raceEligible = v != 0; }
        else { std::string ignore; std::getline(in, ignore); }
    }
    message = "Loaded " + file.string();
    return true;
}
bool StudioAuthoringData::saveGameplay(const std::filesystem::path& file, std::string& message) const
{
    std::ofstream out;
    if (!openOutput(file, out, message)) return false;
    // HGAME 12 adds STUDIO28 autoslalom/gymkhana event types while preserving HGAME 1..11 loading.
    out << "HGAME 12\n";
    for (const auto& event : gameEvents)
    {
        out << "EVENT " << event.id << ' ' << static_cast<int>(event.type) << ' ' << (event.enabled ? 1 : 0) << ' '
            << event.startMarkerId << ' ' << event.finishMarkerId << ' ' << event.layoutId << ' ' << event.laps << ' ' << event.maxEntrants << ' '
            << (event.rollingStart ? 1 : 0) << ' ' << (event.trafficEnabled ? 1 : 0) << ' '
            << (event.policeEnabled ? 1 : 0) << ' ' << (event.nightOnly ? 1 : 0) << ' '
            << event.entryFee << ' ' << event.reward << ' ' << event.heat << ' ' << quote(event.name) << '\n';
    }
    for (const auto& point : worldPoints)
    {
        out << "POINT " << point.id << ' ' << static_cast<int>(point.type) << ' ' << (point.enabled ? 1 : 0) << ' '
            << point.headingDeg << ' ' << point.radiusM << ' ' << (point.discoverable ? 1 : 0) << ' '
            << (point.fastTravelEnabled ? 1 : 0) << ' ' << point.servicePriceMultiplier << ' ' << quote(point.name) << ' ';
        writeVec3(out, point.position); out << '\n';
    }
    const auto& police = policeGameplay;
    out << "POLICE_CONFIG " << (police.enabled ? 1 : 0) << ' ' << police.maxHeatLevel << ' ' << police.maxPursuitUnits << ' '
        << police.civilianWitnessRadiusM << ' ' << police.policeDetectionRadiusM << ' ' << police.speedToleranceKmh << ' '
        << police.heatDecayDelayS << ' ' << police.heatDecayPerSecond << ' ' << police.lostSightSeconds << ' '
        << police.searchDurationS << ' ' << police.cooldownDurationS << ' ' << police.bustHoldSeconds << ' ' << police.backupDelayS << ' '
        << police.roadblockMinimumHeat << ' ' << (police.civilianWitnesses ? 1 : 0) << ' ' << (police.speedingGeneratesHeat ? 1 : 0) << ' '
        << (police.collisionsGenerateHeat ? 1 : 0) << ' ' << (police.illegalRacesGenerateHeat ? 1 : 0) << ' ' << (police.evasionEscalatesHeat ? 1 : 0) << '\n';
    for (const auto& zone : policePatrolZones)
    {
        out << "PATROL " << zone.id << ' ' << (zone.enabled ? 1 : 0) << ' ' << zone.radiusM << ' ' << zone.patrolWeight << ' '
            << zone.maximumUnits << ' ' << zone.responseMultiplier << ' ' << zone.speedToleranceKmh << ' ' << zone.startHour << ' ' << zone.endHour << ' '
            << zone.responsePortalId << ' ' << quote(zone.name) << ' '; writeVec3(out, zone.position); out << '\n';
    }
    for (const auto& site : policeRoadblockSites)
    {
        out << "ROADBLOCK " << site.id << ' ' << (site.enabled ? 1 : 0) << ' ' << site.nodeId << ' ' << site.headingDeg << ' ' << site.widthM << ' '
            << site.minimumHeat << ' ' << site.unitCount << ' ' << (site.spikeStrip ? 1 : 0) << ' ' << (site.leaveEscapeGap ? 1 : 0) << ' '
            << site.selectionWeight << ' ' << quote(site.name) << ' '; writeVec3(out, site.position); out << '\n';
    }
    for (const auto& zone : policeEscapeZones)
    {
        out << "ESCAPE " << zone.id << ' ' << (zone.enabled ? 1 : 0) << ' ' << zone.radiusM << ' ' << zone.searchTimeMultiplier << ' '
            << zone.heatDecayMultiplier << ' ' << (zone.breakLineOfSight ? 1 : 0) << ' ' << (zone.safehouse ? 1 : 0) << ' ' << quote(zone.name) << ' ';
        writeVec3(out, zone.position); out << '\n';
    }
    for (const auto& meet : clandestineMeets)
    {
        out << "MEET " << meet.id << ' ' << (meet.enabled ? 1 : 0) << ' ' << meet.headingDeg << ' ' << meet.radiusM << ' ' << meet.openHour << ' '
            << meet.closeHour << ' ' << meet.maximumVehicles << ' ' << meet.policeRisk << ' ' << meet.heatMultiplier << ' ' << meet.eventId << ' '
            << (meet.discoverable ? 1 : 0) << ' ' << quote(meet.name) << ' '; writeVec3(out, meet.position); out << '\n';
    }
    out << "MOTORSPORT_CONFIG " << (motorsport.enabled ? 1 : 0) << ' ' << (motorsport.aiCompetitorsEnabled ? 1 : 0) << ' '
        << (motorsport.autoBuildGrid ? 1 : 0) << ' ' << (motorsport.simulateUnspawnedCompetitors ? 1 : 0) << ' ' << motorsport.maxPhysicalCompetitors << ' '
        << motorsport.defaultAiSkill << ' ' << motorsport.qualifyingPaceSpreadPercent << ' ' << motorsport.baseMechanicalDnfChancePerHour << ' '
        << (motorsport.multiClassTiming ? 1 : 0) << ' ' << (motorsport.championshipPersistence ? 1 : 0) << '\n';
    out << "MOTORSPORT_AI " << (motorsportAi.enabled ? 1 : 0) << ' ' << motorsportAi.updateHz << ' ' << motorsportAi.lookaheadMinimumM << ' '
        << motorsportAi.lookaheadMaximumM << ' ' << motorsportAi.brakingLookaheadM << ' ' << motorsportAi.opponentAwarenessM << ' '
        << motorsportAi.slipstreamMinimumGapM << ' ' << motorsportAi.slipstreamMaximumGapM << ' ' << motorsportAi.overtakeMinimumClosingKmh << ' '
        << motorsportAi.defensiveTriggerGapM << ' ' << motorsportAi.blueFlagYieldGapM << ' ' << motorsportAi.wetLineThreshold << ' '
        << motorsportAi.maximumWetSpeedPenalty << ' ' << motorsportAi.fuelUseLitersPer100Km << ' ' << motorsportAi.tireWearPer100Km << ' '
        << motorsportAi.fuelReserveLaps << ' ' << motorsportAi.tirePitThreshold << ' ' << motorsportAi.mistakeRecoverySeconds << ' '
        << (motorsportAi.strategyEnabled ? 1 : 0) << ' ' << (motorsportAi.mistakesEnabled ? 1 : 0) << ' '
        << (motorsportAi.slipstreamEnabled ? 1 : 0) << ' ' << (motorsportAi.defendingEnabled ? 1 : 0) << ' '
        << (motorsportAi.multiclassNegotiation ? 1 : 0) << ' ' << (motorsportAi.wetLineEnabled ? 1 : 0) << ' '
        << (motorsportAi.liveDecisionTelemetry ? 1 : 0) << '\n';
    out << "MOTORSPORT_AI_PHYSICS " << (motorsportAi.fullPhysicsCompetitors ? 1 : 0) << ' ' << motorsportAi.physicsHighRateHz << ' '
        << motorsportAi.steeringLookaheadSeconds << ' ' << motorsportAi.steeringGain << ' ' << motorsportAi.crossTrackGain << ' '
        << motorsportAi.throttleGain << ' ' << motorsportAi.brakeGain << ' ' << motorsportAi.maximumSteerAngleDeg << ' '
        << motorsportAi.sideBySideSafetyM << ' ' << motorsportAi.trackLimitSafetyM << ' ' << motorsportAi.gripSlipRatioLimit << ' '
        << motorsportAi.gripSlipAngleDeg << ' ' << motorsportAi.physicalRecoveryDistanceM << ' ' << motorsportAi.formationSpeedKmh << ' '
        << motorsportAi.rollingStartSpeedKmh << ' ' << motorsportAi.pitLaneSpeedKmh << ' ' << motorsportAi.damagePitThreshold << ' '
        << motorsportAi.damageDnfThreshold << ' ' << motorsportAi.collisionDamageScale << ' ' << motorsportAi.weatherForecastSeconds << ' '
        << (motorsportAi.gripAwareBraking ? 1 : 0) << ' ' << (motorsportAi.spatialAvoidance ? 1 : 0) << ' '
        << (motorsportAi.trackLimitAwarePassing ? 1 : 0) << ' ' << (motorsportAi.damageStrategyEnabled ? 1 : 0) << ' '
        << (motorsportAi.weatherForecastEnabled ? 1 : 0) << '\n';
    out << "MOTORSPORT_AI_RACECRAFT " << (motorsportAi.colliderBoundsAuthority ? 1 : 0) << ' ' << motorsportAi.collisionEnvelopeMarginM << ' '
        << motorsportAi.sweptEnvelopeSeconds << ' ' << motorsportAi.sideBySideOverlapToleranceM << ' ' << motorsportAi.divebombCommitGapM << ' '
        << motorsportAi.divebombClosingThresholdKmh << ' ' << motorsportAi.switchbackWindowS << ' ' << motorsportAi.maximumDefensiveMovesPerStraight << ' '
        << motorsportAi.blockingPenaltySeconds << ' ' << motorsportAi.unsafeReleasePenaltySeconds << ' ' << motorsportAi.pitReleaseLookaheadM << ' '
        << motorsportAi.multiclassPassHorizonS << ' ' << motorsportAi.tireOptimalMinimumC << ' ' << motorsportAi.tireOptimalMaximumC << ' '
        << motorsportAi.fuelDensityKgPerLiter << ' ' << (motorsportAi.predictiveCollisionAvoidance ? 1 : 0) << ' '
        << (motorsportAi.divebombJudgement ? 1 : 0) << ' ' << (motorsportAi.blockingRules ? 1 : 0) << ' '
        << (motorsportAi.unsafeReleaseStewarding ? 1 : 0) << ' ' << (motorsportAi.tireThermalStrategy ? 1 : 0) << ' '
        << (motorsportAi.fuelMassAwareness ? 1 : 0) << ' ' << (motorsportAi.componentDamageStrategy ? 1 : 0) << '\n';
    out << "MOTORSPORT_AI_INCIDENTS " << (motorsportAi.contactEvidenceEnabled ? 1 : 0) << ' '
        << (motorsportAi.incidentStewardingEnabled ? 1 : 0) << ' ' << motorsportAi.incidentMinimumNormalImpulseNs << ' '
        << motorsportAi.incidentMinimumClosingKmh << ' ' << motorsportAi.severeIncidentNormalImpulseNs << ' '
        << motorsportAi.severeIncidentClosingKmh << ' ' << motorsportAi.avoidableContactPenaltySeconds << ' '
        << motorsportAi.severeContactPenaltySeconds << ' ' << motorsportAi.contactEvidenceCooldownSeconds << ' '
        << motorsportAi.retainedIncidentEvidence << '\n';
    out << "MOTORSPORT_REPLAY " << (motorsportReplay.enabled ? 1 : 0) << ' ' << motorsportReplay.sampleHz << ' '
        << motorsportReplay.preRollSeconds << ' ' << motorsportReplay.postRollSeconds << ' ' << motorsportReplay.maximumIncidentClips << ' '
        << motorsportReplay.maximumRecordedCompetitors << ' ' << (motorsportReplay.capturePlayer ? 1 : 0) << ' '
        << (motorsportReplay.captureControls ? 1 : 0) << ' ' << (motorsportReplay.ghostReviewEnabled ? 1 : 0) << ' '
        << motorsportReplay.maximumGhostVehicles << '\n';
    out << "MOTORSPORT_REPLAY_CAMERA " << (motorsportReplay.broadcastDirectorEnabled ? 1 : 0) << ' '
        << (motorsportReplay.autoIncidentCamera ? 1 : 0) << ' ' << motorsportReplay.incidentCameraDistanceM << ' '
        << motorsportReplay.incidentCameraHeightM << ' ' << motorsportReplay.tracksideCameraLeadM << ' '
        << motorsportReplay.helicopterCameraHeightM << ' ' << motorsportReplay.cameraSmoothing << '\n';
    for (const auto& cls : motorsportClasses)
        out << "MCLASS " << cls.id << ' ' << (cls.enabled ? 1 : 0) << ' ' << cls.minimumPowerKw << ' ' << cls.maximumPowerKw << ' '
            << cls.minimumWeightKg << ' ' << cls.maximumWeightKg << ' ' << cls.balanceBallastKg << ' ' << cls.maximumEntrants << ' '
            << quote(cls.code) << ' ' << quote(cls.name) << '\n';
    for (const auto& entrant : motorsportEntrants)
        out << "ENTRANT " << entrant.id << ' ' << (entrant.enabled ? 1 : 0) << ' ' << entrant.eventId << ' ' << entrant.classId << ' ' << entrant.raceNumber << ' '
            << entrant.aiSkill << ' ' << entrant.qualifyingPace << ' ' << entrant.racePace << ' ' << entrant.wetSkill << ' ' << entrant.aggression << ' '
            << entrant.consistency << ' ' << entrant.pitSkill << ' ' << entrant.racecraft << ' ' << entrant.awareness << ' ' << entrant.defending << ' '
            << entrant.tireManagement << ' ' << entrant.fuelManagement << ' ' << entrant.strategyRisk << ' ' << entrant.mistakeRatePerHour << ' '
            << entrant.reactionTimeS << ' ' << entrant.preferredLineBias << ' ' << (entrant.clandestine ? 1 : 0) << ' ' << entrant.gridOverride << ' '
            << quote(entrant.driverName) << ' ' << quote(entrant.teamName) << ' ' << quote(entrant.vehiclePreset) << '\n';
    for (const auto& championship : motorsportChampionships)
        out << "CHAMPIONSHIP " << championship.id << ' ' << (championship.enabled ? 1 : 0) << ' ' << championship.classId << ' '
            << championship.poleBonus << ' ' << championship.fastestLapBonus << ' ' << championship.dropWorstRounds << ' '
            << quote(championship.pointsScheme) << ' ' << quote(championship.name) << '\n';
    for (const auto& round : motorsportRounds)
        out << "ROUND " << round.id << ' ' << round.championshipId << ' ' << round.eventId << ' ' << (round.enabled ? 1 : 0) << ' ' << round.order << ' '
            << round.pointsMultiplier << ' ' << quote(round.name) << '\n';

    const auto& execution = eventExecution;
    out << "EVENT_EXECUTION " << (execution.enabled ? 1 : 0) << ' ' << (execution.autoStagePlayer ? 1 : 0) << ' '
        << (execution.autoSavePersonalBests ? 1 : 0) << ' ' << execution.gridSettleSeconds << ' ' << execution.countdownSeconds << ' '
        << execution.falseStartSpeedKmh << ' ' << execution.gateDebounceSeconds << ' ' << execution.trackLimitGraceSeconds << ' '
        << execution.trackLimitRejoinSeconds << ' ' << execution.fullCourseYellowSpeedKmh << ' ' << execution.virtualSafetyCarSpeedKmh << ' '
        << execution.safetyCarSpeedKmh << ' ' << execution.resultsHoldSeconds << ' ' << (execution.practiceLoopEnabled ? 1 : 0) << ' '
        << (execution.practiceLoopAutoRestart ? 1 : 0) << ' ' << (execution.practiceLoopRestoreAngularVelocity ? 1 : 0) << ' '
        << (execution.practiceLoopRestoreGear ? 1 : 0) << ' ' << execution.practiceLoopEndGateWidthM << ' ' << execution.practiceLoopRestoreDelayS << '\n';
    message = "Saved " + file.string();
    return true;
}

bool StudioAuthoringData::loadGameplay(const std::filesystem::path& file, std::string& message)
{
    std::ifstream in;
    if (!openInput(file, in, message)) return false;
    std::string magic; int version = 0;
    if (!(in >> magic >> version) || magic != "HGAME") { message = "Invalid HGAME file."; return false; }
    gameEvents.clear();
    worldPoints.clear();
    policeGameplay = PoliceGameplayConfiguration{};
    policePatrolZones.clear();
    policeRoadblockSites.clear();
    policeEscapeZones.clear();
    clandestineMeets.clear();
    motorsport = MotorsportConfiguration{};
    motorsportAi = MotorsportAiConfiguration{};
    motorsportReplay = MotorsportReplayConfiguration{};
    motorsportClasses.clear();
    motorsportEntrants.clear();
    motorsportChampionships.clear();
    motorsportRounds.clear();
    eventExecution = EventExecutionConfiguration{};
    std::string keyword; std::uint32_t maxId = 0;
    while (in >> keyword)
    {
        if (keyword == "EVENT")
        {
            GameEvent event; int type = 0; int enabled = 1; int rolling = 0; int traffic = 0; int police = 0; int night = 0;
            if (!(in >> event.id >> type >> enabled >> event.startMarkerId >> event.finishMarkerId)) break;
            if (version >= 2)
            {
                if (!(in >> event.layoutId)) break;
            }
            if (!(in >> event.laps >> event.maxEntrants >> rolling >> traffic >> police >> night >> event.entryFee >> event.reward >> event.heat >> std::quoted(event.name))) break;
            event.type = clampEnum<GameEventType>(type, static_cast<int>(version >= 12 ? GameEventType::Gymkhana : GameEventType::TestDrive));
            event.enabled = enabled != 0;
            event.rollingStart = rolling != 0;
            event.trafficEnabled = traffic != 0;
            event.policeEnabled = police != 0;
            event.nightOnly = night != 0;
            maxId = std::max(maxId, event.id);
            gameEvents.push_back(event);
            continue;
        }
        if (keyword == "POINT")
        {
            WorldPoint point; int type = 0; int enabled = 1; int discoverable = 1; int fastTravel = 0;
            if (!(in >> point.id >> type >> enabled >> point.headingDeg >> point.radiusM >> discoverable >> fastTravel
                >> point.servicePriceMultiplier >> std::quoted(point.name))) break;
            if (!readVec3(in, point.position)) break;
            point.type = clampEnum<WorldPointType>(type, static_cast<int>(WorldPointType::ParkingArea));
            point.enabled = enabled != 0;
            point.discoverable = discoverable != 0;
            point.fastTravelEnabled = fastTravel != 0;
            maxId = std::max(maxId, point.id);
            worldPoints.push_back(point);
            continue;
        }
        if (version >= 3 && keyword == "POLICE_CONFIG")
        {
            int enabled=0, witnesses=1, speeding=1, collisions=1, illegalRaces=1, evasion=1;
            if (!(in >> enabled >> policeGameplay.maxHeatLevel >> policeGameplay.maxPursuitUnits >> policeGameplay.civilianWitnessRadiusM
                >> policeGameplay.policeDetectionRadiusM >> policeGameplay.speedToleranceKmh >> policeGameplay.heatDecayDelayS >> policeGameplay.heatDecayPerSecond
                >> policeGameplay.lostSightSeconds >> policeGameplay.searchDurationS >> policeGameplay.cooldownDurationS >> policeGameplay.bustHoldSeconds
                >> policeGameplay.backupDelayS >> policeGameplay.roadblockMinimumHeat >> witnesses >> speeding >> collisions >> illegalRaces >> evasion)) break;
            policeGameplay.enabled = enabled != 0; policeGameplay.civilianWitnesses = witnesses != 0; policeGameplay.speedingGeneratesHeat = speeding != 0;
            policeGameplay.collisionsGenerateHeat = collisions != 0; policeGameplay.illegalRacesGenerateHeat = illegalRaces != 0; policeGameplay.evasionEscalatesHeat = evasion != 0;
            continue;
        }
        if (version >= 3 && keyword == "PATROL")
        {
            PolicePatrolZone zone; int enabled=1;
            if (!(in >> zone.id >> enabled >> zone.radiusM >> zone.patrolWeight >> zone.maximumUnits >> zone.responseMultiplier >> zone.speedToleranceKmh
                >> zone.startHour >> zone.endHour >> zone.responsePortalId >> std::quoted(zone.name))) break;
            if (!readVec3(in, zone.position)) break; zone.enabled = enabled != 0; maxId = std::max(maxId, zone.id); policePatrolZones.push_back(zone); continue;
        }
        if (version >= 3 && keyword == "ROADBLOCK")
        {
            PoliceRoadblockSite site; int enabled=1, spike=1, gap=0;
            if (!(in >> site.id >> enabled >> site.nodeId >> site.headingDeg >> site.widthM >> site.minimumHeat >> site.unitCount >> spike >> gap
                >> site.selectionWeight >> std::quoted(site.name))) break;
            if (!readVec3(in, site.position)) break; site.enabled = enabled != 0; site.spikeStrip = spike != 0; site.leaveEscapeGap = gap != 0;
            maxId = std::max(maxId, site.id); policeRoadblockSites.push_back(site); continue;
        }
        if (version >= 3 && keyword == "ESCAPE")
        {
            PoliceEscapeZone zone; int enabled=1, los=1, safehouse=0;
            if (!(in >> zone.id >> enabled >> zone.radiusM >> zone.searchTimeMultiplier >> zone.heatDecayMultiplier >> los >> safehouse >> std::quoted(zone.name))) break;
            if (!readVec3(in, zone.position)) break; zone.enabled = enabled != 0; zone.breakLineOfSight = los != 0; zone.safehouse = safehouse != 0;
            maxId = std::max(maxId, zone.id); policeEscapeZones.push_back(zone); continue;
        }
        if (version >= 3 && keyword == "MEET")
        {
            ClandestineMeet meet; int enabled=1, discoverable=1;
            if (!(in >> meet.id >> enabled >> meet.headingDeg >> meet.radiusM >> meet.openHour >> meet.closeHour >> meet.maximumVehicles >> meet.policeRisk
                >> meet.heatMultiplier >> meet.eventId >> discoverable >> std::quoted(meet.name))) break;
            if (!readVec3(in, meet.position)) break; meet.enabled = enabled != 0; meet.discoverable = discoverable != 0;
            maxId = std::max(maxId, meet.id); clandestineMeets.push_back(meet); continue;
        }
        if (version >= 5 && keyword == "MOTORSPORT_CONFIG")
        {
            int enabled=1, ai=1, grid=1, simulate=1, multi=1, persist=1;
            if (!(in >> enabled >> ai >> grid >> simulate >> motorsport.maxPhysicalCompetitors >> motorsport.defaultAiSkill
                >> motorsport.qualifyingPaceSpreadPercent >> motorsport.baseMechanicalDnfChancePerHour >> multi >> persist)) break;
            motorsport.enabled=enabled!=0; motorsport.aiCompetitorsEnabled=ai!=0; motorsport.autoBuildGrid=grid!=0;
            motorsport.simulateUnspawnedCompetitors=simulate!=0; motorsport.multiClassTiming=multi!=0; motorsport.championshipPersistence=persist!=0; continue;
        }
        if (version >= 6 && keyword == "MOTORSPORT_AI")
        {
            int enabled=1, strategy=1, mistakes=1, slipstream=1, defending=1, multiclass=1, wetLine=1, telemetry=1;
            if (!(in >> enabled >> motorsportAi.updateHz >> motorsportAi.lookaheadMinimumM >> motorsportAi.lookaheadMaximumM
                >> motorsportAi.brakingLookaheadM >> motorsportAi.opponentAwarenessM >> motorsportAi.slipstreamMinimumGapM >> motorsportAi.slipstreamMaximumGapM
                >> motorsportAi.overtakeMinimumClosingKmh >> motorsportAi.defensiveTriggerGapM >> motorsportAi.blueFlagYieldGapM >> motorsportAi.wetLineThreshold
                >> motorsportAi.maximumWetSpeedPenalty >> motorsportAi.fuelUseLitersPer100Km >> motorsportAi.tireWearPer100Km >> motorsportAi.fuelReserveLaps
                >> motorsportAi.tirePitThreshold >> motorsportAi.mistakeRecoverySeconds >> strategy >> mistakes >> slipstream >> defending >> multiclass >> wetLine >> telemetry)) break;
            motorsportAi.enabled=enabled!=0; motorsportAi.strategyEnabled=strategy!=0; motorsportAi.mistakesEnabled=mistakes!=0;
            motorsportAi.slipstreamEnabled=slipstream!=0; motorsportAi.defendingEnabled=defending!=0; motorsportAi.multiclassNegotiation=multiclass!=0;
            motorsportAi.wetLineEnabled=wetLine!=0; motorsportAi.liveDecisionTelemetry=telemetry!=0; continue;
        }
        if (version >= 7 && keyword == "MOTORSPORT_AI_PHYSICS")
        {
            int fullPhysics=0, gripAware=1, spatial=1, trackLimits=1, damage=1, forecast=1;
            if (!(in >> fullPhysics >> motorsportAi.physicsHighRateHz >> motorsportAi.steeringLookaheadSeconds >> motorsportAi.steeringGain
                >> motorsportAi.crossTrackGain >> motorsportAi.throttleGain >> motorsportAi.brakeGain >> motorsportAi.maximumSteerAngleDeg
                >> motorsportAi.sideBySideSafetyM >> motorsportAi.trackLimitSafetyM >> motorsportAi.gripSlipRatioLimit >> motorsportAi.gripSlipAngleDeg
                >> motorsportAi.physicalRecoveryDistanceM >> motorsportAi.formationSpeedKmh >> motorsportAi.rollingStartSpeedKmh >> motorsportAi.pitLaneSpeedKmh
                >> motorsportAi.damagePitThreshold >> motorsportAi.damageDnfThreshold >> motorsportAi.collisionDamageScale >> motorsportAi.weatherForecastSeconds
                >> gripAware >> spatial >> trackLimits >> damage >> forecast)) break;
            motorsportAi.fullPhysicsCompetitors=fullPhysics!=0; motorsportAi.gripAwareBraking=gripAware!=0; motorsportAi.spatialAvoidance=spatial!=0;
            motorsportAi.trackLimitAwarePassing=trackLimits!=0; motorsportAi.damageStrategyEnabled=damage!=0; motorsportAi.weatherForecastEnabled=forecast!=0; continue;
        }
        if (version >= 8 && keyword == "MOTORSPORT_AI_RACECRAFT")
        {
            int colliderAuthority=1, predictive=1, divebomb=1, blocking=1, unsafeRelease=1, thermal=1, fuelMass=1, componentDamage=1;
            if (!(in >> colliderAuthority >> motorsportAi.collisionEnvelopeMarginM >> motorsportAi.sweptEnvelopeSeconds
                >> motorsportAi.sideBySideOverlapToleranceM >> motorsportAi.divebombCommitGapM >> motorsportAi.divebombClosingThresholdKmh
                >> motorsportAi.switchbackWindowS >> motorsportAi.maximumDefensiveMovesPerStraight >> motorsportAi.blockingPenaltySeconds
                >> motorsportAi.unsafeReleasePenaltySeconds >> motorsportAi.pitReleaseLookaheadM >> motorsportAi.multiclassPassHorizonS
                >> motorsportAi.tireOptimalMinimumC >> motorsportAi.tireOptimalMaximumC >> motorsportAi.fuelDensityKgPerLiter
                >> predictive >> divebomb >> blocking >> unsafeRelease >> thermal >> fuelMass >> componentDamage)) break;
            motorsportAi.colliderBoundsAuthority=colliderAuthority!=0; motorsportAi.predictiveCollisionAvoidance=predictive!=0;
            motorsportAi.divebombJudgement=divebomb!=0; motorsportAi.blockingRules=blocking!=0; motorsportAi.unsafeReleaseStewarding=unsafeRelease!=0;
            motorsportAi.tireThermalStrategy=thermal!=0; motorsportAi.fuelMassAwareness=fuelMass!=0; motorsportAi.componentDamageStrategy=componentDamage!=0;
            continue;
        }
        if (version >= 9 && keyword == "MOTORSPORT_AI_INCIDENTS")
        {
            int evidence=1, stewarding=1;
            if (!(in >> evidence >> stewarding >> motorsportAi.incidentMinimumNormalImpulseNs >> motorsportAi.incidentMinimumClosingKmh
                >> motorsportAi.severeIncidentNormalImpulseNs >> motorsportAi.severeIncidentClosingKmh
                >> motorsportAi.avoidableContactPenaltySeconds >> motorsportAi.severeContactPenaltySeconds
                >> motorsportAi.contactEvidenceCooldownSeconds >> motorsportAi.retainedIncidentEvidence)) break;
            motorsportAi.contactEvidenceEnabled=evidence!=0; motorsportAi.incidentStewardingEnabled=stewarding!=0;
            continue;
        }
        if (version >= 10 && keyword == "MOTORSPORT_REPLAY")
        {
            int enabled=1, capturePlayer=1, captureControls=1, ghostReview=1;
            if (!(in >> enabled >> motorsportReplay.sampleHz >> motorsportReplay.preRollSeconds >> motorsportReplay.postRollSeconds
                >> motorsportReplay.maximumIncidentClips >> motorsportReplay.maximumRecordedCompetitors >> capturePlayer >> captureControls
                >> ghostReview >> motorsportReplay.maximumGhostVehicles)) break;
            motorsportReplay.enabled=enabled!=0; motorsportReplay.capturePlayer=capturePlayer!=0; motorsportReplay.captureControls=captureControls!=0;
            motorsportReplay.ghostReviewEnabled=ghostReview!=0; continue;
        }
        if (version >= 11 && keyword == "MOTORSPORT_REPLAY_CAMERA")
        {
            int director=1, automatic=1;
            if (!(in >> director >> automatic >> motorsportReplay.incidentCameraDistanceM >> motorsportReplay.incidentCameraHeightM
                >> motorsportReplay.tracksideCameraLeadM >> motorsportReplay.helicopterCameraHeightM >> motorsportReplay.cameraSmoothing)) break;
            motorsportReplay.broadcastDirectorEnabled=director!=0; motorsportReplay.autoIncidentCamera=automatic!=0; continue;
        }
        if (version >= 5 && keyword == "MCLASS")
        {
            MotorsportClass cls; int enabled=1;
            if (!(in >> cls.id >> enabled >> cls.minimumPowerKw >> cls.maximumPowerKw >> cls.minimumWeightKg >> cls.maximumWeightKg >> cls.balanceBallastKg
                >> cls.maximumEntrants >> std::quoted(cls.code) >> std::quoted(cls.name))) break;
            cls.enabled=enabled!=0; maxId=std::max(maxId,cls.id); motorsportClasses.push_back(cls); continue;
        }
        if (version >= 5 && keyword == "ENTRANT")
        {
            MotorsportEntrant entrant; int enabled=1, clandestine=0;
            if (!(in >> entrant.id >> enabled >> entrant.eventId >> entrant.classId >> entrant.raceNumber >> entrant.aiSkill >> entrant.qualifyingPace
                >> entrant.racePace >> entrant.wetSkill >> entrant.aggression >> entrant.consistency >> entrant.pitSkill)) break;
            if (version >= 6)
            {
                if (!(in >> entrant.racecraft >> entrant.awareness >> entrant.defending >> entrant.tireManagement >> entrant.fuelManagement >> entrant.strategyRisk
                    >> entrant.mistakeRatePerHour >> entrant.reactionTimeS >> entrant.preferredLineBias)) break;
            }
            if (!(in >> clandestine >> entrant.gridOverride >> std::quoted(entrant.driverName) >> std::quoted(entrant.teamName) >> std::quoted(entrant.vehiclePreset))) break;
            entrant.enabled=enabled!=0; entrant.clandestine=clandestine!=0; maxId=std::max(maxId,entrant.id); motorsportEntrants.push_back(entrant); continue;
        }
        if (version >= 5 && keyword == "CHAMPIONSHIP")
        {
            MotorsportChampionship championship; int enabled=1;
            if (!(in >> championship.id >> enabled >> championship.classId >> championship.poleBonus >> championship.fastestLapBonus
                >> championship.dropWorstRounds >> std::quoted(championship.pointsScheme) >> std::quoted(championship.name))) break;
            championship.enabled=enabled!=0; maxId=std::max(maxId,championship.id); motorsportChampionships.push_back(championship); continue;
        }
        if (version >= 5 && keyword == "ROUND")
        {
            MotorsportRound round; int enabled=1;
            if (!(in >> round.id >> round.championshipId >> round.eventId >> enabled >> round.order >> round.pointsMultiplier >> std::quoted(round.name))) break;
            round.enabled=enabled!=0; maxId=std::max(maxId,round.id); motorsportRounds.push_back(round); continue;
        }
        if (version >= 4 && keyword == "EVENT_EXECUTION")
        {
            int enabled=1, stage=1, saveBests=1, loopEnabled=1, loopRestart=1, loopAngular=1, loopGear=1;
            if (!(in >> enabled >> stage >> saveBests >> eventExecution.gridSettleSeconds >> eventExecution.countdownSeconds
                >> eventExecution.falseStartSpeedKmh >> eventExecution.gateDebounceSeconds >> eventExecution.trackLimitGraceSeconds
                >> eventExecution.trackLimitRejoinSeconds >> eventExecution.fullCourseYellowSpeedKmh >> eventExecution.virtualSafetyCarSpeedKmh
                >> eventExecution.safetyCarSpeedKmh >> eventExecution.resultsHoldSeconds >> loopEnabled >> loopRestart >> loopAngular >> loopGear
                >> eventExecution.practiceLoopEndGateWidthM >> eventExecution.practiceLoopRestoreDelayS)) break;
            eventExecution.enabled = enabled != 0; eventExecution.autoStagePlayer = stage != 0; eventExecution.autoSavePersonalBests = saveBests != 0;
            eventExecution.practiceLoopEnabled = loopEnabled != 0; eventExecution.practiceLoopAutoRestart = loopRestart != 0;
            eventExecution.practiceLoopRestoreAngularVelocity = loopAngular != 0; eventExecution.practiceLoopRestoreGear = loopGear != 0;
            continue;
        }
        std::string ignore; std::getline(in, ignore);
    }
    m_nextId = std::max(m_nextId, maxId + 1);
    if (motorsportClasses.empty()) addMotorsportClass("Open");
    message = "Loaded " + file.string() + " (HGAME v" + std::to_string(version) + ")";
    return true;
}


} // namespace heritage::studio::authoring
