# STUDIO21 — Full-Physics Racing AI & Racecraft

STUDIO21 connects the STUDIO20 Racing AI decision layer to native Heritage Vehicle physics without deleting the logical competitor path used for large fields.

## Architecture

The authority chain is now:

`venue / AI lines -> RacingAI intent -> RacingAIVehicleController -> Heritage Vehicle -> physical route projection -> event timing / classification`

Competitors outside the physical budget continue to use the scalable logical simulation. A physical competitor can therefore be upgraded or downgraded later without changing its entrant, championship, strategy or racecraft identity.

## Native race-car controller

When **Use native Heritage Vehicle dynamics for physical competitors** is enabled, physical-budget competitors receive:

- dynamic rigid-body chassis;
- collision body;
- four native suspension/wheel contacts;
- native tire/contact telemetry;
- high-rate Vehicle physics;
- steering, throttle and braking through `Vehicle.SetInputs`;
- automatic gear requests;
- ABS/TC driver aids;
- speed-sensitive lookahead and cross-track correction.

The placeholder debug body is presentation only. `vehiclePreset` remains the future asset/factory key, allowing the controller to be replaced by complete authored race vehicles without rewriting race intelligence.

## Physical authority

Native competitors no longer advance because a logical distance counter says they did. Their body is projected onto the authored race route every fixed update. Chassis speed and route projection feed the event participant progress.

A lap crossing for a closed circuit requires the physical route projection to wrap from the final portion of the route to the first portion while the chassis is moving forward. Open routes finish at the physical route end.

## Grip-aware control

The controller reads per-wheel:

- grounded state;
- slip ratio;
- slip angle;
- surface wetness.

Those signals generate a bounded grip estimate used to temper throttle/brake requests. The original STUDIO20 desired-speed and braking intent remains the strategic authority.

## Racecraft safety

STUDIO21 adds:

- configurable track-limit safety margin for lateral pass/defend offsets;
- side-by-side overlap spacing;
- physical cross-track correction;
- formation-lap and rolling-start speed caps;
- gross-divergence recovery for pathological physics/authoring situations.

## Physical pit-lane traversal

When a venue layout has an authored pit route and a full-physics competitor accepts a pit request, the physical chassis changes path authority to that pit spline instead of receiving an instantaneous logical service. The final STUDIO21 path is anchored by the race infrastructure already authored in Studio:

- **Pit Entry** is projected onto the main race spline and arms the physical lane switch at the real entry location;
- the car follows the authored pit spline and obeys the STUDIO21 AI pit-lane speed cap;
- its assigned **Pit Box** is projected onto the pit spline and becomes the physical service-stop location;
- **Start / Finish** is projected across the pit spline so a car crossing the lap line through pit lane receives the correct lap;
- **Pit Exit** is projected onto both pit and main splines and becomes the physical rejoin authority;
- after service, the car continues down pit lane and rejoins the main race route at the authored exit.

Logical/off-budget competitors retain the cheaper immediate service model. This keeps visually important nearby cars physical without forcing the whole field through expensive pit physics.

## Damage-aware strategy

A contact combined with substantial instantaneous speed loss can reduce a physical competitor's mechanical-health estimate. This is intentionally a foundation rather than the final deformation/damage model.

- below the pit threshold, Racing AI may request damage repair;
- below the DNF threshold, the competitor retires;
- physical control power is reduced as severe damage accumulates.

Future component damage can replace this coarse health source while retaining the strategy interface.

## Weather forecast

Racing AI maintains a smoothed surface-wetness trend and projects it over the authored short forecast horizon. Rising/falling wetness can therefore trigger a tire crossover decision before the current surface value alone would have done so.

## Debugging

The RACE prototype panel reports, per competitor:

- backend type;
- actual throttle / brake / steering;
- grip estimate;
- grounded wheels;
- maximum slip ratio / slip angle;
- cross-track error;
- mechanical health;
- body contact count;
- physical recovery count;
- forecast wetness and trend;
- existing STUDIO20 decision/strategy telemetry.

## Compatibility

HGAME v7 adds `MOTORSPORT_AI_PHYSICS`. HGAME v6 and older load with safe STUDIO21 defaults. Full-physics Racing AI is disabled by default so existing projects retain STUDIO20 behavior until deliberately enabled.
