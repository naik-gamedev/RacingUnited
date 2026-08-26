# OPT00B — Profiling Validation Guard Fixes

This is a validator-only correction for OPT00. The repository had two stale/fragile assertions: one counted a trailing newline as an extra renderer source line, and another required the old `[0/4]` banner after OPT00 legitimately expanded the build pipeline to five stages. Both checks now validate the actual architectural intent rather than incidental formatting.
