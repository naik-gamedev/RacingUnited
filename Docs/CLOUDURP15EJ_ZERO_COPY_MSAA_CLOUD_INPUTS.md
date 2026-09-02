# CLOUDURP15EJ — Zero-copy MSAA cloud inputs

CLOUDURP15EJ removes the redundant full-resolution camera color resolve and
same-sample depth copy that previously preceded every volumetric-cloud frame.
It does not reduce cloud quality.

The main post framebuffer now stores multisampled color and depth/stencil in
texture-backed attachments. Cloud raymarch, full-resolution composition,
temporal reconstruction, celestial shafts and ground-cloud shadows read those
exact attachments directly. When writing the resolved cloud result back, the
depth attachment is detached briefly to avoid an OpenGL framebuffer feedback
loop and is restored before later scene passes. Display-spanning and future
unsupported framebuffer types retain the original staging-copy fallback.

At the production 3840x2100, FXAA + MSAA x4 configuration, asynchronous GPU
queries measured the scene-copy pass falling from approximately 3.1 ms to
0.001 ms. Exact four-sample scene reconstruction adds approximately 0.28 ms to
the full-resolution composition pass, leaving a net saving of about 2.8 ms per
frame. The following remain unchanged:

- full-resolution cloud raymarch targets;
- 64 primary volume steps and occupied-interval behavior;
- physical Sun and Moon scattering;
- bilateral reconstruction;
- full-resolution temporal history and rejection;
- cloud depth, shafts and ground-shadow presentation;
- authored regional weather and cloud morphology.

The native shader path was launched after a Release build with no shader or
framebuffer errors. Physics regressions and static project validation remain
mandatory for the milestone.
