STEP 29J.1 ARTICULATED WHEEL SLOT
=================================

Default slot:
    PlayerWheel.obj

Authoring contract:
    origin = exact wheel center
    +X = axle
    +Y = up
    +Z = forward
    outer/visible wheel face = local +X
    units = metres

The engine places every wheel at the authoritative native WheelState.worldCenter.
Left-side instances are turned 180 degrees around Y and their visual spin sign is
compensated so the same shared wheel asset can face outward on both sides.
Per-wheel asset paths are supported when a car needs front/rear or left/right
specific wheel geometry.

Current 2003 Peugeot 206 RC reference geometry:
    wheelbase = 2.442 m
    front track = 1.437 m
    rear track = 1.428 m
    tire = 205/40 ZR17
    derived unloaded tire radius = 0.2979 m

The original Step 29J placeholder OBJ itself is 0.42 m radius and about
0.1711902 m wide, so the prototype definition scales that placeholder to the
reference tire envelope. A properly authored 1:1 wheel mesh can use visual scale
1.0 instead.

This remains presentation/reference work; the final production Peugeot vehicle
will receive researched suspension, tire, drivetrain and alignment data.
