# Heritage Engine Vegetation — Octahedral Foliage, Lighting, Wind, LOD and Authoring Design

**Project:** Heritage Engine / Racing United — The Virtual Heritage of Racing  
**Status:** Design specification / future implementation contract  
**Purpose:** Preserve the agreed vegetation direction so future work, contributors and AI coding agents do not replace it with a conventional billboard-only vegetation system.

---

## 1. Core decision

Heritage Engine should have **one general vegetation system**, not separate unrelated systems for trees, shrubs and grass.

The system must be able to represent:

- trees and woodland;
- shrubs, bushes and shrubland;
- grass and grassland;
- reeds, flowers, crops and other plant types;
- location-specific/modular vegetation libraries supplied by game modules and mods.

The renderer is free to use different representation strategies for different plant classes. The fact that all vegetation belongs to one system does **not** mean every plant must use octahedral impostors in the same way.

The central visual idea is:

> **Near trees and shrubs remain real 3D structures. Dense foliage is represented by many small octahedral foliage-cluster impostors attached to real branch/stem geometry. At greater distances, those clusters are progressively merged until an entire plant can become one impostor or an HLOD representation.**

An octahedral impostor is therefore a **building block inside the vegetation LOD system**, not the whole vegetation system by itself.

---

## 2. Near-field tree representation

For the highest-detail representation, the complete tree must **not** simply be replaced by one whole-tree billboard/impostor.

A typical LOD0 tree should conceptually be:

```text
Tree
├─ Trunk                         real 3D mesh
├─ Major branches               real 3D mesh
│  ├─ secondary branches        real/simplified 3D mesh
│  │  ├─ foliage cluster        octahedral impostor
│  │  ├─ foliage cluster        octahedral impostor
│  │  └─ foliage cluster        octahedral impostor
│  └─ ...
└─ ...
```

This preserves:

- the real trunk silhouette;
- meaningful branch silhouettes;
- believable gaps through the crown;
- branch-to-foliage attachment;
- hierarchical wind motion;
- much lower geometry cost than modeling every leaf as runtime geometry.

The source foliage used to bake a cluster may contain hundreds or thousands of leaves. That expensive source geometry is an **offline authoring/baking asset** and does not need to ship as runtime geometry.

---

## 3. Octahedral foliage cluster contents

A foliage cluster should eventually be able to store enough material information to react to dynamic lighting rather than behaving like a flat photograph.

Candidate baked data:

- base color / albedo;
- alpha / coverage;
- normal;
- roughness;
- depth;
- ambient occlusion / self-occlusion;
- leaf thickness or transmission information;
- optional specular/material information if the standard material representation requires it.

The exact texture packing is an implementation detail and should be optimized later. A possible packing could use a small number of atlases rather than six unrelated textures.

Example concept:

```text
High-detail source branch + leaves
                │
                ▼
        Offline impostor baker
                │
      ┌─────────┴──────────┐
      │ color / alpha      │
      │ normal / roughness │
      │ depth              │
      │ AO / thickness     │
      └─────────┬──────────┘
                │
                ▼
      Runtime foliage cluster
```

Depth-aware impostors are preferred for experimentation because they can preserve more apparent volume/parallax and can improve shadow behavior. However, the implementation must be profiled rather than assuming that every possible depth reconstruction technique is automatically worth its cost.

---

## 4. Structural AO and branch attachment

A foliage cluster attached deep inside a branch junction or crown is naturally darker than exposed outer foliage. Heritage should allow this structural occlusion to be authored cheaply.

### 4.1 Baked micro-AO

Tiny leaf-on-leaf and twig-on-leaf occlusion should primarily be **baked into the foliage cluster** offline.

The runtime should not attempt to dynamically calculate every microscopic leaf self-shadow.

### 4.2 Vertex-color / cluster structural AO

The cluster mesh or attachment region may also carry a vertex-painted or generated AO multiplier.

Example:

```text
outer crown             AO ≈ 1.00
moderately buried       AO ≈ 0.85
deep crown              AO ≈ 0.65
branch attachment       AO ≈ 0.55
```

These numbers are examples only, not fixed defaults.

The important rule is:

> Structural AO should primarily suppress **indirect/environment illumination**, not permanently darken the material's albedo and not prevent strong direct light from illuminating the leaves.

A branch junction can therefore look naturally dark under ambient lighting but still brighten when illuminated by the sun, a street lamp or a vehicle headlight.

This AO can be manually vertex painted for hero assets and eventually generated automatically by the procedural vegetation authoring tools.

---

## 5. Dynamic lighting and vehicle headlights

Octahedral foliage must participate in normal dynamic lighting.

A foliage pixel can contain a baked normal, allowing direct lights to calculate diffuse and specular response even though the underlying individual leaf is not runtime geometry.

This includes:

- sunlight;
- moon/environment contribution where appropriate;
- vehicle headlights;
- xenon/HID headlights;
- LED headlights;
- street lamps;
- lightning;
- other module-defined dynamic lights.

### 5.1 Headlight color response

A cool xenon light should **not paint the leaf blue**. Instead:

- the leaf's diffuse response remains based on its green/brown/etc. albedo;
- the illuminated diffuse result becomes a brighter version of that material under the light spectrum/color;
- the specular highlight can inherit the pale/cool color of the xenon source;
- transmission through thin leaves may produce a brighter saturated transmitted color rather than a flat white surface.

Conceptually:

```text
leaf body             material/albedo color
headlight illumination brighter material response
specular reflection   mostly light-colored/cool highlight
back-lighting         thickness/transmission response
```

### 5.2 Leaf transmission

Leaves are thin materials. A strong light behind foliage may partially transmit through them.

Future foliage shaders should therefore allow experimentation with a cheap thickness/transmission term:

```text
HEADLIGHT  →  leaf  →  CAMERA
                 │
                 └─ transmitted foliage light
```

This should remain scalable through quality settings and LOD. Distant foliage does not need expensive per-pixel transmission calculations.

---

## 6. Shadows: baked detail + dynamic silhouette

The agreed direction is **not** to use one fixed premade ground shadow for a plant.

A day/night cycle, moving sun, headlights and wind make a single fixed cast-shadow texture incorrect.

Instead divide shadowing into two jobs.

### 6.1 Offline / baked shadow information

Bake the expensive microscopic information:

- leaf-on-leaf self-occlusion;
- branch interior darkness;
- soft local AO;
- optional directional/self-shadow approximations if later experiments prove useful.

### 6.2 Runtime dynamic cast shadows

The large shadow cast onto the environment should respond dynamically to the current light direction.

Near foliage clusters can participate in a shadow pass using their alpha/coverage and, where worthwhile, depth information. This allows a cluster to cast a foliage-shaped shadow instead of the rectangular shadow of its underlying proxy geometry.

The same wind deformation used in the visible foliage shader should also be available to the vegetation shadow shader so moving branches and crowns produce moving shadows.

Conceptually:

```text
weather wind
     │
     ├─ visible vegetation deformation
     └─ shadow-pass vegetation deformation
```

### 6.3 Shadow LOD

Shadow quality must reduce with distance independently from visual LOD.

Possible strategy:

```text
LOD0 / close
  detailed cluster alpha/depth shadow

LOD1
  merged-cluster shadow

LOD2
  simplified crown shadow

LOD3 / whole-tree impostor
  cheap whole-tree silhouette shadow

very far / forest HLOD
  broad canopy / terrain-scale shadow approximation
```

Grass should be treated more aggressively. Near grass may cast individual/small-clump shadows on higher quality settings, while distant grass should rely on cheaper AO/contact/terrain-level shading rather than millions of tiny shadow casters.

---

## 7. Overdraw and foliage-layer cost

A major vegetation performance risk is **overdraw**, not only triangle count.

Hundreds of overlapping transparent foliage surfaces can cause the GPU to shade the same screen pixel many times.

The system should therefore prefer:

- alpha-tested/masked foliage where appropriate rather than conventional blended transparency;
- depth writing / early depth rejection when technically compatible;
- foliage clusters large enough to represent useful crown volumes rather than hundreds of tiny cards;
- offline cluster generation that minimizes cluster count while preserving silhouette and apparent volume;
- distance-based simplification;
- optional limits/budgets for expensive transmission or layered lighting.

A possible quality concept for **expensive** foliage transmission/shading layers is:

```text
Low      very small expensive-layer budget
Medium   moderate budget
High     larger budget
Ultra    adaptive/larger budget
```

This is **not** permission to simply delete all foliage after two layers. Deep foliage should remain visually dense; only the expensive shading path should be simplified after the relevant cost budget is reached.

The final method must be based on profiling and visual testing rather than a hard-coded arbitrary layer count.

---

## 8. LOD strategy

LOD should reduce both geometry and the number of foliage clusters.

The exact distances are content-, hardware- and quality-dependent and must not be hard-coded into this document as permanent numbers.

### 8.1 Trees

Conceptual progression:

```text
LOD0
  detailed real trunk + branches
  many small octahedral foliage clusters

LOD1
  simplified branch structure
  fewer / merged foliage clusters

LOD2
  major structure only
  a few large crown clusters

LOD3
  one whole-tree octahedral impostor

LOD4
  forest HLOD / grouped canopy representation

Extreme distance
  terrain/distant-landscape forest representation or no individual trees
```

The important design decision is that **whole-tree octahedral impostors are allowed and desirable at distance**, even though the near representation uses branch-level clusters.

### 8.2 Shrubs and bushes

Shrubs are particularly suitable for foliage clusters because they contain relatively little visually important large woody geometry.

Conceptual progression:

```text
LOD0: real stems + multiple foliage clusters
LOD1: simplified stems + merged clusters
LOD2: a few large volumetric clusters
LOD3: one whole-shrub octahedral impostor
```

### 8.3 Grass

Individual grass blades probably do **not** need octahedral impostors at LOD0. A blade/card is already cheap.

However, **grass clumps/patches are good impostor candidates**.

The current authoring idea is to experiment with roughly **0.5 m grass clumps**. One metre may also be useful in some environments, but 0.5 m provides finer placement control and avoids an overly coarse grid appearance.

Conceptual progression:

```text
near
  actual blades / cards / inexpensive geometry

mid
  simplified 3D clumps

farther
  octahedral clump/patch impostors

very far
  terrain-level grass coverage/color/normal response
```

Octahedral representation should therefore be an optional tool for dense grass **clumps**, not a requirement for every individual blade.

---

## 9. Million-tree scalability

A world database containing 1,000,000 trees must **not** imply that the renderer evaluates every tree or every foliage cluster each frame.

Example worst case to avoid:

```text
1,000,000 trees × 100 foliage clusters = 100,000,000 active clusters
```

This would be unacceptable if processed simultaneously.

Instead, the runtime should progressively reduce representation cost:

- only a limited nearby set uses detailed branch-level clusters;
- medium-distance plants use merged clusters;
- far trees/shrubs become one whole-plant impostor each;
- very distant groups become forest/vegetation HLOD;
- occluded/off-screen/unloaded plants cost effectively nothing in rendering;
- world streaming keeps only relevant vegetation chunks resident;
- GPU instancing should be used for repeated plant assets;
- future GPU-driven culling/indirect rendering is desirable when justified.

The relevant performance question is therefore not "how many trees exist in the world?" but:

> **How many visible instances, clusters and pixels are being processed in the current frame?**

---

## 10. Chunked and quantized placement

Vegetation placement should be compatible with Heritage's large-world architecture.

A good model is:

```text
FP64 / integer global chunk address
             +
compact local position within chunk
             ↓
camera-relative FP32 rendering
```

VEG01 established the direction of using **64 m chunks with 16-bit local coordinate quantization**, which gives approximately millimeter-scale local placement precision.

This is well beyond what a grass clump or tree requires while remaining compact enough for huge instance counts.

This also means a 0.5 m grass clump can be positioned very precisely without giving every vegetation instance three FP64 global coordinates.

Compact instance data should be preferred for:

- local X/Y/Z or terrain-relative placement;
- rotation;
- scale;
- species ID;
- deterministic variation seed;
- biome/growth parameters where needed.

Exact packing should be optimized after the final chunk/streaming representation is established.

---

## 11. Wind architecture

Wind must come from the shared environment/weather simulation rather than every tree inventing its own unrelated animation.

Conceptually:

```text
Weather / wind field
        │
        ▼
Vegetation system
        │
        ├─ trunk / main stem bend
        ├─ primary branch sway
        ├─ secondary branch response
        └─ foliage / grass flutter
```

Different species and plant classes should have different response profiles.

Examples:

- large oak: heavy/stiff trunk, slower crown response;
- birch: more flexible trunk/branches;
- conifer: different branch distribution and damping;
- shrub: strong stem/crown response;
- grass: high-frequency blade/clump bending and flutter.

Potential future inputs from the environment simulation:

- wind velocity/vector;
- gust strength;
- turbulence;
- wetness/rain effects;
- local shelter/exposure if such a system is later implemented.

The CPU should **not update every leaf or foliage-cluster transform individually every frame**. Hierarchical parameters and shared wind fields should allow large amounts of visible foliage motion to be evaluated efficiently on the GPU.

---

## 12. AI-assisted / procedural Blender authoring

A major long-term goal is for an AI-assisted Blender script/tool to generate plant variations offline.

The system should operate from a **species definition** rather than requiring thousands of manually modeled individual trees.

A species definition can eventually describe things such as:

- height/age range;
- trunk taper and curvature;
- branching angles and distributions;
- crown shape;
- branch droop;
- asymmetry;
- foliage density;
- branch/leaf source assets;
- seasonal variants;
- damage/missing branches;
- wind response profile;
- LOD generation rules.

Conceptually:

```text
Species definition
+ source branch/leaf assets
+ deterministic seed
          │
          ▼
AI-assisted Blender vegetation generator
          │
          ├─ unique trunk/branch structure
          ├─ foliage-cluster placement
          ├─ structural AO generation
          ├─ LOD generation / cluster merging
          ├─ whole-plant impostor bake
          └─ Heritage-ready 3D asset + metadata
```

The generator should produce genuinely different silhouettes and structures rather than merely rotating the same tree.

A deterministic seed is desirable so the same plant can be regenerated later.

The first real octahedral baker/shader should be developed against a **small sacrificial reference asset** (for example one leafy branch or one ~0.5 m grass clump) so the renderer can be compared directly with the source geometry in Blender.

---

## 13. Modular locations and biome content

The engine vegetation technology must not be hard-wired to Slovenia or any other single region.

The intended content model is modular:

- Racing United can ship region/location-specific vegetation libraries;
- mods/modules can define their own species sets and biome rules;
- Germany, France, USA, South Africa, Brazil, etc. can use different plant libraries without changing the engine;
- a location pack should select the vegetation appropriate to that environment rather than using one universal "game forest" asset set.

Racing United's Slovenian content is an initial practical use case, including planned motorsport/free-roam environments such as Ivarčko Jezero, Vipava-area rally/free-roam content and Pod Nanos hill-climb content. These locations can eventually define appropriate local tree, shrub and grass species libraries while using the same engine vegetation renderer.

The species list itself is **content data**, not an engine hard-coded list.

---

## 14. Runtime lighting/shadow performance policy

The vegetation renderer must be scalable from low-end hardware to high-end systems.

Quality settings may independently control:

- near foliage cluster density;
- LOD transition distances;
- whole-tree impostor distance;
- shadow distance and shadow LOD;
- transmission/back-lighting quality;
- depth-aware impostor quality;
- grass density;
- wind detail;
- HLOD thresholds.

The system should always prefer graceful degradation rather than turning an entire feature off unnecessarily.

Example:

```text
High quality
  depth-aware clusters + transmission + detailed near shadows

Medium
  same basic vegetation structure, cheaper transmission/shadows

Low
  earlier cluster merging / whole-plant impostors, reduced shadow distance
```

The architectural principle is:

> **Keep the visual idea intact while changing how much work is spent proving it.**

---

## 15. Performance monitoring requirements

Vegetation development must be driven by measurements.

The existing performance-monitoring direction should eventually expose at least:

- vegetation CPU update time;
- vegetation GPU time where measurable;
- visible vegetation instance count;
- visible tree/shrub/grass count by LOD;
- visible foliage-cluster count;
- whole-plant impostor count;
- forest/HLOD count;
- vegetation draw/dispatch count;
- triangle count for 3D vegetation;
- estimated/observable foliage overdraw if practical;
- vegetation memory usage;
- streamed vegetation chunk count;
- culling counts.

This prevents the project from guessing whether a frame-rate regression comes from vegetation, physics, rendering submission, shadows, material complexity, AI or another system.

---

## 16. Suggested implementation roadmap

VEG01 is the architectural foundation. Future work should remain modular.

### VEG02 — First octahedral foliage experiment

- create one test leafy branch and/or ~0.5 m grass clump;
- implement/bake the octahedral view representation;
- render base color + alpha + normal;
- evaluate depth-aware version;
- compare against source geometry in Blender;
- measure view-transition artifacts and overdraw.

### VEG03 — Dynamic lighting and material response

- sunlight and local lights;
- headlights including xenon/LED color response;
- roughness/specular;
- optional thickness/transmission;
- structural AO rules.

### VEG04 — Vegetation shadows

- foliage-shaped alpha/depth shadow casting;
- shared wind deformation in visible and shadow passes;
- shadow LOD and distance policy;
- cheap grass shadow policy.

### VEG05 — Hierarchical wind

- shared weather wind field;
- species wind profiles;
- trunk/branch/foliage response hierarchy;
- grass-clump behavior.

### VEG06 — LOD merging and whole-plant impostors

- branch-cluster merging;
- whole-tree / whole-shrub octahedral impostors;
- grass clump distance representations;
- automated transitions.

### VEG07 — Procedural Blender vegetation authoring

- species definitions;
- deterministic generation;
- AI-assisted variant generation;
- automatic cluster placement;
- automatic structural AO;
- automatic LOD/impostor bake pipeline.

### VEG08+ — Large-scale world vegetation

- streaming;
- GPU instancing;
- GPU-driven culling/indirect work where useful;
- forest HLOD;
- biome/location placement tools;
- million-instance stress testing and optimization.

The numbering above is a planning guide, not a requirement that every feature must use exactly these milestone names.

---

## 17. Explicit non-goals / things to avoid

Do **not** accidentally evolve the system into any of the following without a deliberate design change:

1. **One flat billboard for every nearby tree.** Near trees should retain real 3D structure.
2. **Individual runtime geometry for every leaf.** High-detail source leaves are primarily bake inputs.
3. **CPU simulation of every leaf/cluster every frame.** Wind should be hierarchical and GPU-friendly.
4. **One fixed premade cast shadow for a tree.** Baked self-shadowing is useful; large cast shadows must respond to current lighting.
5. **AO baked into albedo so strongly that headlights cannot illuminate foliage.** AO is primarily an indirect-light modifier.
6. **Unlimited translucent overdraw.** Prefer masks/depth/culling and controlled cluster counts.
7. **100 foliage clusters for every tree at every distance.** Cluster count must collapse aggressively with LOD.
8. **A Slovenia-only species system.** Biomes/species are modular content.
9. **Octahedral impostors for every individual blade of grass.** Use them where they reduce complexity, especially clumps/patches and farther LODs.
10. **Optimization by visual destruction.** Prefer scalable representation and quality tiers.

---

## 18. Short design summary

Heritage vegetation should ultimately work like this:

```text
GLOBAL WORLD / BIOME DATABASE
            │
            ▼
       streamed chunks
            │
            ▼
  vegetation instance/culling
            │
      ┌─────┴───────────────┐
      │                     │
   near plant             far plant
      │                     │
real trunk/stems       whole-plant impostor
+ octahedral foliage         │
clusters                     ▼
      │                  forest HLOD
      ▼
shared weather wind
+ dynamic sun/headlights
+ baked micro AO/self-shadow
+ dynamic cast-shadow LOD
```

The design goal is a vegetation system that can look volumetric and react believably to wind, headlights and the day/night cycle while still scaling to enormous modular open worlds.

**Spend geometry and shader work close to the player where it is visible. Merge aggressively with distance. Bake microscopic detail. Keep large lighting changes dynamic. Measure everything.**
