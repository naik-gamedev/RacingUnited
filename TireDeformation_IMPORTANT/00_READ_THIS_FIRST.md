# TireDeformation_IMPORTANT — READ THIS FIRST

This folder is the handoff/postmortem for Heritage Engine / Racing United tire deformation work through **TIRE27 (2026-08-12)**.

## Absolute priority

The user currently cares about **visible, correct 3D tire deformation above everything else**. Do not spend another iteration improving thermals, wear, Pacejka coefficients, wall-climb traction, telemetry, or architecture polish unless that work is strictly required to make the visible tire deform correctly.

The acceptance test is simple:

1. A stationary loaded tire on flat pavement visibly forms a plausible loaded footprint.
2. A sidewalk edge running longitudinally under the center of the tread visibly indents the center of the tread while adjacent shoulder/sidewall regions respond smoothly.
3. A lower-front tread/shoulder region pushed into a curb or wall visibly deforms around the collider instead of clipping through it.
4. No spikes, exploding polygons, or whole-tire rigid translation masquerading as deformation.
5. The bead/rim region remains constrained.

If these are not visible in the live Windows game, **the attempt is a failure even if unit tests, GLSL compilation, telemetry, or isolated shader tests pass**.

## Important live observation

TIRE23 conclusively proved that `EntityMeshShaders.hpp` is the live render path: an intentionally forced ~45 mm lower-half squash appeared on all four player tires. Therefore do NOT waste time questioning the tire shader path again. TIRE24 then failed to make real sidewalk geometry influence that proven shader, which shifted the highest-priority suspicion to the collider-data bridge. The render-facing collider cache was refreshed in wheel substep phase 06, but earlier phases can return before phase 06 during missing-support / beyond-reach / airborne transitions. TIRE25 therefore refreshes collider triangles directly from the authoritative scene BVH in the native render-binding call instead of trusting that cache as the sole source. TIRE25 also failed live. The next concrete mismatch was then found: the visible embedded wheel intentionally follows the current/interpolated chassis + authored bind pose, while TIRE25 still gathered collision geometry around `WheelState.worldCenter`, a pre-integration state the source itself warns can be far from the visible wheel at speed. TIRE26 therefore abandons absolute triangle transfer and builds a 9x7 lower-half deformation lattice with direct CollisionSystem sphere casts around a render-policy wheel centre; only scalar compression crosses into GLSL.

## Current baseline warning

The user reverted to `RacingUnited(20260812-083256).zip`, an older stable tree. Several later patches had previously modified validator scripts. Overlaying an old archive on an existing directory without deleting stale files can leave **newer validator files behind**. This happened during TIRE22R: seven validation failures referred to TIRE17C8/TIRE20/TIRE21 checks that do not exist in the reverted archive's validator scripts. TIRE22R1 therefore ships the complete validator set from the reverted baseline (with only the CollisionSystem line guard adjusted) to remove stale cross-branch validation rules.

## Do not infer success from tests alone

A recurring failure pattern was: compile/test/shader smoke test says PASS, but the live car shows no meaningful deformation. Future work must instrument and verify the **actual live driven tire render path**.

Read `01_ATTEMPTS_CHRONOLOGICAL.md` next.


## TIRE26A live-debug key

Press **INSERT** to toggle the on-tire 9x7 probe visualization. Grey means no measured compression, green means shallow active contact, yellow means medium compression, and red means deep compression. A faint cyan lattice makes the exact probe topology visible directly on the rubber. Use this before wireframe for contact/deformation debugging.

## Current live state after TIRE26B / next target TIRE27

TIRE26B finally produced visible real-scene tire deformation and a working on-tire `INSERT` probe overlay while the wheel was against the sidewalk. That means the current probe bridge and live shader path are proven. The remaining visible problem is coarse spatial resolution: 9x7 samples create large scalloped deformation regions. TIRE27 keeps that exact architecture and replaces it with a **bottom-biased 21x13 lattice (273 samples)**, concentrating circumference resolution between 65 and 115 degrees around the bottom and doubling width detail. Do not restart the contact bridge unless new evidence disproves it.
