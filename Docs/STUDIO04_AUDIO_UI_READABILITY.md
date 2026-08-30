# STUDIO04 - Audio UI Readability Pass

## Goal
Make Heritage Studio's audio authoring page readable enough to use without memorizing what each slider does.

## Delivered
- Dedicated right-hand **description column** for important slider rows.
- Matching **hover tooltip help** on parameter labels and slider values.
- **Collapsible sections** for:
  - Engine Core / Combustion
  - Intake Architecture
  - Exhaust Architecture
  - Cabin Construction
  - Source Character
  - Raw / Electric Harshness
  - Engine / Intake
  - Exhaust
  - Cabin / World Preview
- Slightly rebalanced audio workspace widths so the center authoring lane has more usable room.
- Clearer top-level copy explaining that the center lane is the readable authoring lane.

## Notes
- This pass focuses on readability and information hierarchy, not adding more DSP complexity.
- A future pass can add a packaged heading font such as Orbitron SemiBold once the font asset is formally added to the repository.
- The current implementation keeps explanatory text readable by using the default UI font for dense descriptions.
