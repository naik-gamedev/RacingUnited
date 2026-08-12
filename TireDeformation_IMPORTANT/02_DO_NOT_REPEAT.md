# DO NOT REPEAT THESE FAILED APPROACHES

1. **Do not assume a passing unit test means the live tire is deforming.** The user has repeatedly seen no meaningful live difference despite regressions/shader compilation passing.

2. **Do not add more Pacejka/traction/thermal complexity before visible deformation works.** The current priority is visual 3D deformation.

3. **Do not use a separate renderer collision reality and call it solved.** If renderer-only wrapping is used temporarily to prove visual geometry, explicitly label it as a visual proof. Long term, the same physical contact state should drive physics and visuals.

4. **Do not accumulate projection through every nearby collider triangle.** This caused exploding/spiked tires. Use a bounded dominant/contact-manifold style constraint or an iterative solver with physically meaningful constraints and strict stability limits.

5. **Do not mix coordinate spaces.** Historical attempts mixed absolute world collider triangles with camera-relative or differently posed tire vertices. Prefer one explicit space for the deformation test, ideally current tire-node local space for visual collision.

6. **Do not rank large triangles only by vertices or centroid.** A giant road/sidewalk triangle can pass directly under the tire while all vertices are far away. Rank using true closest point on finite triangle.

7. **Do not let dense flat-road tessellation consume every collision-candidate slot.** Reserve/select for non-coplanar/raised/wall/curb geometry or use a spatial query that cannot starve relevant nearby features.

8. **Do not treat the 32×7 or 9×7 lattice as isolated raycasts.** Structural/control nodes can drive deformation, but collision against the shell should be continuous enough that thin edges cannot fall between samples.

9. **Do not treat Magic Formula as collision detection.** MF consumes a contact state; it does not discover walls/rocks/curbs.

10. **Do not apply normal tread Magic Formula blindly to the sidewall.** Sidewall contact is primarily carcass/contact/friction behavior.

11. **Do not preserve old validation assertions simply because their milestone name is newer.** When rebasing to an older stable tree, validator rules from abandoned experimental branches can actively prevent the correct code from building.

12. **Do not keep patching an unverified live path.** First prove which shader/program/material/entity path renders the actual player tire. Add a deliberately impossible visual diagnostic if necessary (for example, temporarily move only one known tire vertex band by a huge amount under a debug flag) to prove the modified shader is the shader actually drawing the player's tire.
