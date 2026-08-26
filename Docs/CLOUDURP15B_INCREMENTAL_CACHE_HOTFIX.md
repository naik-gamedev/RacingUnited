# CLOUDURP15B — Incremental cache / stale object hotfix

## Failure fixed

CLOUDURP15A could link against a stale `LuaPhysicsBindingRegistration.obj` left on the destination Windows machine. The linker then referenced eight Cloud Lab handlers that were not referenced by the current packaged registration source.

The source and linker object disagreed because the previous full ZIP accidentally included `Build/Cache/IncrementalSourceHashes.tsv`. Extracting that cache over an existing working tree made the incremental freshness guard believe some native inputs were already synchronized even though the destination `.obj` files had been compiled from different source contents.

## Fix

`Tools/EnsureIncrementalBuildFreshness.ps1` now uses a versioned cache format marker:

`# heritage-incremental-source-hashes-v2`

Any missing, legacy, or foreign cache is rejected once. With no trusted previous hashes, every tracked native input is content-hashed and touched, forcing MSBuild to reconcile its object files with the actual source tree. The guard then writes a v2 cache and subsequent builds return to normal incremental behavior.

The CLOUDURP15B hotfix ZIP intentionally contains **no `Build/Cache` directory**.

## Expected first run

The first CLOUDURP15B build should report a message similar to:

`Incremental freshness: legacy/foreign cache detected; invalidating once...`

followed by most/all native build inputs being touched. That first run can therefore compile substantially more files than a normal incremental build. Later runs should again touch only genuinely changed native inputs.

No volumetric-cloud rendering math was changed by CLOUDURP15B.
