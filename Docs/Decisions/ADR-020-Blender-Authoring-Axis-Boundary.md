# ADR-020 — Blender Authoring Axis Boundary

## Status
Accepted as a safety rule after FIX01; a later native Z-up migration remains
possible as its own tested milestone.

## Creator-facing convention
Blender-authored Racing United content uses Blender semantics: X left/right,
Y longitudinal, Z up. The current Peugeot points toward Blender -Y. Correct
Blender geometry must not be rotated or mirrored merely to satisfy engine
internals.

## Current runtime
Heritage's current native rigid-body/vehicle convention is X right, Y up,
Z forward. glTF is Y-up and Blender's exporter performs the standard basis
conversion. Native axes are an implementation detail, not an authoring rule.

## Boundary rule
Axis conversion belongs at explicit import/authoring-to-runtime boundaries.
It must not be independently recreated in tire code, Ackermann code, camera
code, GLB visual binding, or individual content scripts.
