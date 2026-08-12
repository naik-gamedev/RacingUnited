# ADR-064 — Pressure-Resolved Continuous Tire Marks

**Status:** TIRE16G candidate  
**Date:** 2026-08-11

## Context

The earlier driven-surface presentation could draw bounded deformable-terrain ruts, but a tire-skid implementation based on independent rectangular stamps would expose segment boundaries and break immersion. Racing United already computes the tire quantities that should determine a mark: contact load, slip velocity, dissipated slip work, tread temperature and TIRE08's three-band lateral load distribution derived from pressure/camber.

## Decision

Tire marks are a continuous, wheel-owned world-space trail rather than square decals.

- Each physical wheel owns a stable `sourceStreamId`.
- High-rate 1 kHz contact samples are distance-resampled into bounded FP64 `SurfaceTireMarkSegment` history; the renderer never receives one decal per tire substep.
- Each segment stores different start/end intensity values, so changing physical slip work produces a smooth longitudinal darken/fade gradient.
- Each endpoint stores a three-band negative-right/centre/positive-right pressure/load profile derived from TIRE08 tire state. The renderer interpolates it across the width, allowing camber/load/pressure to make one region subtly darker without authored fake striping.
- The lateral edge envelope fades continuously to zero. Connected segments suppress their touching longitudinal caps; only exposed trail ends receive a soft longitudinal feather. There are no rectangular alpha borders.
- Formation intensity is continuous and depends on dissipated slip work, slip speed, load, tread temperature, wetness and receiving hard-surface material. Ordinary near-pure rolling therefore does not paint the road, while braking lockup, wheelspin and sliding become progressively stronger.
- Stationary/near-stationary wheelspin can leave sparse overlapping short footprints rather than requiring vehicle translation.
- Dry visual history does not arbitrarily disappear. Fresh marks age smoothly toward approximately half their deposited visibility; explicit weather/abrasion removal can be added later.
- Rendering is camera-bounded and presentation-only. Tire forces remain authoritative in the tire/surface solver and do not read tire-mark presentation state.

## Consequences

The mark can vary continuously in both dimensions: across its width from physical contact-pressure asymmetry, and along its length from changing slip/rubber-transfer conditions. Multiple marks can layer naturally, producing track history without the visible tiled/square look of decal stamping. The bounded segment store is suitable for the current feature stage; endurance-scale persistence/streaming is a later scalability gate.


## TIRE16A visible-transfer gate

The first TIRE16 calibration deliberately allowed mild scrub to leave a subtle
trace. In live vehicle testing this proved too permissive: ordinary traction can
contain small physical slip and non-zero dissipation without producing a
visible black tire mark.

TIRE16A therefore keeps the response continuous but gates visible transfer with
the tire model's existing `gripUtilization` output in addition to slip speed and
dissipated slip power. Microscopic rolling/traction slip remains part of the
tire physics but does not visibly paint the road. Wheelspin, lockup, near-limit
braking and high-angle sliding naturally approach the visible-transfer region.
A small slip-power deadband also rejects low-energy ordinary traction.


## TIRE16B genuine-slide gate and neutral rubber color

Live TIRE16A testing showed a second false-positive class: an ordinary road-speed
roundabout can use substantial lateral grip and dissipate several kilowatts while
the tire remains in normal elastic cornering. `gripUtilization` and slip power are
therefore necessary but not sufficient indicators of visible rubber smearing.

TIRE16B passes the tire model's explicit `slipRatio` and `slipAngleDegrees` into
`SurfacePresentation`. Visible transfer now also requires continuous kinematic
slide activation: longitudinal activation ramps from 0.12 to 0.30 absolute slip
ratio, while lateral activation ramps from 7 to 15 degrees absolute slip angle.
Either lock/wheelspin or genuine high-angle lateral sliding can open the gate.
Slip speed, dissipated power, grip utilization, load, temperature, wetness and
material remain continuous intensity factors after that gate. This keeps ordinary
cornering visually clean without using a binary skid flag.

The tire-mark renderer also uses neutral grayscale charcoal for all supported hard
surfaces. The earlier tiny red-over-green-over-blue bias was visible as brown under
night/cool lighting; surface type may now change darkness but not rubber hue.


## TIRE16C persistence, night neutrality and render scalability

Live TIRE16B testing confirmed the genuine-slide gate, but exposed three presentation
issues: the old renderer stopped showing new segments when its first-in-vector 9000
segment cap filled, the 115 m draw range was too short, and the generic track shader's
per-object gamma/brightness transform lifted translucent charcoal into a brownish
night-time streak. The 420 s age constant also made dry history visually weaken too
quickly for a circuit that should retain evidence of use.

TIRE16C therefore separates tire marks into a dedicated neutral-dark overlay shader,
keeps their RGB spectrally neutral, and does not apply the generic track presentation
gamma/brightness lift to the mark itself. Tire-mark draw distance increases to 325 m.
Geometry is selected camera-nearest under a 110k-triangle budget with 9/5/3 lateral
samples in near/mid/far distance bands; longitudinal feather caps are only tessellated
in the near field. This replaces the old first-9000 cutoff, so a newly created mark near
the followed vehicle cannot disappear merely because older history already consumed a
slot budget. Selected marks are then rendered oldest-to-newest for correct layering.

Dry visual ageing now approaches the same 50% floor with a 3600 s time constant rather
than 420 s. Physical weather/abrasion removal remains future authoritative work.

Finally, genuine skid activation remains the dominant transfer mechanism, but a tightly
capped pre-slide band may leave a barely visible trace when slip, energy and grip are
already high yet the tire has not reached an obvious lock/drift threshold. Because that
trace keeps the TIRE08 pressure bands, a heavily loaded shoulder can become subtly more
visible than the opposite side without reintroducing ordinary-driving graffiti.


## TIRE16D distance LOD and far-history reservation

Dense local skid history must never make valid distant tire marks disappear merely because one global nearest-first triangle budget is exhausted. TIRE16D reserves independent render budgets for near (0–110 m), mid (110–260 m), far (260–430 m), and horizon (430–650 m) history. Near marks preserve the full pressure-resolved lateral profile, mid marks progressively simplify it, and far/horizon marks render as a single full-width quad with pressure detail removed and opacity reduced toward roughly 50% at the horizon. The authoritative FP64 tire-mark history is unchanged; only presentation complexity changes with distance.

TIRE16D also forces the tire-mark source overlay to pure black. The mark remains translucent so the road texture is still visible underneath, but no warm source tint is encoded into the mark itself.

## TIRE16E simplified production LOD and expanded history

Production visual testing favored a leaner, explicit distance ladder: 0–100 m uses six lateral control samples with full contact-pressure/camber structure, 100–200 m uses three samples with simplified structure, 200–300 m collapses to a single solid-width quad, 300–500 m uses the same two-triangle strip under an independently reserved horizon budget, and 500+ m is not rendered. The far bands are deliberately less opaque than close marks so distant history remains readable without appearing unnaturally bold.

The FP64 tire-mark history reservoir doubles from 131,072 to 262,144 segments. Rendering trims each distance band with partial selection before sorting so the larger history does not require sorting the entire visible reservoir every frame. Age does not delete dry marks: visibility now approaches a permanent 62% floor on a two-hour-scale decay, while pressure-band contrast separately softens with age. This keeps old rubber present but visually less defined. Physical removal mechanisms such as rain, traffic abrasion, washing, and resurfacing remain the intended path for stronger removal.
## TIRE16F lean production LOD

The TIRE16E persistence model remains authoritative and FP64. Presentation tessellation is reduced to a 4/2/1/off ladder: 0–100 m uses four lateral pressure/camber control samples, 100–300 m uses two left/right samples, 300–500 m renders one solid-width two-triangle strip with an independently reserved horizon budget, and 500+ m is culled. Two-sample mid-distance strips retain a subtle shoulder-to-shoulder loading gradient but intentionally discard the centre-pressure peak. The change is presentation-only; no FP16 world positioning is introduced.



## TIRE16G visibility continuity and support conformity

The 4/2/1/off LOD remains, but per-band populations are enlarged so independently budgeted near and horizon history cannot leave a missing 100–300 m middle band. Newly generated marks receive a brief render-priority window. Ground-history LOD distance is horizontal and remains FP64-addressed. Tire-mark geometry is no longer lifted several millimetres off the road; polygon-offset depth bias resolves coplanar depth fighting, while abrupt support-normal/height transitions break a trail to avoid floating curb bridges. Lateral marking is additionally gated by actual ground-relative motion so a nearly stationary car dragged sideways down a slope cannot create an opaque drift mark.
