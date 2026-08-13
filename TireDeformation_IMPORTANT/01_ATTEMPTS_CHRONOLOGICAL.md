# Tire deformation attempts — chronological record

This is a numbered history of the major attempts discussed/tested in Racing United Dev Part 5/6. Names reflect the milestone/archive names used during development.

## 1. Pre-TIRE17 / early visual deformation

**Goal:** Show loaded tire flattening and basic carcass deformation.

**What existed / what was observed:**
- Some pressure/inflation-deflation deformation path could visibly change tire geometry.
- A small/minimal lateral tire deformation was visible in the live game.
- The tire could still sink into road/sidewalk collider geometry.
- Visual result remained mostly rigid/round under real scene contact.

**Lesson:** The mesh/shader can deform, but real collider contact was not reliably driving that deformation.

---

## 2. TIRE17C7 — real nearby collider triangles to GPU (earlier experimental path)

**Goal:** Send nearby real scene collider triangles to the tire shader and deform tire vertices against actual geometry rather than only a scalar loaded-radius/heightfield approximation.

**Logic:**
- Gather a bounded neighborhood of real collision triangles around each tire.
- Upload triangle data to GPU.
- Let tire shader use those triangles to push visible rubber away from colliders.

**Live result:** Did not produce correct sidewalk wrapping. Tire still sank/clipped.

**Lesson:** Merely uploading triangles is not enough; coordinate space, candidate selection, active render path, and deformation constraint logic all have to be correct.

---

## 3. TIRE17C8 / VIS11 — camera-relative collider-space correction

**Hypothesis:** The visible tire was camera-relative while collider triangles were absolute world-space, making tire-vs-triangle tests geometrically meaningless.

**Changes:**
- Convert collider triangle A/B/C to the same camera-relative space as the main `uModel` render path.
- Apply equivalent correction to shadow rendering.
- Improve triangle ranking using true closest point on finite triangle surface instead of only triangle vertices/centroid.
- Add regression for very large triangles whose vertices are far from the wheel while the triangle surface passes nearby.

**Live result:** Still wrong. One tire could glitch/explode into spikes; other regions still failed to deform correctly.

**Lesson:** A real coordinate-space bug existed, but fixing it did not solve the complete deformation problem.

---

## 4. TIRE17C9 / VIS12 — bounded/elastic non-cumulative triangle deformation

**Hypothesis:** One tire vertex was being successively projected by many nearby triangle planes, causing accumulated displacement and extreme spikes.

**Changes:**
- Stop cumulative projection through every triangle.
- Evaluate contacts from the original vertex and choose a dominant constraint.
- Bound correction magnitude.
- Bias deformation inward toward the rim/hub instead of arbitrary rigid-plane pushes.
- Keep bead region anchored.
- Add metric edge feathering.
- Preserve broad carcass/curb deformation instead of disabling it when exact triangles existed.
- Apply road-plane authority last to avoid pushing the tire back through pavement.

**Live result:** Still did not deliver the required correct 3D contact deformation.

**Lesson:** Preventing explosions is necessary, but the underlying live system still was not producing the desired geometry response.

---

## 5. Research / architecture pivot — contact is not only below the tire

**Trigger:** The user explicitly required physically meaningful contact around the tire, including front/side/diagonal contacts for curbs, walls, and rock crawling.

**Important conceptual correction:**
- Pacejka / Magic Formula is a force/moment model, not a 3D collision detector.
- Contact geometry must first detect where the tire touches the world and establish local contact frames.
- Tread contacts may use Magic Formula locally where appropriate; sidewall/carcass crushing is a structural/contact problem.
- Multiple simultaneous contacts are required.

**User's intended topology:** lower half of tire only for expensive contact/deformation, mirrored across width, with direct bottom contacts. The same contacts should drive both visual deformation and physics/traction.

---

## 6. TIRE18 — omnidirectional 3D carcass contact experiment

**Goal:** Make tire contact physically aware of arbitrary 3D directions rather than only downward support.

**Planned/implemented ideas:**
- Conceptual 32 circumferential × 7 cross-section structure.
- Multiple contact manifold entries.
- Tread/shoulder contacts use local tire-force logic; sidewall contacts use structural/friction response.
- Feed physical contact state to visual deformation.

**Live result:** No meaningful visible deformation. Tire could still sink into scene geometry.

**Lesson:** The newly described physical architecture was not becoming authoritative in the live driven-car visual path.

---

## 7. TIRE19 — continuous 3D carcass shell / render-locked contact mapping

**Hypotheses:**
- Discrete 32-sector collision probes could miss narrow features between sectors.
- Raised horizontal surfaces could be incorrectly rejected as duplicate ground because their normals resembled the road.
- Physics sent absolute contact positions while rendered wheels used a different/current pose, causing contact kernels to miss the visible tire.

**Changes:**
- Treat collision as continuous analytic shell rather than only 224 discrete points.
- Keep raised same-normal geometry distinct from the primary road.
- Transfer contact in tire structural coordinates (width/circumference/local normal) instead of absolute world contact position.
- Retain exact-triangle anti-penetration fallback.
- Add contact telemetry/diagnostics.

**Build issue:** Initial TIRE19 validator expected an old hard-coded Lua table preallocation size. TIRE19A fixed the stale validator.

**Live result:** Still no satisfactory deformation.

**Lesson:** More sophisticated contact representation did not fix the fact that the live render result remained effectively legacy.

---

## 8. TIRE20 — lower-hemisphere 9×7 unified physical contact

**Goal:** Match the user's desired lower-half topology and unify bottom road support with curb/rock contacts.

**Topology:**
- 9 circumferential stations over lower half.
- 7 bands across tire width.
- 63 structural/contact loci per tire.
- Continuous shell between loci so narrow features are not missed purely because they fall between samples.

**Physics intent:**
- Primary bottom road support and secondary 3D contacts belong to one physical tire state.
- Tread/shoulder contacts can generate local MF6.2 force contributions.
- Sidewall contacts get structural/friction response.
- Actual contact position produces force and `r × F` moment.

**Visual intent:**
- Remove renderer-only collision cheats and drive deformation from the physical contact state.

**Build issue:** A stale validator still demanded the older TIRE17C9 renderer-side exact-collider path. TIRE20A relaxed/superseded that validator.

**Live result:** Tire still appeared round/undeformed and could sink below scene collider.

**Lesson:** The new physical tire system was still not visibly authoritative in the live driven vehicle.

---

## 9. TIRE21 — authoritative lower-shell wall traction + VIS13

**Goal:** Prove that a driven tread contact against a vertical wall could produce local traction along the wall and that physical bottom support could directly deform the final shader.

**Changes / hypotheses:**
- Physical lower tire shell detects wall/curb contact.
- Construct local contact plane; MF6.2 used for tread/shoulder contact in that local frame.
- Add bounded penetration stabilization.
- Expand detailed candidate consideration so dense flat-road triangles do not starve nearby wall/curb triangles.
- Make final tire shader read physical bottom-support compression directly rather than relying only on a possibly stale scalar tire-deflection channel.

**Test work:** Isolated/native wall-traction and GLSL deformation tests were described as passing.

**Build issue:** Validator corpus omitted the tire regression file, so TIRE21A fixed validation discovery.

**Live result after preceding architecture:** user remained unconvinced / no proper visible deformation. The branch was then abandoned in favor of an older stable archive.

**Critical lesson:** Isolated tests can pass while the actual live vehicle still uses an unchanged-looking path. The live driven-car path must be traced, not assumed.

---

## 10. Revert to stable archive `RacingUnited(20260812-083256).zip`

**User observation after reverting:**
- Game runs like the familiar legacy system.
- New systems from later patches did not appear to produce visible differences.
- This strengthened suspicion that later tire code was either not active for the driven car, not reaching the render path, or was layered on top of a legacy authority path that still dominated.

**Decision:** Rebase visual tire deformation work directly onto this exact stable archive rather than assume TIRE21-era interfaces/state are present.

---

## 11. TIRE22R — local-space exact collider wrap rebased to stable archive

**Primary goal:** Ignore Pacejka/traction complexity and make one thing happen: **real scene collider intersects visible lower tire → visible tire vertices move around collider.**

**Major changes:**
- Transform nearby collider triangles on CPU into the current spinning/steering tire node's **local space**.
- Compare tire-local vertex directly against tire-local collider triangles in shader.
- Use a single dominant bounded correction per vertex instead of cumulative plane pushes.
- Restrict arbitrary collider visual wrapping to lower tire regions while protecting bead/rim-adjacent geometry.
- Use finite-triangle closest-point logic.
- Improve BVH candidate ranking using true nearest point on finite triangles.
- Request a larger candidate set before selecting the bounded GPU set, with intent to avoid dense-road starvation of curb/wall geometry.

**Build result:** User could not reach compile/test because static validation failed with seven checks referencing later TIRE17C8/TIRE20/TIRE21 assumptions.

**Root cause of this build failure:** The reverted baseline archive's validator scripts do not contain most of those later checks. The user's working directory therefore almost certainly retained **stale later validator files** when the old archive was overlaid without deleting the directory first.

**This is not a live deformation result.** TIRE22R itself has not yet had a fair live test on this clean baseline.

---

## 12. TIRE22R1 — clean validator rebase + handoff folder

**Goal:** Give TIRE22R a fair build on the actual reverted baseline and permanently document all attempts.

**Changes:**
- Ship the complete `Tools/Validation` set from the reverted stable archive, replacing stale later-branch validator scripts.
- Preserve the architecture validator but increase only the historical `CollisionSystem.cpp` line-count guard because TIRE22R adds finite-triangle ranking/candidate plumbing and pushed the file slightly above the old CLEAN13 threshold.
- Keep all TIRE22R local-space visual-wrap code.
- Add this `TireDeformation_IMPORTANT` handoff folder.

**Next live requirement:** If TIRE22R1 builds, test visible tire deformation immediately before any further tire-physics development.


---

## 13. TIRE22R2 — wheel-telemetry validator preallocation hotfix

**Build symptom:** After TIRE22R1 replaced the stale later-branch validators with the reverted baseline set, validation reached a single failure:

`named wheel telemetry returns tire, contact-diagnostic and upright state in one table`

**Root cause / lesson:** The baseline validator treated the Lua named-wheel telemetry table's `lua_createtable` preallocation count (`205`) as if it were a public ABI requirement. That integer is only a Lua hash-table capacity hint and can legitimately change when telemetry fields are added or removed. A working tree contaminated by newer telemetry can therefore fail validation even when the required named fields are present.

**Fix:** Validate the existence of `luaVehicleGetWheelTelemetry`, a numeric `lua_createtable(state, 0, N)` reservation, and the required tire/contact/upright fields, but do **not** require the literal number `205`.

**Priority remains unchanged:** This hotfix changes no tire collision, deformation, shader, Pacejka, suspension, or vehicle physics behavior. Its only purpose is to let TIRE22R reach the compiler/live test.

---

## 14. TIRE22R2 live result — still no contact-driven deformation; curb causes rodeo bounce

**Live result:** TIRE22R2 finally built and launched on the reverted/stable baseline. The user's screenshot showed the visible tires still essentially round/legacy-looking, with only the previously known small lateral deformation. Real scene contact still did not visibly wrap/squash the rubber.

**New symptom:** Driving over the sidewalk/curb can make the car buck/bounce violently, described as behaving "like a bull in a rodeo."

**Interpretation:**
- The tire visual shader is probably active at least for the old lateral/shear effect, but the exact collider-driven branch is still not proven to be affecting the player's visible rubber.
- The curb bounce is a separate physics/support discontinuity symptom. TIRE22R's exact wrap changes are presentation-side; the correctness-first 256-candidate query was also running inside the wheel substep and was unnecessarily expensive for a visual experiment, so the next diagnostic reverts that candidate gather to the stable baseline while isolating the render path.
- Do **not** respond by adding more Pacejka, carcass-force, wall-traction or suspension logic before proving which visible shader path and collider bridge are actually live.

**Decision:** Move to a deliberately absurd, contact-independent visual proof. If a hard-coded 45 mm lower-half squash cannot be seen, the edited shader/range is not the path drawing the tire. If that squash is visible but a second collider-count-dependent notch is absent, the native collider-triangle bridge is not reaching the shader. If both are visible, the path and bridge are alive and the remaining bug is in triangle-space/penetration math.

---

## 15. TIRE23 — visible tire path + collider bridge proof

**Purpose:** Stop guessing. This is intentionally *not* a final tire model.

**Temporary visual markers:**
1. Every recognized live tire draw receives an unconditional ~45 mm lower-half radial squash. It requires no grounded state, no tire force, no contact manifold and no collider geometry.
2. If `uTireVisualColliderTriangleCount > 0` reaches that same draw, an additional ~28 mm asymmetric front/lower tread notch is applied.
3. The shadow tire path receives the same temporary geometry markers so the shadow does not hide/misrepresent the proof.

**One-shot console diagnostics:**
- `TIRE23 VIS15 PROOF: live tire shader draw node=...`
- `TIRE23 VIS15 PROOF: collider bridge reached node=... triangles=N`

**Physics isolation:** The TIRE22 correctness-first 256-candidate/priority gather in `06_ContactKinematicsAndPatchGeometry.inl` is reverted to the stable baseline 64-nearest gather for this proof. TIRE23 intentionally does not add or change tire forces, suspension support, Pacejka, wall traction or collision impulses.

**How to interpret one screenshot:**
- **No huge lower-half squash:** wrong/inactive render path or tire range; stop all collider math work.
- **Huge squash but no asymmetric notch:** tire shader is live, collider bridge/count is not reaching it.
- **Huge squash + asymmetric notch:** shader and bridge are live; focus next on actual triangle transformation, finite-triangle penetration and deformation response.

**Cleanup requirement:** Remove the forced proof deformation immediately after the live path is identified. Never ship it as tire physics.

## 16. TIRE23 live result — shader path finally proven

**Observed live result (2026-08-12):** The user's screenshot showed the intentionally grotesque lower-half squash on all four visible tires.  Therefore `EntityMeshShaders.hpp` / the `applyTireVisualDeformation` path is conclusively the live render path for the player's tires.  Stop suspecting that all earlier shader edits were hitting an inactive tire renderer.

**Also observed:** the vehicle still bucks violently / "rodeo bull" bounces when crossing the sidewalk.  TIRE23 did not add tire forces, so that symptom belongs to the underlying physics/support/contact path and must be treated separately from visual deformation.

**What this disproved:**
- "We are editing the wrong visible tire shader" is false.
- The tire mesh is not intrinsically rigid; GPU vertex deformation visibly works.

**What remains unresolved:**
- Real scene-contact geometry has not yet produced convincing, smooth, local tire wrap.
- The old exact-collider method behaved like hard per-vertex clipping/projection and did not create a carcass-like response.

## 17. TIRE24 — continuous contact-kernel visual tire wrap

**Priority:** visual 3D deformation only.  No Pacejka or suspension changes.

**Hypothesis:** TIRE22R's core collision rule is too discrete.  It waits for each individual render vertex to cross a finite triangle plane.  A curb/rock edge can intersect the continuous tire volume between render vertices, and even when it hits, independent vertex projection looks like clipping rather than a deformable carcass.

**Implementation:**
- Remove all TIRE23 unconditional proof deformation.
- Keep the proven live tire shader path.
- For each nearby real collider triangle already transformed to tire-local space, compute the closest finite-triangle point to the tire centre.
- Treat the lower tire as a continuous rounded shell; if the triangle's closest point lies inside that shell, generate bounded compression independent of render-vertex coincidence.
- Map that contact into a smooth kernel around the circumference and across tire width.
- Move affected rubber radially inward toward the rim; anchor the bead/rim region.
- Add a small adjacent sidewall bulge as crude volume preservation.
- Take the strongest local indentation instead of summing many triangle corrections, preserving the anti-spike lesson from C8/C9.
- Mirror the same rule into the shadow shader.

**Live acceptance:** longitudinal curb edge beneath tread centre should create a smooth local centre indentation; front/lower wall contact should squash that region; no unconditional deformation away from contacts.

**Separate known issue:** sidewalk rodeo/bounce is physics/support behavior and is intentionally not changed in this visual-only attempt.


## 16. TIRE24 / VIS16 - Contact-kernel tire wrap — LIVE FAILURE

**Live result:** The visible tire still sank into the road and did not visibly conform to the sidewalk/kerb. Positioning the car on the sidewalk was difficult because the legacy support/suspension path could launch the car in a violent rodeo-like bounce.

**What this disproved:** Merely changing the live tire shader from per-vertex plane projection to a center-distance contact kernel is insufficient. TIRE23 had proven the shader itself is live, but TIRE24 did not prove that the real collider triangle bridge was reliably fresh or populated at the exact moment the wheel crossed a kerb.

**Important code-path discovery after the failure:** `tireVisualColliderTriangles` was refreshed inside wheel-substep phase 06, but phases 01/05 contain legitimate early returns for missing support, beyond-reach, airborne and related states. Therefore the render-facing collider cache can be stale or empty specifically during violent kerb transitions. The live render binding was blindly copying that cache. This is a major architectural weakness for visual deformation.

## 17. TIRE25 / VIS17 - Direct render-time collider bridge + finite-triangle soft-body wrap

**Priority:** visual 3D tire deformation only.

**Changes:**
1. `Entity.SetMeshNodeTireColliderTrianglesFromWheel` no longer relies solely on the WheelState collider cache. On each visible wheel update it directly queries the authoritative static-scene triangle BVH around the wheel and reduces a 384-candidate neighborhood to the 64 triangles permitted by the OpenGL 3.3 shader.
2. 48 of those 64 slots are priority slots for geometry that is inside/near the free tire shell or has a non-horizontal normal. Dense road tessellation should therefore not starve kerb/wall/rock faces.
3. The old cached WheelState neighborhood remains only as fallback when the direct query returns nothing.
4. The shader lower-half test is now gravity-relative rather than based on the current support normal. A wall/kerb normal can therefore be horizontal/diagonal without rotating the definition of the tire's lower hemisphere.
5. Deformation now uses the real finite triangle normal. Stage A creates smooth carcass compression around the triangle contact. Stage B performs a bounded finite-triangle non-penetration projection on any rubber vertex that still crosses real geometry. Only the strongest correction is applied, preventing cumulative 64-plane explosions.
6. Bead/rim-adjacent rubber remains anchored. Maximum deformation is bounded by carcass radial thickness and 110 mm in physical scale.

**Why this attempt exists:** TIRE23 proved the live shader. TIRE24 failing strongly suggests the next thing to prove/fix is the collider data source itself, not another artistic deformation coefficient.

---

## 18. TIRE25 / VIS17 live result — direct BVH triangle bridge still failed

**Live result (2026-08-12):** No convincing collider-driven deformation. The tire still sank into the road/sidewalk and the car continued to buck violently when the central support crossed the sidewalk step.

**What this disproved:** Merely refreshing 64 exact scene triangles directly from the static BVH in the visible-wheel binding is not enough. The proven live shader still did not receive/use collider geometry in a way that corresponded to the tire the player actually sees.

**Critical architectural mismatch found after the live failure:** `EmbeddedWheelBinding.lua` explicitly does **not** render the wheel from `WheelState.worldCenter`; it reconstructs the wheel from the current/interpolated chassis transform plus authored bind pose and suspension travel. TIRE25's BVH neighborhood was nevertheless centred on `WheelState.worldCenter`. That state is generated before rigid-body integration and was already documented in the code as capable of being tens of centimetres away from the rendered wheel at speed. Even perfect local-space triangle transforms cannot rescue a neighborhood gathered around the wrong centre.

**Decision:** Stop shipping absolute collider triangles to GLSL. The next attempt sends only a compact contact/deformation field produced by CollisionSystem queries around a centre reconstructed using the same current chassis + suspension policy as the visible wheel. This removes absolute contact-position/triangle-space synchronization from the presentation boundary entirely.

---

## 19. TIRE26 / VIS18 — 9x7 lower-half CollisionSystem probe lattice

**Priority:** visible 3D tire deformation. Pacejka/thermals/wear remain untouched.

**Design:** This implements the user's requested lower-half contact topology directly: 9 stations around the lower circumference (front equator -> straight bottom -> rear equator) x 7 bands across tire width (sidewall -> centre tread -> opposite sidewall), 63 possible contact/deformation loci.

**Major changes:**
1. Add read-only `VehicleSystem::wheelDescription()` so the render-facing probe builder can use the authored physical wheel mount, radius, tire width and maximum tire deflection without reaching into VehicleSystem internals.
2. Reconstruct the probe centre from the **current Entity chassis world transform** + authored physical mount + current suspension length. This deliberately follows the same current-chassis policy used by `EmbeddedWheelBinding.lua`, instead of using stale `WheelState.worldCenter` as the visual query origin.
3. At every 9x7 lattice locus, construct a rounded-tire surface normal. Centre tread probes point mainly radially; shoulder probes become diagonal; outer bands become sidewall-facing. This allows the same lattice to detect flat road, curb/chamfer, front/rear obstacles and lower-sidewall contacts.
4. Use a 6 mm **sphere cast** from inside the nominal tire shell outward through each locus. Sphere casting is chosen over a zero-radius ray so thin curb edges/seams can still be discovered between exact ray lines. Compression is the amount by which the real collider lies inside the nominal tire surface.
5. Store only 63 scalar compression values in the Entity mesh-node override. No world triangle/contact point crosses into the shader, eliminating the repeated world/local/camera/spin synchronization failures of TIRE17-TIRE25.
6. The proven live tire vertex shader bilinearly interpolates those 63 compression values in a **world-anchored wheel basis**. The contact field therefore stays at road/curb position while tire material spins through it.
7. GPU deformation moves tread/shoulder/sidewall inward along a rounded tire-surface normal, with the bead strongly anchored and a small bounded sidewall bulge.
8. When the 9x7 grid is valid it becomes the sole loaded/contact deformation authority; the old contact-plane flattening and exact triangle projection do not double-apply deformation.
9. The legacy exact-triangle GLSL arrays are removed from the shader to avoid wasting vertex-uniform budget on a failed path. C++ triangle plumbing remains inert/disabled for compatibility until cleanup.
10. Console proof lines report first live/deep probe activation per wheel (`TIRE26 VIS18 probe lattice LIVE...`, `DEEP CONTACT...`).

**Performance note:** 63 broadphase-backed sphere casts per visible tire is correctness-first and intentionally much cheaper/cleaner than testing every render vertex or sending 64 triangles to every vertex. For 150-car production, this lattice should later be adaptively reduced/slept for distant/simple-road tires; do not optimize it before the live deformation is correct.

**Live acceptance:**
- flat loaded tire visibly forms a footprint instead of sinking round through road;
- longitudinal sidewalk edge under centre tread produces a centre groove/indentation;
- lower-front curb contact deforms the front/lower tread/shoulder;
- bead/rim stays constrained;
- no polygon spikes.

**Known separate issue:** the rodeo bounce remains caused by the legacy authoritative suspension support jumping across a raised sidewalk/kerb. TIRE26 intentionally isolates the visual deformation rewrite first; once visual contact is proven, the same lower-shell contacts should replace/augment the single centre support for curb-climb physics.


---

## 20. TIRE26 live build attempt — failed before tests due stale TIRE21 wheel-substep leftovers

**Observed build failure (2026-08-12):** Static validation passed, then `HeritagePhysicsTests` failed while compiling `VehicleWheelSimulation.cpp`. The first errors were in `00_PrepareWheelAndSupportQuery.inl` and `02_SteeringBrakingAndFreeWheel.inl`: those files referenced TIRE18/TIRE21-era members such as `WheelState::tireCarcass3DContactValid`, `WheelRecord::cachedTireColliderTriangles`, `cachedTirePrimitiveSurfaces`, `cachedTireCarcass3DPotential`, and `carcassContactActiveLastSubstep`, while the TIRE26 `VehicleSystem.hpp` was based on the reverted stable branch and intentionally did not contain those fields.

**Root cause:** overlay ZIP extraction had left later experimental native WheelSubstep files in the user's working directory even after reverting to the older stable archive. TIRE22R1 cleaned stale validators, but it did not delete/overwrite all stale native experimental files. TIRE26 then overwrote `VehicleSystem.hpp`, exposing the cross-branch mismatch.

**Lesson:** do not "fix" this by restoring the entire TIRE21 physical carcass state into TIRE26. That would silently drag the abandoned wall-traction/carcass experiment back into the current visual-deformation investigation. The cleaner correction is to overwrite the five WheelSubstep files that TIRE21 changed with the reverted stable versions: phases 00, 02, 05, 06, and 10. TIRE26's visual 9x7 probe lattice does not depend on the TIRE21 physical carcass solver.

## 21. TIRE26A / VIS19 — clean WheelSubstep rebase + INSERT on-tire probe debug

**Priority:** get TIRE26 to compile on the user's contaminated overlay tree, then make live diagnostics visible directly on the tire instead of relying on wireframe/console alone.

**Build correction:** overwrite the TIRE21-modified WheelSubstep files with the clean reverted-baseline versions:
- `00_PrepareWheelAndSupportQuery.inl`
- `02_SteeringBrakingAndFreeWheel.inl`
- `05_SuspensionAndContactResolution.inl`
- `06_ContactKinematicsAndPatchGeometry.inl`
- `10_ApplyForcesAndIntegrateWheel.inl`

This removes stale TIRE21 physical-carcass references while leaving the TIRE26 render-facing 9x7 CollisionSystem probe lattice intact. Local C++20 syntax compilation of `VehicleWheelSimulation.cpp` succeeds with this clean combination.

**Developer visual diagnostic:** `INSERT` toggles an on-tire probe overlay. It is deliberately a direct hotkey, not a buried UI option.

When enabled on a recognized tire node:
- a faint cyan grid shows the 9 lower-circumference stations x 7 width bands directly on the rubber;
- every lattice point is shown as a visible marker;
- inactive/no-compression probes are grey;
- shallow active probes are green;
- medium compression trends yellow;
- deep compression trends red.

The overlay is applied after lighting/color grading so it remains readable at night and in shadow. It is presentation/debug only and does not modify physics or the probe compression values.

**How to interpret the next live screenshot:**
- Tire visibly intersects road/sidewalk and all nearby markers remain grey: probe placement/query is wrong or the CollisionSystem returns the wrong surface.
- Correct markers turn green/yellow/red at the visible sidewalk but tire shape does not follow them: the GPU interpolation/deformation field is wrong.
- Markers and deformation both react correctly, but the vehicle still bucks: visual deformation is working; the separate legacy suspension/support step response must be replaced afterward.

## 22. TIRE26A build attempt — engine target still contained stale TIRE21 Lua telemetry

**Observed build result (2026-08-12):** `HeritagePhysicsTests` got past the WheelSubstep contamination after TIRE26A, but the main `HeritageEngine.vcxproj` later failed in `Core/Modules/LuaBindings/Vehicle/LuaVehicleTelemetryBindings.cpp`. The stale file still referenced removed TIRE18/TIRE21 `WheelState` members: `tireCarcass3DContactValid`, `tireCarcass3DContactCount`, `tireCarcass3DTotalNormalForceN`, and `tireCarcass3DMaximumCompressionM`.

**Why the build appeared to continue after red errors:** MSBuild can continue compiling independent translation units/projects after one `.cpp` fails. These are real fatal compiler errors; the final engine executable cannot link successfully from that build.

**Root cause:** Another overlay-extraction leftover. TIRE26A explicitly replaced the contaminated WheelSubstep files but did not overwrite `LuaVehicleTelemetryBindings.cpp`, so the user's on-disk tree still mixed TIRE21 telemetry code with the reverted/TIRE26 `WheelState` definition.

**Lesson:** ZIP overlays cannot be assumed to restore a branch. Any file modified by TIRE18-TIRE21 but not explicitly overwritten by the later stable/TIRE26 patch may remain contaminated even if validation passes. Future tire-deformation patches should either be applied to a clean extracted baseline directory or explicitly restore all known experimental leftovers that are not part of the current design.

## 23. TIRE26B — cross-branch native cleanup, preserving VIS18/VIS19 probe work

**Priority:** make the exact TIRE26 9x7 visual probe experiment build without resurrecting abandoned TIRE18-TIRE21 carcass physics.

**Changes:**
- Restore the reverted-stable `LuaVehicleTelemetryBindings.cpp`, removing the four stale TIRE21-only `WheelState` fields that caused the main engine compile failure.
- Re-overwrite all five WheelSubstep files with the reverted stable versions.
- Proactively restore other TIRE18-TIRE21-modified native/test/project files that TIRE26 does not intentionally depend on: collision query/header leftovers, TireModel source/header, physics regression sources/project files, and the Racing United tire LivePanel.
- Preserve all intentional TIRE26/TIRE26A work: 9x7 lower-half CollisionSystem probe lattice, render-space compression interpolation, on-tire debug markers, and the direct `INSERT` hotkey.
- Verify the intended TIRE26B source tree contains none of the abandoned member names (`tireCarcass3DContactValid`, `cachedTireColliderTriangles`, `cachedTireCarcass3DPotential`, etc.).

**Live diagnostic remains:** press `INSERT` to toggle the 9x7 tire probe overlay. The next useful screenshots are one tire standing on flat pavement and the same tire intersecting the sidewalk/kerb.

## 24. TIRE26B live proof — visible deformation and probe bridge confirmed on sidewalk

**Observed live result (2026-08-12):** The `INSERT` tire overlay worked on all four visible player tires. With a wheel positioned against the sidewalk/kerb, the tire visibly deformed and the coloured probe field followed the actual visible tire. This is the first clean live proof that the current `CollisionSystem -> render-space probe lattice -> Entity override -> active tire shader` chain is functioning end-to-end on real scene geometry.

**What the screenshot showed:** deformation exists, but the old 9x7 lattice is visibly coarse. Individual probe regions own large blobs/scallops of rubber, particularly around the bottom footprint. The user's requested next direction is therefore not another collider rewrite: preserve this proven path and increase structural/contact sampling, with especially high density in the lowermost loaded region below the user's green-line chord, both around the tire circumference and across its width.

**Do not regress:** do not reintroduce TIRE17-TIRE25 world-triangle GPU projection or the abandoned TIRE18-TIRE21 physical carcass fields while pursuing this visual refinement. The TIRE26B probe bridge is the first path conclusively seen working against the sidewalk.

## 25. TIRE27 / VIS20 — bottom-biased 21x13 dense probe lattice

**Priority:** turn the proven-but-coarse TIRE26B deformation into a substantially finer 3D contact/deformation field without changing the now-proven bridge architecture.

**Implementation:**
1. Increase the render-facing structural/contact lattice from **9x7 = 63** samples to **21x13 = 273** samples per tire.
2. Circumference stations are intentionally **non-uniform**. Full lower-half coverage remains from front equator (0 deg) through bottom (90 deg) to rear equator (180 deg), but 13 of the 21 stations lie between **65 and 115 degrees**. This is the user's requested high-resolution region below the green line.
3. Width resolution grows from 7 to 13 bands, spanning sidewall -> shoulder -> tread centre -> opposite shoulder/sidewall. Width coordinates are non-uniform and explicitly shared between CPU probe generation and GLSL reconstruction.
4. GLSL no longer assumes uniform station/band spacing. It maps each tire vertex's real lower-half angle and width into the non-uniform lattice before bilinear interpolation.
5. The `INSERT` overlay uses exactly the same non-uniform mapping, so the dense lower region is visible directly on the tire and each marker corresponds to the actual new probe topology.
6. Keep the 6 mm CollisionSystem sphere-cast probes and render-policy wheel centre proven by TIRE26B. This attempt deliberately changes sampling density rather than reopening coordinate-space/contact-bridge questions already answered live.
7. Improve the ordinary flat-road fallback: instead of forcing one single bottom row to full `tireDeflection`, distribute the authoritative loaded deflection smoothly across the dense bottom stations according to contact-patch length and taper it across tread/shoulder width. Real obstacle probe compression is combined with `max()` and remains authoritative wherever it is larger.
8. The old exact world-triangle visual bridge remains disabled. TIRE27 has one irregular visual contact authority: this bottom-biased probe lattice.

**Performance note:** 273 sphere casts per visibly updated tire is correctness-first and is not the eventual 150-car policy. Once shape fidelity is accepted, move this lattice behind distance/importance LOD and/or derive multiple deformation samples from shared broadphase/narrowphase neighborhoods rather than blindly querying all 273 positions on all 600 tires.

**Live acceptance test:** with `INSERT` enabled, the bottom loaded region must visibly contain many more, much smaller probe cells than TIRE26B. A sidewalk edge underneath the tread should produce a narrower, more localized deformation instead of the old large scalloped blobs. Keep the same sidewalk test so the only major variable is lattice resolution.


---

## 26. TIRE28C / VIS21 — code-only semantic wheel routing correction (DO NOT EDIT THE GLB)

**Triggering live discovery (2026-08-12):** with the TIRE27 21x13 probe overlay active, the user deliberately placed the **left-side physical tires** on the sidewalk and observed the strong contact/deformation pattern on the **right-side visible tires**. The creator confirmed the Blender/GLB wheel names and physical sides are correct. Therefore the asset is authoritative and must not be renamed/swapped to accommodate a runtime routing bug.

**Correction strategy:** keep the existing, now-proven `CollisionSystem -> dense probe lattice -> Entity node override -> live tire shader` path. Fix only which native wheel owns each semantic visual node.

**Concrete geometry evidence from the uploaded/current GLB:** the authored transformed wheel pivots place `WH_FL_Pivot` on the **positive-X** side and `WH_FR_Pivot` on the **negative-X** side (same pattern rear). That is valid for the creator's vehicle convention: the nose is authored toward Blender `-Y`, so driver-left is `+X`. The current prototype/native positional assumptions had effectively treated the opposite X side as left for presentation routing. This is a code-side convention mismatch, not a bad asset.

`Entity.SetMeshNodeTireColliderTrianglesFromWheel(chassisEntity, tireNodeName, nativeVehicle, index)` previously trusted the caller's positional wheel index. TIRE28C instead parses the semantic GLB node (`WH_FL`, `WH_FR`, `WH_RL`, `WH_RR`) and resolves that semantic corner against the native `WheelDescription.localMount` positions. It selects the matching lateral/longitudinal mount and then uses that resolved native wheel for both `WheelState` and `WheelDescription` when constructing the 21x13 probes.

**Why this is the correct layer to fix:** visual asset naming/placement is creator-authored truth. Native vector/contact-unit ordering is an implementation detail and may change. A presentation binding named `WH_FL_Tire` must never silently receive another corner's contact field merely because a loader/vector order differs.


**Complete visual-state routing fix:** `EmbeddedWheelBinding.lua` now resolves each visual corner (`FL/FR/RL/RR`) by the physical mount geometry before selecting `vehicleWheelTelemetry` and the prototype wheel record. This means suspension/upright/spin/tire state and the dense contact probes all come from the wheel physically occupying that semantic corner. The mapping is not a hard-coded `1<->2`/`3<->4` swap; it derives the lateral and longitudinal extrema from the current wheel definitions.

**Live diagnostic:** on first use per tire node the console prints:

`TIRE28C VIS21 semantic routing node=WH_FL_Tire requested=... resolved=... mount=(x,y,z)`

The same lines exist for FR/RL/RR. They make the routing explicit and auditable.

**Acceptance test:** repeat the exact TIRE27 screenshot setup: put the physical left tires on the sidewalk with `INSERT` debug enabled. Strong red/orange probe compression and visible deformation must remain on the **left visible tires**. The right-side tires must not inherit the left-side sidewalk field.

**Do not repeat:** do not rename/swap `WH_FL`/`WH_FR` or `WH_RL`/`WH_RR` in the GLB. TIRE28 asset-side swapping was a mistaken diagnosis and is superseded by this code-only correction.

## Attempt 29 — TIRE29 / VIS22: non-inverting carcass constraint after rear-left foldover

### Live evidence
- TIRE28C corrected the left/right routing in code without modifying the authored GLB.
- In the next live sidewalk test, the front-left tire finally deformed at the correct visible wheel.
- The rear-left tire, however, produced a long thin tongue/fin from one lower sidewall region instead of a plausible compressed carcass.
- This is **not** another proof that the wheel routing is reversed: the front-left mapping is now correct and the rear-left probe/debug activity is on the intended visible tire.

### Root cause found in the active TIRE27 deformation math
- `maximumTireDeflectionM` for the prototype is 0.08 m.
- The tire half-width is only about 0.102 m and its radial rubber depth from rim/bead to tread is about 0.082 m.
- The probe bridge previously allowed the same scalar compression limit in every direction.
- A sidewall-facing probe could therefore request close to 80 mm of lateral displacement, enough to drive a sidewall strip through the protected interior/centre of the tire.
- The shader had no geometric non-inversion constraint, and it also added a synthetic local sidewall bulge. Together these can create the observed tongue/fin when one edge band is heavily loaded by the curb.

### TIRE29 changes
1. Keep the proven TIRE27 21x13 bottom-biased probe lattice and TIRE28C semantic routing.
2. Derive radial compression capacity from authored rim diameter / available sidewall height.
3. Derive lateral compression capacity from actual tire half-width.
4. Blend those capacities according to whether each probe is tread-facing or sidewall-facing before the compression grid is uploaded.
5. Add a second GPU geometric non-inversion guard per rendered vertex: rubber cannot be pushed through a protected radial bead core or through a protected lateral centre band.
6. Disable the old synthetic sidewall bulge until a real neighboring-node pressure/volume constraint exists.
7. Do not modify the GLB/model.

### Acceptance
- Repeat the same left-side sidewalk test with INSERT debug enabled.
- FL must remain correctly routed.
- RL may compress strongly at its sidewall/shoulder, but it must no longer form a thin tongue that crosses/folds through the tire.
- If RL still shows an isolated deformation on the wrong circumferential/width region, next inspect the resolved RL wheel basis and individual station/band values rather than changing routing again.


## Attempt 30 — TIRE30 / VIS23 Atomic Probe-Basis Ownership

**Observed before this attempt:** TIRE28C fixed whole-wheel left/right routing, but the
rear-left tire revealed a second, narrower bug: a contact on one lateral side of RL was
shown on the opposite lateral side of that same tire. This is distinct from the later
carcass-folding/non-inversion problem.

**Hypothesis:** the 21x13 grid and the shader's lateral basis had split ownership. C++
semantic routing generated the grid using one resolved `WheelState.worldWheelRight`,
while the earlier Lua tire-deformation call independently populated
`MeshNodeOverride::tireWheelRightWorld`. If those paths disagree on native ownership or
sign, the correct compression array is mirrored during shader lookup.

**Change:** `setMeshNodeTireProbeGrid` now publishes the exact resolved wheel forward/right
basis together with the compression field. The probe grid is therefore an atomic state:
values and coordinate basis can no longer come from different wheel-routing paths. No
asset/model changes and no hard-coded RL-only sign flip. Added one-shot basis and width
half diagnostics.

**Priority:** verify RL side correctness before tuning folding, volume preservation, or
carcass aesthetics.

## Attempt 31 — TIRE31 / VIS24 Shape-Preserving Coupled Carcass

### Live evidence leading to this attempt
- TIRE30 fixed the remaining RL lateral-side inversion: the deformation now appears on the same physical side of the rear-left tire that actually touches the sidewalk.
- With routing finally correct, the remaining visual failure is the carcass shape itself. The loaded lower section can pinch into a narrow concave U / fold instead of forming the broad smooth compressed section the user sketched in green.
- This is now a deformation-shape problem, not a wheel-routing problem and not an asset/model problem.

### Root cause in the active deformation path
1. The 21x13 probe values were treated as almost independent dents. A strongly compressed band could move much farther than its neighbours, creating a local hinge/pinch.
2. The shader used width position to blend the deformation direction toward the tire centre. At the bottom of the tire this is wrong: a road/kerb load should primarily shorten the tire radially/upward. Pulling the lower sidewalls laterally inward narrows the cross-section and encourages foldover.
3. TIRE29 disabled the old fake bulge to stop catastrophic inversion, but nothing then represented the basic pressure/carcass response where centre-tread compression makes the lower sidewalls move slightly outward.

### TIRE31 changes
- Preserve TIRE30 atomic wheel/basis routing and the dense 21x13 lower-half lattice.
- Add one-pass anisotropic carcass coupling on the CPU. Real contact depth is never averaged down; adjacent circumference/width nodes inherit only a decayed fraction of a deeper neighbour. This turns isolated probe dents into a connected tread/belt deformation field.
- Make deformation direction depend on circumference as well as width. Near straight-down, radial compression has authority even on shoulder/sidewall bands. Strong lateral inward motion is reserved for contacts approaching the front/rear equators where a wall/rock can genuinely push the side of the tire.
- Strengthen the protected bead/rim and lateral-centre non-inversion core.
- Add a restrained pressure-like lower-sidewall outward response driven by centre-tread compression. It is suppressed for equator-style side contacts so a wall can still push a sidewall inward.
- No GLB/model changes. No wheel-routing changes. INSERT debug remains available.

### Acceptance
- On flat/sidewalk support, the lower cross-section should resemble the user's broad green U/flat-footprint sketch rather than the narrow red pinch.
- The tread can still form a local groove over a longitudinal kerb edge, but neighbouring bands must bend smoothly into it.
- Lower sidewalls must remain outside the bead/rim and may bulge slightly outward under centre load; they must not be sucked laterally through the wheel centre.
- Front/rear equator obstacle contacts must retain the ability to deform inward laterally.


## Attempt 32 — TIRE32 / VIS25 Physical Carcass Compliance

### Live evidence
- TIRE31 obstacle routing/deformation works on all four wheels.
- Under steering/lateral force, the direction of carcass shear is correct but the magnitude looks like a severely underinflated tire even at about 2.3 bar.
- The visual shift is too local and too large compared with the user's tire-ring/sidewall deformation reference.

### Root cause
The shader was estimating lateral visual displacement from `Fy/Fz * radialDeflection` and could reach about 22 mm. Radial deflection is not lateral carcass compliance. The active prototype `.tir` already identifies a reduced-order structural belt mode (`LATERAL_STIFFNESS = 650000 N/m`, `FREQ_LAT = 55 Hz`, damping), and physics already integrates the corresponding ring displacement. Presentation was ignoring that better physical state.

### Changes
1. Render longitudinal/lateral carcass shear from `tireRingLongitudinalOffset` / `tireRingLateralOffset` — the actual second-order structural state.
2. Feed live tire inflation pressure, reference pressure, and thermal stiffness into the rigid-ring solver.
3. Scale structural stiffness with a reduced-order pneumatic pressure-tension law and scale modal frequency consistently with `sqrt(k)`.
4. Demote the old force-ratio presentation shear to a small legacy fallback only.
5. Replace local lower-patch translation with a broad carcass shear mode: bead fixed, sidewall bends progressively, tread/belt approaches the physical ring offset over a broad lower arc.
6. Preserve TIRE30/TIRE31 wheel-side routing and the 21x13 obstacle lattice unchanged.

### Expected current prototype magnitude
At 650000 N/m, 3300 N of lateral force gives about 5.1 mm quasi-static belt displacement at reference pressure, before transient dynamics. This is physically much closer to the intended normal-pressure road-tire response than the former ~22 mm presentation allowance.

### Accuracy boundary
The current PrototypeRoad `.tir` explicitly states that its structural values are synthetic development seeds, not measured historical Pirelli data. TIRE32 now obeys those structural parameters instead of an arbitrary visual heuristic. Tire-specific absolute accuracy requires identified rig data.


## Attempt 33 — TIRE32A / Validator Supersession Hotfix

### Build failure after TIRE32
Static repository validation stopped the build before compilation on two historical guards:
- `TIRE17C2 three-axis carcass/belt deformation...`
- `TIRE17C3 force-resolved carcass shear...`

### Why those guards became stale
TIRE32 intentionally superseded the old presentation-only shear implementation. The legacy validator required shader implementation tokens such as `beltAnchorMask`, `longitudinalLoadRatio`, and `lateralLoadRatio`. Those tokens belonged to the older force-ratio visual heuristic. TIRE32 instead consumes the physics-integrated `tireRingLongitudinalOffset` / `tireRingLateralOffset`, reconstructs them through `physicalLongM` / `physicalLatM`, applies a bead-anchored `carcassShearMask`, and preserves the loaded contact plane. Therefore the old literal-token check incorrectly rejected the newer implementation even though the intended architectural invariant is still present and improved.

### TIRE32A change
- Do **not** alter tire physics, pressure scaling, 21x13 obstacle deformation, wheel routing, or shader behavior.
- Update `30_VehicleAndContentArchitecture.ps1` so each historical TIRE17 guard accepts either the legacy implementation or the TIRE32 physical structural-ring implementation.
- The TIRE32 path is explicitly required to show authoritative wheel basis, bead anchoring, structural ring offsets from telemetry, physical ring displacement in the shader, and preservation of the loaded contact plane.
- Do not make exact historical shader variable names into permanent API contracts when a newer implementation cleanly supersedes them.

### Priority after this hotfix
Resume the TIRE32 live test. Judge the lateral carcass deformation magnitude, smoothness, pressure response, and stationary jitter. This hotfix must not change those behaviors.

## Attempt 34 — TIRE33 / VIS26 Dense-Bottom Whole-Carcass Relaxation

### Live evidence leading to this attempt
- TIRE32 made the magnitude/sign of lateral carcass shear much more plausible at normal road pressure, but the user observed that the visible deformation still appeared to grab too small a patch of rubber.
- The requested visual/structural rule is explicit: **everything beneath the green-line chord in the dense lower tire region must participate**, with interpolation and smooth deformation across the whole bottom lattice rather than a few visibly pulled vertices.
- Stationary contact deformation also still exhibited small visible jitter.

### Changes
1. Preserve the proven TIRE27 21x13 CollisionSystem probe topology, TIRE28C/TIRE30 wheel-side/basis routing, TIRE31 non-inverting contact direction, and TIRE32 physical rigid-ring magnitude/pressure model.
2. Treat stations **4..16 (65..115 degrees)** — the deliberately dense region below the user's green-line chord — as one reduced-order carcass domain. All 13 width bands participate.
3. Retain true raw probe penetration as a hard lower bound. Real collision depth can never be averaged away.
4. First perform the local neighbour inheritance from TIRE31, then run three separable [1 4 6 4 1]/16 relaxation iterations circumferentially and laterally across the complete dense bottom domain. This propagates belt/sidewall tension far beyond one/two neighbours without replacing local curb/rock detail.
5. Feather the neighbouring station rows 3 and 17 at 50% so the dense bottom solve cannot create a hinge at its boundary.
6. Change GPU lattice reconstruction from plain bilinear weights to smooth Hermite interpolation (`smoothstep`-shaped cell fractions), removing visible piecewise-linear transitions between probe rows/bands while retaining the inexpensive four-sample lookup.
7. Broaden the TIRE32 physical rigid-ring shear eigenmode so the full lower carcass below the green-line chord participates. The bead remains anchored; sidewall/shoulder/tread progressively follow the physical ring displacement instead of only the outer belt region appearing to move.
8. Add presentation-only temporal carcass relaxation in `setMeshNodeTireProbeGrid`: fast compression response, slower release, and a 0.15 mm deadband while contact remains valid. This attacks the previously observed stationary deformation buzz without altering physics/contact queries.

### Intended result
- Under lateral load, the bottom of the tire should shear as one broad, smooth carcass region similar to the user's ring/sidewall reference instead of a few vertices being pulled sideways.
- Under road/sidewalk contact, a local high-compression region remains locally deepest, but the neighbouring lower carcass smoothly bends into it across the entire dense bottom lattice.
- The bead/rim stays constrained and the existing non-inversion limits remain active.
- Stationary tire deformation should settle rather than visibly flicker between nearly identical probe solutions.

### Do not regress
- Do not change GLB wheel naming/transforms.
- Do not undo TIRE28C/TIRE30 wheel and width-basis routing.
- Do not reintroduce independent GPU triangle collision.
- Do not replace the physical TIRE32 rigid-ring displacement with the old `Fy/Fz * radialDeflection` visual heuristic.


## Attempt 35 — TIRE33A / GLSL Carcass Hotfix

### Live failure
TIRE33 launched with catastrophic scene rendering corruption: stretched/black geometry and effectively broken world rendering. This was not a tire-shape tuning result and must not be interpreted as a failure of dense-bottom relaxation.

### Deterministic cause
TIRE33 renamed the shader's bead interpolation variable from `beadToBelt` to `beadToCarcass` while broadening the lower-carcass shear mode, but two live expressions (main tire vertex path and shadow tire vertex path) still referenced `beadToBelt`. The identifier no longer had any declaration in the embedded GLSL. That makes the generated shader source invalid. Runtime shader failure/fallback then corrupted the live mesh rendering path.

### Fix
- Replace the two stale `visualRingRadM * beadToBelt` expressions with `visualRingRadM * beadToCarcass`.
- Preserve all TIRE33 CPU 21x13 relaxation, temporal deadband/filtering, smooth lattice interpolation and broadened lower-carcass shear behavior.
- Add a repository validation guard forbidding the dangling shader expression so this exact identifier-rename regression cannot recur.

### Priority
First verify that normal world graphics are restored. Only after that evaluate TIRE33's intended smooth whole-bottom tire behavior and stationary jitter reduction.

## Attempt 36 — TIRE34 / VIS27 Whole-Bottom Carcass Shear

### Live evidence leading to this attempt
- TIRE33A restored rendering, but the user reported **no meaningful visible change** in lateral carcass deformation.
- The physical lateral displacement magnitude from TIRE32 is acceptable as the authority, but visually it still looks as though only a small handful of vertices at the very bottom are being dragged sideways.
- The explicit requirement is now unambiguous: **every rubber vertex in the connected lower carcass beneath the user's green-line boundary must participate smoothly**, not merely the tread/contact vertices. The bead/rim attachment is the only region that should remain strongly anchored.

### Diagnosis
TIRE33 broadened the circumference mask, but its `beadToCarcass = smoothstep(0.035, 0.70, radialFraction)` kept a large fraction of the lower sidewall only weakly coupled to the structural ring displacement. Therefore the visible silhouette could still read as a localized contact-patch pull even though the physical ring offset itself was correct. The TIRE33 shear domain also reused the contact-derived `down` basis, whereas the proven 21x13 lower-shell lattice is world-gravity anchored.

### Changes
1. Preserve TIRE32's physical rigid-ring displacement magnitude and TIRE27/TIRE31 obstacle deformation.
2. Define a **separate gravity-anchored carcass-down basis** from world down, matching the 21x13 lower-shell interpretation and independent of transient contact-normal tilt.
3. Broaden circumferential participation with `smoothstep(-0.08, 0.34, lowerHemisphere)`, coupling the entire lower half smoothly and feathering slightly above the geometric equator.
4. Move the bead-to-carcass transition dramatically inward: `smoothstep(0.015, 0.42, radialFraction)`. The immediate bead remains fixed, but lower sidewall, shoulder and tread now follow the physical ring much more coherently.
5. Add a lower-sidewall participation floor so mid-sidewall vertices cannot lag far behind the tread and create the visible pinch. This is still zero at the bead and fades before the outer tread.
6. Apply radial structural displacement through the same gravity-defined lower-carcass envelope so radial and lateral structural modes do not form separate hinges.
7. Mirror the exact deformation logic in the shadow vertex shader.

### Intended result
Under lateral force, the full lower tire cross-section should form a broad continuous shear from anchored bead -> bending sidewall -> displaced shoulder/tread. The visible deformation should resemble the user's tire/ring compliance reference: several millimetres of belt/contact-patch shift spread smoothly over many vertices, not a few vertices pinched at the bottom.

### Do not regress
- Do not increase the physical lateral offset merely to make the effect visible; magnitude remains physics-driven.
- Do not touch FL/FR/RL/RR routing or width-basis routing.
- Do not weaken TIRE31 non-inversion constraints.
- Do not replace the 21x13 obstacle contact lattice.

## Attempt 37 — TIRE35 / VIS28 Two-Scale Lower-Carcass Equilibrium

### Live evidence leading to this attempt
- The user's current screenshot still showed a small, deep patch of displaced tire
  vertices while the neighboring lower tread and sidewall stayed almost circular.
- TIRE34 broadened physical rigid-ring shear, but the reported failure was also
  present under ordinary vertical load. Shear-mask changes could not repair that
  separate radial/contact deformation path.

### Deterministic cause
The live shader set `deflection` to zero whenever
`uTireVisualProbeGridValid` was true. The 21x13 grid is normally valid on a loaded
tire, so the analytic flat footprint, lower-carcass rise and sidewall bulge were
all disabled in normal driving. Only probe-local compression remained visible.
The CPU also blurred ordinary equilibrium footprint load together with irregular
obstacle penetration, making that local path try to represent two different
structural scales at once.

### Changes
1. Keep the authoritative native radial-deflection/contact-patch mode active while
   the detailed probe grid is live.
2. Build an explicit equilibrium compression field from native deflection, tire
   radius and finite patch length/width.
3. Subtract that equilibrium from direct CollisionSystem samples before the TIRE33
   carcass relaxation, so only kerb/rock/broken-road residual is spread locally.
4. Recombine equilibrium plus relaxed residual with per-region geometric capacity.
   This preserves the deeper of baseline or direct collision and retains TIRE31
   non-inversion limits.
5. In both visible and shadow shaders, subtract the matching equilibrium field from
   the probe value. Ordinary vehicle weight therefore cannot be applied twice.
6. Replace the narrow 26-degree lower-carcass influence with a continuous analytic
   envelope that starts just above the equator, rises through the complete lower
   half, keeps the bead anchored, raises the lower belt/sidewall and bulges the
   sidewall outward.
7. Add repository validation requiring the CPU equilibrium/residual split and both
   embedded GLSL copies. Record the architecture in ADR-073.

### Validation completed before live user test
- Release C++ build succeeds with zero warnings and zero errors.
- Hidden runtime smoke test reaches the Racing United Lua prototype scene on
  OpenGL 4.6 with both mesh and shadow shaders compiling/linking successfully.

### Required live judgement
- Flat road: the bottom half should now read as one smooth loaded carcass, with a
  finite flat tread patch and broad sidewall transition rather than a vertex pinch.
- Sidewalk/kerb: the local residual should remain deepest at the obstacle while
  neighboring rubber bends smoothly into it.
- Rim and bead must remain rigid; no tongue, fin, inversion or opposite-wheel dent.

## Attempt 38 — TIRE36 / VIS29 Relaxed Lateral Carcass Silhouette

### Live evidence leading to this attempt
- TIRE35 was visibly better and finally moved a broad lower region.
- The user's close three-quarter view still showed a concentrated bottom lobe:
  the lateral carcass looked thumb-pressed instead of forming a long relaxed bend.

### Diagnosis
1. Sidewall-facing probes cast diagonally and could hit the same flat support plane
   already represented by native deflection. Their apparent compression survived as
   irregular detail and added a local bulge on top of the equilibrium bulge.
2. TIRE34's lateral shear reached full displacement at a lower-hemisphere dot of
   0.34. Most of the lower tire therefore moved as one chunk, leaving a visible
   transition shoulder instead of accumulating strain toward the footprint.
3. `max` joins between broad and local deformation envelopes introduced a derivative
   change exactly where the silhouette needed to remain relaxed.

### Changes
1. Classify detailed probe hits as duplicate primary support when their normal is
   aligned within the accepted support cone and their point is within 6 mm of the
   native contact plane. Such hits contribute only the analytic equilibrium.
2. Preserve differently oriented or height-offset contacts, so walls, kerb faces,
   raised tops, rocks and broken road remain irregular deformation sources.
3. Spread physical rigid-ring shear gradually across `-0.16..0.94` of the lower
   hemisphere instead of reaching full displacement at `0.34`.
4. Smoothly unite bead/sidewall shear support and broad/local radial envelopes;
   remove the hard `max` shape joins.
5. Let local sidewall bulge consume only probe compression above equilibrium, and
   reduce that redistribution gain. Native deflection remains the sole flat-road
   bulge authority.
6. Slightly reduce lower-carcass rise and bulge peaks while distributing them over
   the broader smooth envelope. Mirror every positional change in the shadow path.

### Required live judgement
- Cornering/lateral load should now produce one continuous equator-to-footprint
  bend with no isolated bottom lobe.
- Flat road must retain visible load deformation without a second sidewall dent.
- A real kerb/sidewalk contact must still create local indentation and smooth
  neighboring displacement.

## Attempt 39 — TIRE37 / VIS30 Residual-Only Irregular Contact Transport

### Live evidence leading to this attempt
- TIRE36 produced a much smoother and more realistic loaded silhouette.
- At almost zero inflation, small dents could still form in the collapsed lower
  carcass even on the ordinary support surface.

### Diagnosis
1. The CPU correctly separated smooth pneumatic equilibrium from irregular contact
   before carcass coupling, but recombined them in the 21x13 GPU payload.
2. Entity presentation then temporally filtered that combined field.
3. The shader recovered a supposed residual by subtracting the current equilibrium.
   At very low pressure, equilibrium deflection changes enough that its filtered
   previous value and current value differ, manufacturing local residual dents.

### Changes
1. Send only the bounded coupled irregular residual through the 21x13 payload.
2. Keep native radial deflection and contact-patch dimensions as the sole smooth
   equilibrium authority; they remain active whether or not irregular probes exist.
3. Remove both duplicate shader equilibrium reconstruction functions.
4. Consume residual probe compression directly for indentation and local rubber
   redistribution in both visible and shadow deformation paths.
5. Update repository guards so equilibrium cannot silently return to the filtered
   payload or be subtracted again in GLSL.

### Required live judgement
- Reducing pressure to minimum on flat ground should produce one broad, relaxed
  collapse without isolated probe-shaped dents.
- The bead/rim interface must remain rigid and the carcass must not invert.
- Kerbs and genuinely irregular surfaces must still produce bounded local detail.

## Attempt 40 — TIRE38 / VIS31 Outward Cross-Section Redistribution

### Live evidence leading to this attempt
- At 1.50 bar on the sidewalk, the unsupported lower side of the tire visibly
  curved inward when vehicle load should have expanded the loaded cross-section.
- The result looked like an inverted sidewall bulge rather than compressed rubber.

### Diagnosis
1. The 21x13 residual is scalar and deliberately relaxed between neighboring width
   bands for mesh-independent smoothness.
2. GLSL interpreted every inherited value as local inward compression.
3. A contact concentrated on one shoulder could therefore indent the contact side
   correctly while also pulling the free side inward; the previous centre-band-only
   bulge had no knowledge of this lateral asymmetry.

### Changes
1. Compare each residual sample with the same circumferential point on the mirrored
   width coordinate.
2. Apply shared compression radially, and reserve lateral inward displacement for
   compression genuinely exceeding the mirrored side.
3. Treat mirrored-side excess as displaced-volume demand and move the unsupported
   lower sidewall outward.
4. Build a bounded section-compression proxy from the centre and both outer shoulders
   so irregular vertical load can redistribute rubber to both sides.
5. Restrict the outward response to sidewall depth, preserve the bead/rim core and
   mirror the complete calculation in shadow geometry.

### Required live judgement
- On the sidewalk, the contact-facing patch may indent but the opposite lower
  sidewall should round outward rather than being sucked inward.
- A centred flat-road load should continue to expand both lower sidewalls evenly.
- The tire must retain its bead attachment and must not balloon at the tread crown.

## Attempt 41 — TIRE39 / VIS32 Relaxed Sidewall Volume Envelope

### Live evidence leading to this attempt
- TIRE38 removed the obvious inward/inverted free-side response and was judged miles
  better in the same sidewalk test.
- The lower sidewall still formed a separate swollen pouch rather than transitioning
  continuously from the tire equator into the footprint.

### Diagnosis
1. A hard `max` selected centre or shoulder compression as section authority. When
   dominance changed between samples, its derivative could move through the shape.
2. Both ordinary and free-side redistribution shared a late `0.30..0.92` bottom mask.
   Expansion therefore began too low and accumulated too abruptly.
3. The free-side response was still suppressed by a blend intended only to protect
   direct contact-side indentation.

### Changes
1. Replace the hard section maximum with a weighted RMS of both shoulders and the
   centre, giving centre load double weight without discontinuous source switching.
2. Spread redistribution from just above the equator to `0.84` lower-hemisphere
   participation instead of concentrating it at the footprint.
3. Separate common-section and free-side masks; retain stronger contact protection
   for the common response while allowing the unsupported side to relax naturally.
4. Slightly reduce the final width cap and retain sidewall-depth/bead constraints.
5. Mirror all deformation math in the shadow shader and guard it in validation.

### Required live judgement
- The unsupported sidewalk-facing silhouette should be one long, smooth curve with
  no distinct bottom pouch or hard shoulder.
- Contact-side rubber must still conform locally to the curb instead of floating.
- Centred flat-road compression must remain symmetric and free of tread ballooning.

## Attempt 42 — TIRE40 / VIS33 Native Reduced-Order Carcass Profile

### Live evidence leading to this attempt
- TIRE39 improved the loaded lower sidewall, but the sidewalk-facing silhouette
  could still show a discrete lateral kink.
- The remaining cross-section expansion was chosen inside GLSL from fixed gains,
  so no amount of shader smoothing could make it a physical carcass equilibrium.

### Diagnosis
1. MF6.2 already owns force and moment; it does not provide render-vertex shape.
2. TireRigidRing already owns belt translation/yaw/wind-up, but no native provider
   owned the actual left/right cross-section expansion presented to the mesh.
3. The shader reconstructed section compression and guessed displaced-volume gains.
   That duplicated tire-structure policy inside presentation and produced the kink.

### Changes
1. Add a deterministic native `TireCarcassProfile` provider with 21 bounded
   circumferential sections rather than hundreds of free soft-body vertices.
2. Solve negative-side expansion, positive-side expansion and lower-carcass rise in
   physical metres from tire/rim geometry, radial deflection, footprint, load,
   pressure and the existing irregular-road residual field.
3. Use an area-preserving cross-section relation with pressure-scaled participation;
   one-sided curb compression sends most displaced volume to the free sidewall.
4. Store and temporally damp the native profile in the entity tire state, then send
   identical arrays to visible and shadow vertex paths.
5. Delete shader-owned equilibrium bulge and irregular free-side transfer gains.
   GLSL now interpolates native state and preserves only mesh/bead/contact constraints.
6. Add deterministic regression checks for symmetry, pressure response, asymmetric
   free-side transfer, section bounds and adjacent-section continuity.

### Required live judgement
- At the same sidewalk pose, the exposed sidewall should form one continuous curve
  from equator to footprint, with no locally attached lobe or sharp red-line kink.
- The curb-facing side may indent while the opposite side expands outward.
- Low pressure may increase the broad response, but neither bead nor tread crown may
  invert or balloon.

## Attempt 43 — TIRE41 / VIS34 Single-Authority Flexible Ring

### Live evidence leading to this attempt
- TIRE40 still produced the same rectangular lower flap, slightly worse, during
  sidewalk contact.
- A source audit found eleven separate visible-shader position mutations and nine
  shadow mutations accumulated from earlier attempts. The problem was therefore
  architectural stacking, not one missing smoothing constant.

### Diagnosis
1. Contact planes, probe residuals, collider triangles, shader bulges, rigid-ring
   transforms and a separate carcass profile all attempted to own final geometry.
2. Their independent masks created derivative breaks and repeatedly moved the same
   lower vertices.
3. The replacement shader initially reconstructed the field's down axis with the
   cross product reversed; this was corrected before runtime validation.

### Changes
1. Delete all old tire-presentation setters, caches, profile provider, collider
   arrays, support-grid telemetry, uniforms and shader displacement statements.
2. Add one cyclic 24x13 flexible-ring field on an elastic foundation, driven by a
   bounded 21x13 collision lattice plus native deflection, patch, pressure, load,
   rigid-ring and flat-spot state.
3. Couple circumferential neighbours, width bands and near-incompressible section
   expansion in the solver so curb contact redistributes through one carcass.
4. Publish only one final forward/down/lateral metre-domain field. The visible
   shader adds it once; the shadow shader adds the same field once.
5. Reject side-facing collision casts that merely rediscover the ordinary support
   plane and would otherwise double-apply flat-road load.

### Required live judgement
- The lower tire must remain one continuous carcass on the sidewalk: no rectangular
  flap, attached pouch, single-row hinge or mirrored inward free side.
- Pressure reduction should increase broad sidewall compliance without spawning
  isolated dents.
- The unloaded upper belt and bead should remain stable while visible and shadow
  silhouettes agree.
