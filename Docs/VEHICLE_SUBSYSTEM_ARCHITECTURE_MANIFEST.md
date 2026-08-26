# Heritage Engine Vehicle Subsystem Architecture Manifest

Status: architectural intent only. This file replaces the old empty/noncompiled `.cpp` scaffolds retired by OPT01.

## Rule

**Create source only when implementation exists.** A planned mechanism is documented here until it has a real contract, runtime owner, tests and a compiled translation unit. Empty `.cpp` files are not architecture.

The common vehicle core remains arbitrary-wheel-count. Topology-specific coupling is added only where the physics genuinely differs; reusable tires, contacts, suspension, steering, brakes and powertrain stay shared.

## Planned native seams

| Domain | Planned mechanism seams | Activation rule |
|---|---|---|
| Core | Vehicle assembly; simulation scheduling | Extract when `VehicleSystem` orchestration has a stable independent responsibility and regression coverage |
| Steering | Steering system; Ackermann geometry; power steering | Implement as reusable mechanisms, not four-wheel assumptions |
| Suspension common | Spring/damper; bump stop; heave spring | Create when the current suspension model needs an independently testable mechanism owner |
| Suspension geometry | Double wishbone; semi-trailing arm; multi-link | Geometry solver owns kinematics only; force elements remain reusable |
| Axles | Solid axle; torsion beam | Topology/constraint-specific kinematics only |
| Springs | Coil; leaf; air; hydropneumatic | Constitutive element models with common suspension interfaces |
| Motorcycle suspension | Telescopic fork; Telelever; rear swingarm; rear linkage | Native physics, shared tire/contact interfaces |
| Wheels | Wheel dynamics; wheel hub; clearance; installed wheel mass properties | Add when these become independent from existing compiled owners |
| Tires | Low-pressure tire model | Add as an explicit tire specialization only when implemented/tested |
| Drivetrain | Power unit; clutch; gearbox; transfer case; chain final drive | Reusable driveline graph components |
| Differentials | Open; limited-slip; locked; active | One common differential contract with mechanism-specific implementations |
| Brakes | Brake system; brake thermal; ABS | Separate hydraulic/torque, thermal and controller responsibilities |
| Aerodynamics | Aerodynamics system; aero surface; **Ground effect** | Force-producing aero elements with a coordinator, not monolithic vehicle code |
| Dynamics | Motorcycle lean dynamics; kart chassis flex | Whole-vehicle coupling that cannot live cleanly in generic components |
| Articulation | Articulated vehicle; fifth wheel; trailer coupling | Constraint/articulation mechanisms reusable by trucks/trailers |
| Topology | Common coordinator; **Two-wheel topology**; three-wheel topology; four-plus-wheel topology | Only topology-specific whole-vehicle coupling belongs here |

## Lua topology intent

Racing United does not keep empty Lua modules for hypothetical topology types. Vehicle authoring remains data-driven. When a topology-specific Lua orchestration layer is actually needed, it should expose a narrow `RU.Vehicle.Topology.*` module and contain real behavior or configuration.

See `VEHICLE_TOPOLOGY_ARCHITECTURE.md` for the wheel-count/topology contract.
