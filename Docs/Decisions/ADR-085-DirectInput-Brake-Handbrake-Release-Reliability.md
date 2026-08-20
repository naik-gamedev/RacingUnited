# ADR-085 — DirectInput Brake / Handbrake Release Reliability

## Status
Accepted for INPUT05.

## Context
Heritage can bind Logitech G29 steering, pedals and buttons through DirectInput,
but a live vehicle could occasionally start or remain with Brake or Handbrake
active even when the corresponding physical control was released. Vehicle
physics does not latch those controls: the problem is in hardware-state
interpretation before the action reaches the vehicle.

Three DirectInput details matter:

1. `DIJOYSTATE2::rgbButtons` uses bit 7 as the pressed state. Treating any
   non-zero byte as pressed is not a valid DirectInput button-state test.
2. Heritage historically captured an axis neutral from the first successful
   device poll. A wheel driver can transiently report a non-rest value during
   startup/acquire, leaving a centred steering or combined pedal axis biased for
   the rest of the session.
3. Binding capture already knows the exact released position at the moment the
   user clicks a binding cell. For asymmetric/inverted pedal axes that position
   is a better session neutral than a startup sample.

## Decision

- DirectInput buttons are active only when `(rgbButtons[n] & 0x80) != 0`.
- Centred axes are allowed to repair a bad startup neutral only after eight
  consecutive samples within ±0.06 of canonical centre. This is deliberately
  conservative so simply crossing centre does not continually recalibrate an
  axis.
- A 0.015 neutral noise deadzone is applied before directional axis scaling.
- When an axis binding is captured, the baseline present when capture started is
  retained as that axis's session neutral. This supports end-stop-resting and
  inverted pedals such as clutch/throttle/brake axes.
- No vehicle-physics brake latch or Lua-side forced release is added. Input
  hardware state remains the authority.

## Consequences

A G29 combined accelerator/brake axis that returns to centre can recover from a
bad initial DirectInput sample instead of leaving one pedal partially active.
DirectInput buttons cannot become held merely because a driver uses unspecified
low bits in the state byte. Newly bound asymmetric pedals use the actual released
position observed during binding.

The existing action model still permits several legitimate bindings per action.
If a future live report shows an unrelated secondary controller continuously
asserting Brake/Handbrake, that should be handled as explicit input-source
arbitration with diagnostics rather than by hiding values in vehicle physics.
