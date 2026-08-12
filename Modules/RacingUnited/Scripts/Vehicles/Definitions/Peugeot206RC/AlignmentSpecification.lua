-- ALIGN01: Peugeot 206 RC alignment evidence supplied as a MIN/MAX specification
-- table by the project author. No standard/nominal column was present in the
-- supplied table, so midpoint values below are explicitly workshop defaults,
-- not claimed factory targets. Factory ranges are reference evidence only and
-- never clamp custom/race/historical setup values.

Peugeot206RCAlignmentSpecification = {
    provenance = "user_supplied_peugeot_206_rc_alignment_spec_table",
    confidence = 0.65,
    front = {
        totalToeDegrees = { minimum = -0.20, maximum = -0.03 },
        toeInPerWheelDegrees = { minimum = -0.10, maximum = -0.02 },
        camberDegrees = { minimum = -0.50, maximum = 0.50 },
        casterDegrees = { minimum = 2.70, maximum = 3.70 },
        steeringAxisInclinationDegrees = { minimum = 9.20, maximum = 10.20 }
    },
    rear = {
        totalToeDegrees = { minimum = 0.43, maximum = 0.60 },
        toeInPerWheelDegrees = { minimum = 0.22, maximum = 0.30 },
        camberDegrees = { minimum = -1.50, maximum = -0.50 }
    }
}

Peugeot206RCReferenceAlignment = {
    provenance = "midpoint_of_user_supplied_peugeot_206_rc_spec_range",
    frontCasterDegrees = 3.200000,
    frontSteeringAxisInclinationDegrees = 9.700000,
    frontCamberDegrees = 0.0,
    frontToeOutPerWheelDegrees = 0.060000,
    rearCamberDegrees = -1.0,
    rearToeInPerWheelDegrees = 0.260000
}

Peugeot206RCWorkshopAlignmentDefault = {
    provenance = "midpoint_of_user_supplied_peugeot_206_rc_spec_range",
    confidence = 0.65,
    front = {
        camberDegrees = 0.0,
        toeInDegrees = -0.060000,
        casterDegrees = 3.200000,
        casterAdjustable = true
    },
    rear = {
        camberDegrees = -1.0,
        toeInDegrees = 0.260000,
        casterDegrees = 0.0,
        casterAdjustable = false
    }
}
