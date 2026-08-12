-- Versioned, topology-first vehicle authoring contract.
-- Classification selects an editor template only. Simulation support is
-- determined by the components and their connections, never by `if car` or
-- `if motorcycle` branches in the generic vehicle loader.
VehicleDefinitionV2 = {
    schemaVersion = 2,
    templateOrder = {
        "road_car", "formula", "indycar", "kart", "sprint_car",
        "atv", "motorcycle", "truck", "twin_engine", "custom"
    },
    templates = {
        road_car = {
            label = "ROAD CAR", classification = "car", massKg = 1100.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 6,
            driveLayout = "fwd", engineLocation = "front",
            suspensionProvider = "linear_raycast_v1",
            maximumTorqueNm = 250.0
        },
        formula = {
            label = "FORMULA", classification = "formula", massKg = 795.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 8,
            driveLayout = "rwd", engineLocation = "mid",
            suspensionProvider = "pushrod_double_wishbone_v1",
            maximumTorqueNm = 500.0
        },
        indycar = {
            label = "INDYCAR", classification = "indycar", massKg = 770.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 6,
            driveLayout = "rwd", engineLocation = "mid",
            suspensionProvider = "pushrod_double_wishbone_v1",
            maximumTorqueNm = 530.0
        },
        kart = {
            label = "GO-KART", classification = "kart", massKg = 165.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 1,
            driveLayout = "rwd", engineLocation = "rear",
            suspensionProvider = "kart_chassis_flex_v1",
            maximumTorqueNm = 22.0
        },
        sprint_car = {
            label = "SPRINT CAR", classification = "sprint_car", massKg = 625.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 1,
            driveLayout = "rwd", engineLocation = "front",
            suspensionProvider = "live_axle_torsion_v1",
            maximumTorqueNm = 900.0
        },
        atv = {
            label = "ATV", classification = "atv", massKg = 330.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 5,
            driveLayout = "awd", engineLocation = "mid",
            suspensionProvider = "double_wishbone_v1",
            maximumTorqueNm = 65.0
        },
        motorcycle = {
            label = "MOTORCYCLE", classification = "motorcycle", massKg = 205.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 2, forwardGearCount = 6,
            driveLayout = "rwd", engineLocation = "mid",
            suspensionProvider = "motorcycle_linkage_v1",
            maximumTorqueNm = 120.0, requiresLeanDynamics = true
        },
        truck = {
            label = "TRUCK", classification = "truck", massKg = 7500.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 6, forwardGearCount = 12,
            driveLayout = "rwd", engineLocation = "front",
            suspensionProvider = "live_axle_leaf_v1",
            maximumTorqueNm = 2200.0
        },
        twin_engine = {
            label = "TWIN ENGINE", classification = "car", massKg = 735.0,
            bodyCount = 1, powerUnitCount = 2, transmissionCount = 2,
            contactUnitCount = 4, forwardGearCount = 4,
            driveLayout = "split", engineLocation = "distributed",
            suspensionProvider = "linear_raycast_v1",
            maximumTorqueNm = 54.0
        },
        custom = {
            label = "CUSTOM", classification = "custom", massKg = 1000.0,
            bodyCount = 1, powerUnitCount = 1, transmissionCount = 1,
            contactUnitCount = 4, forwardGearCount = 5,
            driveLayout = "rwd", engineLocation = "front",
            suspensionProvider = "linear_raycast_v1",
            maximumTorqueNm = 200.0
        }
    }
}
