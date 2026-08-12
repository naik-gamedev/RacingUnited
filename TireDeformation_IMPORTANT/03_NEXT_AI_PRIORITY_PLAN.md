# Next AI priority plan

## Priority 0 — prove the code path is live

Before designing another carcass model, prove that the exact shader / renderer code being edited is the code drawing the player's visible tire.

Recommended temporary diagnostic:
- Add a developer-only toggle such as `Tire Visual Path Proof`.
- When enabled, deliberately move the lower tread of one tire by an absurd but bounded amount (for example 50-80 mm inward) or apply a distinctive geometry-only deformation.
- Do not rely on contact data for this proof.
- If the user cannot see the forced deformation, stop immediately: the wrong render path/material/entity is being edited.

This is the single most important diagnostic that previous attempts did not conclusively establish in the live game.

## Priority 1 — flat plate, one tire, stationary

Remove motion/curb/MF complexity.

For one stationary tire:
1. Identify actual rendered tire mesh and its local coordinate system.
2. Identify actual road collider triangle(s) directly under that visible tire.
3. Log/draw tire local bounds and collider triangles after transformation into the same space.
4. Force a known 10-20 mm geometric overlap in a controlled test.
5. Confirm lower tread vertices are moved out of the collider.
6. Confirm the result is visible on screen.

Only after this passes should wall/curb wrapping be debugged.

## Priority 2 — longitudinal sidewalk-edge indentation

This is the user's most important shape test:
- Put left tires with the sidewalk edge running along the tire's travel direction under the tread center.
- Center tread should indent locally.
- Shoulders/sidewalls should respond smoothly rather than whole tire translating or one ring flattening.

Use a 2D structural deformation basis across circumference × width. Render vertices interpolate from it.

## Priority 3 — lower-front wall/curb

- Lower-front tread/shoulder contacts a wall/curb.
- Rubber must visibly compress at that region.
- No clipping through wall.
- No spikes.

## Priority 4 — structural tire behavior

Once collision-driven visible deformation is unquestionably live:
- bead anchoring;
- radial and lateral stiffness;
- tread/belt bending stiffness;
- circumferential coupling;
- shear;
- damping;
- pressure/volume resistance;
- smooth neighboring bulge.

A reduced control lattice is acceptable; the GPU render mesh can be much denser.

## Priority 5 — unify with physics/traction

Only after visual deformation works:
- Feed the same contact manifold into tire force generation.
- Use actual contact position and normal.
- Local tread contacts may use MF6.2 in their local tangent frame.
- Sidewall contacts use structural/friction response.
- Sum forces and moments at the hub.

## Performance target

The final architecture must scale to up to ~150 cars / ~600 tires on Nürburgring 24h-style grids. Therefore:
- cheap normal-road mode for most tires most frames;
- detailed lower-shell contacts wake only when geometry is complex;
- bounded contact manifold;
- GPU interpolation for dense visual mesh;
- no brute-force all tire nodes × all world triangles.

Correctness comes first during the isolated visual proof. Optimize after the path demonstrably works.

## TIRE23 live-proof protocol (added after TIRE22R2 failure)

Do not tune anything before reading the TIRE23 result.

- If the unconditional 45 mm lower-half squash is invisible, inspect which draw path actually renders `WH_*_Tire`, whether the node is recognized by `computeTireVisualGeometry`, and whether `SetMeshNodeTireDeformation` creates the override used by that draw range. Do not touch collider/contact math yet.
- If the unconditional squash is visible but the collider-dependent front/lower notch is not, trace `WheelState.tireVisualColliderTriangleCount` -> `Entity.SetMeshNodeTireColliderTrianglesFromWheel` -> `MeshNodeOverride.tireVisualColliderTriangleCount` -> renderer uniform upload.
- If both proof shapes are visible, delete the proof block and debug only the actual local-space triangle constraint using one stationary tire and one flat/vertical test triangle.

The rodeo/bucking curb response is a separate physics issue. Keep it isolated until visible deformation authority is proven.

## After TIRE23 proof / TIRE24

TIRE23 proved the visible shader path.  Future attempts MUST NOT waste time asking whether `EntityMeshShaders.hpp` draws the player's tire unless the renderer architecture changes later.

Current order:
1. Make real collider geometry produce convincing local 3D carcass shape in the proven shader path.
2. Validate centre-tread longitudinal-edge indentation and front/lower wall indentation.
3. Only after visual wrap is visibly correct, address the independent sidewalk bounce/support problem and unify physical contacts with the visual state.

Do not reintroduce the TIRE23 forced squash except as a temporary diagnostic.  It was intentionally nonphysical and served its purpose.
