# STUDIO12 — Free-Roam Road Construction / Junctions / Navigation

STUDIO12 extends the existing HROAD node/link graph. It does not replace or remove the STUDIO10 graph authoring layer.

## HROAD v3

HROAD v3 adds a high-level free-roam road-authoring layer:

- road splines with Motorway / Arterial / Collector / Local / Residential / Service / Mountain / Gravel / Dirt classes;
- forward/backward lane counts, one-way policy, lane width, shoulders, median, sidewalks and curb parking flags;
- road speed limits, traffic density and spawn weighting;
- ordered spline control nodes with Bezier handles, automatic tangent mode, banking and width scaling;
- authored intersections with priority-road / yield / stop / signalized / roundabout / uncontrolled policies;
- legal turn connectors with source/destination road and lane, yield, U-turn and turn-speed metadata;
- traffic-signal phases with green/yellow/all-red timing and connector groups;
- parking strips with road attachment, count, spacing, angle, side and occupancy;
- global traffic-population mix and behavior parameters;
- navigation-build parameters for slope, turn radius, lane-change length and junction/merge lookahead.

HROAD v1 and v2 remain loadable.

## Lane graph compiler

The higher-level spline layer can synchronize deterministic `AUTO_LANE_*` TrafficNode objects and directed TrafficLink objects. Lane center offsets are derived from carriageway geometry. Existing hand-authored graph nodes and links are preserved.

The generated graph is therefore a lower-level runtime/navigation layer, while road splines remain the editable world-production authority.

## Heritage Studio UI

Traffic now exposes four additive sub-workspaces:

1. Graph / Overrides — existing STUDIO10 nodes and explicit links.
2. Road Splines — surface tracing, road class and carriageway properties, control-node/tangent editing.
3. Junctions / Signals — intersections, right-of-way, legal turns and signal phases.
4. Parking / Population / Nav — parking generators, traffic population and lane-graph compilation.

The 3D viewport displays road centerlines, approximate carriageway envelopes, control nodes, intersections, parking anchors and the existing low-level graph.

## Runtime bridge

`StudioGameplay.lua` v3 publishes road splines, spline nodes, intersections, turns, signal phases, parking strips, traffic-population metadata and navigation settings.

`Runtime/StudioGameplay.lua` adds queries for roads, ordered road nodes, lane descriptors, network summaries, intersections, legal turns, signal phases, parking strips, nearest road nodes, traffic-population configuration and navigation-build configuration.
