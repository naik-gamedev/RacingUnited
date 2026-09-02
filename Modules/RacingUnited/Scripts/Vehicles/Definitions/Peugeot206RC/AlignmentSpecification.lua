-- PEUGEOT_SUSP01: published 206 GTI 180 / RC alignment envelope. Peugeot's
-- workshop data for the 206 family corroborates the sign and adjustment policy;
-- the exact RC ranges below come from the named alignment-equipment database.
-- Midpoints are stock runtime references, not claimed production-line targets.
-- Factory ranges never clamp custom, race, damaged, or historical setups.

Peugeot206RCAlignmentSpecification = {
    provenance = "peugeot_workshop_manual_plus_rc_alignment_database",
    confidence = 0.85,
    sources = {
        {
            kind = "manufacturer_workshop_data",
            url = "https://www.clubpeugeot.es/html/206/info/sp/b3cb0ik7.htm",
            note = "206 family reference-height alignment and adjustment policy"
        },
        {
            kind = "rc_specific_alignment_database",
            url = "https://www.jltechno.com/en/alignment_specs.php?ModelID=615344&ModelName=206%C2%A0GTI%C2%A0180%2FRC&brand=PEUGEOT",
            note = "206 GTI 180/RC 2002-2006 minimum/maximum values"
        }
    },
    front = {
        totalToeDegrees = { minimum = -0.20, maximum = -0.03 },
        toeInPerWheelDegrees = { minimum = -0.10, maximum = -0.02 },
        camberDegrees = { minimum = -0.50, maximum = 0.50 },
        casterDegrees = { minimum = 2.70, maximum = 3.70 },
        steeringAxisInclinationDegrees = { minimum = 9.20, maximum = 10.20 },
        toeAdjustable = true,
        camberAdjustable = false,
        casterAdjustable = false
    },
    rear = {
        totalToeDegrees = { minimum = 0.43, maximum = 0.60 },
        toeInPerWheelDegrees = { minimum = 0.22, maximum = 0.30 },
        camberDegrees = { minimum = -1.50, maximum = -0.50 },
        toeAdjustable = false,
        camberAdjustable = false,
        casterAdjustable = false
    }
}

Peugeot206RCReferenceAlignment = {
    provenance = "midpoint_of_published_peugeot_206_rc_spec_range",
    frontCasterDegrees = 3.200000,
    frontSteeringAxisInclinationDegrees = 9.700000,
    frontCamberDegrees = 0.0,
    frontToeOutPerWheelDegrees = 0.060000,
    rearCamberDegrees = -1.0,
    rearToeInPerWheelDegrees = 0.260000
}

Peugeot206RCWorkshopAlignmentDefault = {
    -- The neutral GLB remains immutable reference geometry. This separate setup
    -- layer applies the midpoint of the published stock envelope at runtime.
    provenance = "stock_midpoint_of_published_peugeot_206_rc_spec_range",
    confidence = 0.85,
    front = {
        camberDegrees = 0.0,
        toeInDegrees = -0.060000,
        casterDegrees = 3.200000,
        toeAdjustable = true,
        camberAdjustable = false,
        casterAdjustable = false
    },
    rear = {
        camberDegrees = -1.0,
        toeInDegrees = 0.260000,
        casterDegrees = 0.0,
        toeAdjustable = false,
        camberAdjustable = false,
        casterAdjustable = false
    }
}
