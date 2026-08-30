# VA02K — Geometric embedded-wheel spin axis

VA02J corrected TRS/scale ordering for embedded GLB wheel pivots, but a visible rim could still precess if the authored `WH_*_Pivot` local X basis was even slightly non-coaxial with the actual tire/rim geometry. A rotationally symmetric tire can hide that error while spokes make it obvious.

VA02K keeps the existing telemetry spin angle but derives the spin line from the already measured `WH_*_Tire` geometry:

- centre = inferred tire mesh centre;
- axle = inferred tire mesh axle axis transformed through its authored bind transform;
- sign = matched to the historical Pivot-local +X direction so existing wheel-spin direction is preserved;
- suspension/upright runtime delta moves that bind centre/axle into the current wheel pose;
- spin is applied as one rigid mesh-global axis-angle transform around that exact line.

This removes Pivot-axis precession and also bypasses local non-uniform scale/reflection as a source of wheel-spin wobble. Generic non-wheel local rotation offsets retain their existing semantics.
