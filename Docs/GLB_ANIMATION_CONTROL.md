# GLB animation control (GFX4)

GFX4 completes the first practical Heritage Engine GLB animation-control layer on top of GFX3 skinning.

## Runtime controls

Lua modules can control a mesh entity with:

```lua
Entity.PlayMeshAnimation(entity, "Idle", true, 0.20, true)
Entity.SetMeshAnimationSpeed(entity, 1.0)
Entity.SetMeshAnimationPlaying(entity, false)
Entity.SeekMeshAnimation(entity, 0.0)
```

`Entity.PlayMeshAnimation(entity, clipName, loop, crossFadeSeconds, restart)`

- `clipName`: exact glTF/Blender animation clip name. Empty string selects the first clip.
- `loop`: whether playback wraps at the end.
- `crossFadeSeconds`: transition time when switching between different clips.
- `restart`: if true, replaying the same clip starts it again from time zero.

`Entity.SetMeshAnimationPlaying(entity, playing)` pauses/resumes without losing the current runtime position.

`Entity.SetMeshAnimationSpeed(entity, speed)` changes playback rate. Negative values are accepted for reverse playback; the safe range is -32 to +32.

`Entity.SeekMeshAnimation(entity, seconds)` seeks the active clip to an explicit non-negative local time.

`Entity.GetMeshAnimation(entity)` returns:

1. requested clip name
2. playing
3. loop
4. speed
5. crossfade seconds
6. last requested seek time

## Blending

Switching to a different clip through `Entity.PlayMeshAnimation` performs a local-space pose crossfade when `crossFadeSeconds > 0`.

Translation and scale blend linearly. Quaternion rotations use shortest-path normalized interpolation.

## glTF interpolation

Animation sampler interpolation now supports:

- `STEP`
- `LINEAR`
- `CUBICSPLINE`

Cubic spline channels use glTF's in-tangent / value / out-tangent Hermite layout and normalize quaternion results for rotation channels.

## Existing behavior preserved

- OBJ/MTL rendering still works.
- Static GLB rendering still works.
- Embedded GLB textures still work.
- GLB skinning still works.
- An empty animation clip name continues to mean "first clip", preserving the previous automatic-play behavior.

## Joint palette note

The current vertex shader still uses a 128-joint palette per skinned draw range. This is deliberately retained as a compatibility/safety ceiling for the current renderer. Most normal game rigs are comfortably below it. Assets exceeding it emit a one-time runtime warning instead of failing silently.

A future renderer milestone can replace the uniform-array palette with an SSBO/texture-buffer path if very large skeletons become necessary.
