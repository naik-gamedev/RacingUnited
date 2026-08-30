# STUDIO06 - Interactive 3D Authoring

Heritage Studio now has a shared spatial authoring viewport for Scene, Race and Traffic data.

## Navigation
- RMB drag: orbit
- MMB drag: pan
- Mouse wheel: zoom
- F: frame the current selection
- Top / Front / Right camera shortcuts

## Editing
- World grid with configurable snap step
- Pick Scene/Race/Traffic markers directly in the viewport
- XYZ move gizmo for the selected point
- Direct ground-plane placement for spawns, triggers, checkpoints, pit boxes, AI-line nodes, lane nodes, intersections, parking, traffic lights and destinations

## Race visualization
- Ordered AI race line
- Ordered wet line
- Left/right track-limit chains when authored
- Selected-marker heading direction

## Traffic visualization
- Draft lane-node chain preview
- Node type coloring
- Heading direction for every road node

## Scope
This milestone is the interactive layout/editor layer. It intentionally does not boot the Racing United runtime. Full GLB/PBR scene rendering inside the Studio viewport remains a later rendering-integration milestone; authoring data and spatial editing are already usable without it.
