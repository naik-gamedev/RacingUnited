# LIVETRACK22 — Unified driven-surface tire-mark tiles

## Near authority

The production Dynamic Surface near working set uses 10 m world tiles at 256×256 texels. LIVETRACK22 adds an R8 tire-mark material atlas with the **same resident tile slots and the same tile-indirection texture** already used by water/snow/mud. It is not a second spatial partition.

Authoritative tire-mark history remains `SurfacePresentation` FP64 segments with the existing one-million-segment / twenty-minute bound. New segments are indexed by 10 m world tile. A resident tile rasterizes only its intersecting serials; a tile that leaves residency and later returns is reconstructed from that authoritative history.

This removes near coplanar ribbon depth fighting. The scene road material samples the R8 mark state directly.

## Far authority

The existing persistent 100 m FP64-origin/FP32-local GPU chunk records remain as an **invisible persistence/far LOD only**. They cross-fade in from 85–110 m and continue to the existing 500 m visual range. Range visibility now fades continuously over the full 0–500 m domain instead of staying opaque until a narrow terminal fade band.

The far vector LOD is lifted 1.25 mm along the recorded support normal and uses negative polygon offset so it is biased toward the camera rather than behind the road.

## Rain interaction

Close material marks are attenuated by the exact same live Dynamic Surface wet film and standing-water state that shades the road. Far-vector marks use regional effective wetness. New mark transfer at full wetness is reduced from 28% of dry to about 8%, retaining only a small hot-rubber smear contribution.

## Unified-system boundary

LIVETRACK22 concretely moves tire marks into the common Dynamic Surface near tile domain. Water/puddles already use it. Snow/mud already have optional atlases in the same runtime. Persistent marbles remain owned by `TrackRubberState` and dust remains a transient particle mechanism; they have different physical lifetimes and are not incorrectly converted into tire-mark pixels by this milestone. The shared tile/indirection pattern is now available for future material-state channels where that representation is physically appropriate.

## Wheel visual companion fix: VA02J

Embedded GLB local rotation offsets previously composed wheel spin after authored scale (`T*R*S*Q`). For orthogonal authored TRS nodes VA02J composes spin before scale/reflection (`T*R*Q*S`), preserving the bind transform at zero spin and preventing non-uniform/mirrored pivot scale from making rim spokes wobble/shear while a round tire hides the problem.
