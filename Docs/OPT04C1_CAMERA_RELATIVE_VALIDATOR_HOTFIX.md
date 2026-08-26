# OPT04C1 — Camera-relative validator ownership hotfix

OPT04C moved per-instance camera-relative frame preparation from `EntityMeshRenderer.cpp` into `EntityMeshAnimation.cpp` so the shadow and material passes can share the same prepared data.

The large-world precision behavior did not change: `prepareFrameInstances()` still subtracts the camera eye from the FP32 local instance position before `modelMatrix()` submission. The repository safety-net check was stale because it searched only `EntityMeshRenderer.cpp` for `cameraRelativeInstance.position`.

OPT04C1 updates that check to require both sides of the new ownership boundary:

- `EntityMeshRenderer.cpp` calls `prepareFrameInstances(instances, eye, elapsedSeconds)`.
- `EntityMeshAnimation.cpp` constructs `cameraRelativeInstance.position` from `instance.position - eye` on all three axes and submits it through `modelMatrix(cameraRelativeInstance)`.

No renderer, shadow, shader, water, weather, or physics runtime code is changed by this hotfix.
