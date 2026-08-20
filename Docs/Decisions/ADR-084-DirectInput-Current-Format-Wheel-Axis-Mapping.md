# ADR-084 — DirectInput Current-Format Wheel Axis Mapping

## Status
Accepted for INPUT04.

## Context
Heritage uses GLFW for standard gamepads and a Windows DirectInput 8 backend for
racing peripherals. The original wheel backend enumerated DirectInput objects and
interpreted `DIDEVICEOBJECTINSTANCE::dwOfs` as a `DIJOYSTATE2` offset. Microsoft
defines that enumerated `dwOfs` as an offset in the device's native/raw data
format, while `DIPH_BYOFFSET` addresses the application's current data format.
Those are not interchangeable. A driver-specific HID layout can therefore make
otherwise valid wheel axes impossible to bind even though the device itself is
present and other axes happen to work.

## Decision
After `SetDataFormat(c_dfDIJoystick2)`, Heritage discovers the eight canonical
axis slots (`lX`, `lY`, `lZ`, `lRx`, `lRy`, `lRz`, `rglSlider[0]`,
`rglSlider[1]`) using `GetObjectInfo(..., DIPH_BYOFFSET)` on their offsets in the
current data format. Range and dead-zone properties are configured and queried
using the same current-format offsets.

Binding capture snapshots every present axis at capture start and selects the
single axis with the largest deliberate movement over threshold, rather than
binding the first axis that happens to cross the threshold.

The generic DirectInput route remains authoritative for wheel input. Logitech's
Steering Wheel SDK may be added later as an optional vendor feature/force-feedback
provider, but basic G29 steering/pedal input must not require proprietary SDK
runtime integration.

## Consequences
- Logitech G29 steering (`lX`) and clutch slider presentation can be discovered
  through the same canonical `DIJOYSTATE2` path as other racing peripherals.
- Driver-native object layout no longer leaks into Heritage's axis mapping.
- Existing bindings remain textual `DInput[instance-guid]:Axis...+/-` values.
- Hardware acceptance remains necessary on Windows because CI does not have the
  physical wheel or Logitech driver stack.
