class_name OpalCore
extends Node
# ── opal_core.gd — BUILD 18 ───────────────────────────────────────────────────
# BUILD 18 — THE WISP BAKE, and the recipe for the new material.
#
# OpalFacet BUILD 21 replaces the shard-mosaic fire with a layered gem:
# emissive vein-gas behind sparse fractured glass sheets. Most of that is
# computed live in the facet shader. ONE part cannot be:
#
#   THE WISPS. The curving fire filaments come from line integral convolution
#   — sparse seeds smeared along the streamlines of a curl-noise flow field.
#   That is ~48 noise-marched steps per pixel. Live, per facet, per frame, it
#   is unaffordable; anything cheap enough to run live (anisotropic noise under
#   a rotation field) can only produce evenly spaced PARALLEL ridges, which is
#   the artifact this replaces.
#
#   So it is BAKED. One SubViewport, rendered ONCE at startup, holds the wisp
#   field as a texture. At runtime each facet costs three texture reads — one
#   per colour channel, which also buys chromatic dispersion for free.
#
# The bake is sampled with MIRRORED tiling, so facets can rotate and offset
# their sampling without ever running off the edge of the texture.
#
# Fluid pipeline still byte-identical to BUILD 15.
#
# ── BUILD 17 notes ────────────────────────────────────────────────────────────
# BUILD 17 — GALAXY RECIPE EXPORTS (superseded: the galaxy objects were cut in
# BUILD 21 of the facet; the exports remain but default to off).
#
# ── BUILD 16 notes ────────────────────────────────────────────────────────────
# BUILD 16 — THE SHARD RECIPE. No sim changes; the fluid pipeline is
# byte-identical to BUILD 15. This build only ADDS the global half of the new
# material: a "Recipe — shards" export group, pushed to every facet.
#
# Why it lives here: shard size, join darkness, per-shard relighting, fire
# contrast and sparkle describe the MATERIAL — the same stone everywhere — so
# they belong with the rest of the recipe, not per facet. OpalFacet BUILD 18
# consumes them.
#
# NOTE crystallize now means more than it did: it is the blend between the old
# smooth fire (0) and the full angular shard partition (100). Existing scenes
# saved crystallize=35 as an Inspector override — raise it to ~80 to see the
# new material at all.
#
# ── BUILD 15 notes ────────────────────────────────────────────────────────────
# BUILD 15 — OPEN BOUNDARIES. The domain is no longer a box.
#
# Everything up to here treated the sim edge as a WALL: pressure reflected off
# it (clamp-to-edge sampling is a Neumann/no-flux condition) and velocity was
# zeroed there. So fluid driven at an edge could not leave — it deflected and
# ran along the wall. Push up the left side and it turns right at the top.
#
# Open boundary instead:
#   * pressure is pinned to ZERO one cell in (Dirichlet). "Nothing out there."
#     Flow exits instead of piling up — this also fixes the border eruptions
#     properly, since pressure can no longer accumulate at the edge.
#   * advection samples outside the domain as EMPTY, not as a copy of the edge
#     cell. Clamped sampling made the boundary feed itself, which is what made
#     it behave like a wall.
#   * the velocity taper at the border is gone.
#
# ── BUILD 14 notes ────────────────────────────────────────────────────────────
# BUILD 14 — THE DYE FIELD. This is the piece the original port never had.
#
# Incompressible flow is globally coupled BY DEFINITION: the pressure solve
# enforces zero divergence everywhere at once, which physically means pressure
# propagates instantly across the whole domain. Poke an incompressible fluid
# anywhere and the entire body responds. So a converged solver ALWAYS agitates
# the full square — that is not a bug, it is what incompressibility means, and
# no amount of tuning escapes it.
#
# The way every fluid demo solves this: you do not look at the velocity field.
# You look at DYE — injected at the cursor, carried by the velocity, decaying.
# The velocity stays global and invisible; the dye marks where the fluid IS.
# Outside the dye nothing renders, however energetic the field underneath.
#
# Our port sampled the displacement accumulator directly, i.e. it drew the
# simulation domain itself. Hence: a square.
#
# ── opal_core.gd — BUILD 10 notes ─────────────────────────────────────────────
# BUILD 10 — THE PRESSURE SOLVE. This is the fix for the fluid never having
# felt like the web prototype.
#
# The original port shipped with 2 Jacobi iterations per frame, converging
# ACROSS frames, and that was flagged at the time as a known fidelity gap.
# It is the actual cause of the "ripple of a fixed radius around the cursor":
#
#   The Jacobi solve is what removes DIVERGENCE from the velocity field.
#   Divergence is the radially-expanding part of a splat. Vorticity is the
#   swirling part. A converged solve deletes the first and keeps the second —
#   which is why real fluid cuts and curls instead of pulsing outward.
#
#   Two iterations does not converge; 20-50 is normal. And while the cursor
#   moves it injects fresh divergence every frame, so the solver never catches
#   up. What was on screen was a barely-projected field: a radial pressure
#   blob, i.e. a ring.
#
# So: jacobi_iterations real passes per frame, plus VORTICITY CONFINEMENT —
# one pass that measures curl and pushes energy back into the vortices the
# grid keeps smearing away. That is the standard trick for making a
# stable-fluids sim look crisp rather than soupy.
#
# Pass order is now:  advect -> splat -> vort -> div -> j0..jN -> grad -> off
# BUILD 5 adds three things:
#   1. BODY FORCE — a whole-field acceleration, so turning the stone sloshes
#      what is inside it. Stronger toward the rim, like liquid in a turned bowl.
#      set_body_force() is driven by the menu's rotation velocity.
#   2. STIR MODE — set_cursor_stir(false) for the main menu, where the cursor
#      turns the stone rather than touching it. Submenus flip it back on: you
#      are inside the gem there, so the cursor pushes the fluid directly.
#   3. LIGHT AIM — aim_light() swings the lamp toward a face. Selection makes a
#      facet genuinely the brightest instead of painting a highlight on it.
# BUILD 3: view bias. The menu turns the whole stone toward the selected face
# instead of deforming it. set_view_bias() adds to the mouse tilt, so every
# facet re-lights together — one solid object catching the light differently.
# (Renamed from OpalSim — your project already had an OpalSim and OpalDisplay
#  used by TitleScene. Those are untouched. This is a separate system.)
#
# The stone's interior. One per scene. Draws NOTHING.
# Owns the fluid, the material recipe, and the LIGHT.
#
# BUILD 2 adds cut lighting. Each OpalFacet has a tilt — a surface normal. This
# node owns the single light they are all lit by, so one face can blaze while
# its neighbour goes nearly black. That brightness contrast is what the eye
# reads as three-dimensional form; without it a faceted layout is just stained
# glass. The mouse tilts the whole stone, so highlights sweep across faces as
# you move: turning it in your hand.

const SIM_W := 256
const SIM_H := 144

@export_group("Recipe — material")
@export var fire_scale: float = 7.0
@export_range(0,100) var darkness: float = 100.0
@export_range(0,100) var rest_zones: float = 100.0
@export_range(0,100) var rolling_flash: float = 68.0
@export_range(0,100) var crystal_shards: float = 25.0
@export_range(0,100) var particles: float = 4.0
@export_range(0,100) var fluid_power: float = 14.0
@export_range(0,100) var rock_matrix: float = 0.0
@export var fire_gamma: float = 2.2
## Blend between smooth fire (0) and the angular shard partition (100).
## Existing scenes saved 35 as an override — raise to ~80 for the new material.
@export_range(0,100) var crystallize: float = 80.0
@export_range(0,100) var fire_coverage: float = 60.0
@export_range(0,100) var hue_balance: float = 50.0

@export_group("Recipe — composition")
## How much of the interior is ONE STONE rather than six separate pictures.
## 1.0 samples the gas, wisps and stars in stone space shared by every facet,
## so a formation runs across the whole gem and only breaks where each face
## refracts it differently. 0.0 restores the old behaviour, where every bezel
## had its own unrelated image and the seams read as picture borders.
@export_range(0.0, 1.0) var shared_composition: float = 1.0

@export_group("Recipe — architecture")
## The facet boundaries. A cut stone throws its hardest light along the edges
## where two planes meet — that structure is what the eye reads FIRST, and
## faint grey seams are why the six faces never held together as one object.
@export_range(0.0, 4.0) var seam_glow: float = 0.9
## The thin white-hot line inside the fringe.
@export_range(0.0, 6.0) var seam_core: float = 1.6
## How far the hue sweeps across a seam. An edge splits light rather than
## being a white stripe.
@export_range(0.0, 1.0) var seam_dispersion: float = 0.30
## NOTE — seam ride moved to OpalFacet. It is a per-face decision, not a
## material one: the hub's seams are short and punctuated by vertex flares and
## want no ride at all, while a submenu with one long cut across a single face
## needs it. Putting it here changed the hub every time the submenu was tuned.
@export_range(0.0, 0.4) var vertex_radius: float = 0.10
@export_range(0.0, 4.0) var vertex_gain: float = 1.1

@export_group("Recipe — framing")
## FOCAL FALLOFF. Six faces bleeding to the frame edge at equal intensity give
## the eye nowhere to go. Concentrating light toward the middle of the stone
## and letting the outer reaches fall away is what makes the composition read
## as an object in a space rather than as wallpaper. 0 = off.
@export_range(0.0, 1.0) var focus_falloff: float = 0.55
## Where the light is fullest, in screen-heights from centre.
@export var focus_inner: float = 0.25
## Where it has fallen away.
@export var focus_outer: float = 1.05

@export_group("Recipe — stone lighting")
## THE VALUE HIERARCHY. The lamp is fixed and the cursor turns the stone, so
## faces swing through the light: one blazes while its neighbour falls away.
## Without this every face sits at the same brightness and the image reads as
## texture rather than as a lit object.
## New names on purpose — the legacy light_contrast / diffuse_floor /
## fill_gain carry whatever a scene saved during earlier tuning, and this is
## the term that decides whether there is contrast at all.
@export_range(0.0, 3.0) var face_key: float = 1.0
## Second lamp, roughly opposite. Higher = flatter, every face lit; lower =
## harder, more of the stone falls into shadow. This is the contrast dial.
@export_range(0.0, 1.5) var face_fill: float = 0.28
## How dark an unlit face is allowed to go. Low is dramatic.
@export_range(0.0, 1.0) var face_floor: float = 0.10
## Curve on the falloff. Higher = a harder edge between lit and unlit faces.
@export_range(0.5, 4.0) var face_contrast: float = 1.5

@export_group("Recipe — surface")
## The polished exterior. A hard highlight raking across a face as it turns is
## the strongest "cut gem" cue there is, and it gives the eye one bright,
## simple thing to rest on among all the fire. Ungated by the fire: a
## reflection sits ON the surface and shows brightest where the interior is dark.
@export_range(0.0, 2.0) var surface_specular: float = 0.35
## Tightness of that highlight. Higher = a smaller, harder glint.
@export var surface_sharpness: float = 42.0
## Roughness texture in the highlight, so it is not a pasted-on gradient.
@export var surface_roughness: float = 6.0
## Fresnel rim. A face turned away reflects more of the room — this keeps the
## silhouette of the solid readable.
@export_range(0.0, 0.5) var surface_rim: float = 0.06

@export_group("Recipe — dispersion")
## How far the hue sweeps ACROSS each cleavage edge. A cut edge splits light,
## so an edge should fringe into colour rather than being one white line —
## this is most of what makes a cut stone look edible.
@export_range(0.0, 1.0) var edge_dispersion: float = 0.22
## Strength of those prismatic fringes.
@export_range(0.0, 3.0) var dispersion_gain: float = 1.0

@export_group("Recipe — fluid response")
## VOLUMETRIC FLOW. How differently each depth answers the stroke. The front
## slices follow the instantaneous velocity and snap; the deep slices follow
## the accumulator and drag, so the layers slide over each other and a stroke
## pushes through a volume instead of sliding one flat sheet. 0 = every depth
## moves together, which is the old behaviour.
@export_range(0.0, 4.0) var flow_depth: float = 1.4
## WEIGHT. How far apart the flow is sampled before displacing the interior.
## A stroke should move a MASS of fluid, not vibrate the pixels under the
## cursor — averaging over a spread of points removes the fine, fast
## structure and leaves broad motion with momentum. Larger = heavier and
## slower-looking; near zero returns to local jitter.
@export_range(0.0, 0.2) var flow_smoothing: float = 0.045
## How much more the front of the volume moves than the back. The whole
## volume rides the same smoothed field, so nothing here is instantaneous.
@export_range(0.0, 2.0) var flow_lead: float = 0.35

## NOTE — an optical bend driven by dye or velocity was tried three times and
## reverted three times. Both fields are injected as a blob at the cursor, so
## any per-pixel gradient of them is concentric near the injection point: the
## result is always a ring of distortion following the mouse, and being
## instantaneous it jitters rather than carrying weight.
## Dye as HEAT. Displacement alone is a weak signal — it asks the eye to spot
## a subtle warp in a field it never saw undisturbed. Where the stone has been
## stirred, the fire brightens and climbs in hue instead, the way breathing on
## an ember does, and the shear then reads as a consequence of that.
@export_range(0.0, 5.0) var flow_heat: float = 1.6
## How far the hue climbs where the stone is stirred.
@export_range(-0.5, 0.5) var flow_hue: float = 0.10
## How hard the flow shears the interior. The gas and wisps ride this
## together, so a stroke drags the whole volume rather than one layer.
@export_range(0.0, 12.0) var flow_shear: float = 3.0

@export_group("Recipe — presence")
## How much the interior FORESHORTENS with its face. The stone is a projected
## solid, so a face compresses as it turns away; without this the interior
## keeps a fixed screen scale and the geometry moves while its contents sit
## still. 0 = the old static behaviour.
@export_range(0.0, 1.5) var perspective: float = 1.0
## A dim interior body, so the CUT is readable everywhere instead of only
## where fire happens to be. This is what lets you see the refractions,
## cleavage planes and pane differences across the dark parts of the gem.
@export_range(0.0, 0.3) var body_ambient: float = 0.035
## Edge light floor, so joins stay visible against black.
@export_range(0.0, 0.15) var glint_floor: float = 0.02

@export_group("Recipe — depth of view")
## Index of refraction. The interior is seen THROUGH each facet's surface, and
## that surface bends the line of sight — so every face shows the same stone
## from its own angle, and the view swings as the stone turns. 1.45 is opal;
## 1.0 disables refraction entirely and the interior goes flat.
@export_range(1.0, 2.4) var index_of_refraction: float = 1.45
## How deep the stone reads. This scales how far interior layers slide as you
## look into them at an angle — the single dial for "volume" versus "decal".
@export_range(0.0, 1.5) var interior_depth: float = 0.35
## How much the fracture sheets participate in that depth. The finest sheet
## sits at the surface and barely moves; the coarse ones swim.
@export_range(0.0, 2.0) var glass_depth: float = 1.0
## Where the wisps sit between the surface and the deepest gas.
@export_range(0.0, 1.5) var wisp_depth: float = 0.55

@export_group("Recipe — scale")
## THE ZOOM. Every frequency in this material was calibrated on a render where
## the frame was one facet's worth of material. A bezel occupies only part of
## the screen, so without this it sees a window of that field and everything
## comes out too coarse — no void, one flat hue per face. Raise it if a facet
## looks like one flat colour; lower it if the material looks busy and small.
@export var material_zoom: float = 3.26

@export_group("Recipe — gas")
## Formation size. LOWER = bigger bodies of gas.
@export var gas_scale: float = 2.1
## How hard the domain warp bends the gas into filaments.
@export var gas_warp: float = 2.6
@export var gas_freq: float = 1.0
## Coverage threshold. Higher = less gas, more void.
@export_range(0.0, 0.9) var gas_coverage: float = 0.46
@export var gas_shoulder: float = 0.30
## The island gate — one low-frequency mask shared by every depth slice, so
## the gas forms bounded bodies rather than wall-to-wall fog.
@export var island_freq: float = 0.55
@export_range(0.0, 0.9) var island_coverage: float = 0.44
## How much gas survives outside the islands. 0 = hard-edged islands.
@export_range(0.0, 0.6) var island_floor: float = 0.05
## Depth slices. Each parallaxes at its own rate, which is what puts the gas
## INSIDE the stone. THE MAIN PERFORMANCE DIAL — drop to 3 if the frame rate
## suffers; the look survives it.
## THE MAIN PERFORMANCE DIAL. Each slice re-evaluates the warp, the density
## and the vein field, so this multiplies the most expensive part of the
## material. Left at 5 deliberately — 3 saves roughly a third of the gas cost
## but visibly flattens the depth parallax, which is the point of the slices.
@export_range(2, 8) var gas_slices: int = 5
## How much each slice occludes the ones behind it.
@export var gas_occlusion: float = 1.4
## Hue drift between the front and back slices.
@export var gas_depth_hue: float = 0.03
## Brightness of the gas, NORMALISED against the palette. A neon ramp is
## about 3x brighter than a jewel ramp, so a gain tuned on one is badly wrong
## on the other — that coupling is now divided out in code, and this number
## means the same thing whatever palette is loaded.
@export var gas_gain: float = 24.0

@export_group("Recipe — vein design")
## THE CURRENT. Ridged noise is isotropic — it has no preferred direction, so
## every filament wanders alone and the fire reads as uniform squiggles with no
## design. This advects the vein field along a curl flow before sampling it, so
## the fire STREAMS: filaments stretch along the current, converge where it
## converges, and gather into bands. 0 = the old directionless scribble.
@export_range(0.0, 2.0) var vein_stream: float = 0.55
## Size of the currents. Lower = long sweeping flows across the whole stone;
## higher = tighter local eddies.
@export var vein_flow_scale: float = 0.85
## HIERARCHY. Major arteries are sampled at this fraction of the vein
## frequency — lower means bigger dominant forms.
@export_range(0.1, 1.0) var vein_major_scale: float = 0.38
## How much the capillaries depend on an artery already being there. High
## values give a few strong forms with detail hanging off them; 0 gives
## uniform veins everywhere, which is the pattern with no design.
@export_range(0.0, 1.0) var vein_hierarchy: float = 0.62

@export_group("Recipe — veins")
## Vein frequency. Higher = finer, more branching threads.
@export var vein_freq: float = 3.0
## Crest sharpness. Higher = thinner, harder-edged veins.
@export var vein_sharpness: float = 4.5
## How much of the emission comes from the vein crests vs the soft body.
@export var vein_amount: float = 3.36
## Falloff of the soft body between veins.
@export var body_falloff: float = 2.4
## The dim ambient haze that carries the glow.
@export_range(0.0, 0.5) var haze: float = 0.06
## Fine iridescent grain inside the gas — the opal shimmer.
@export var grain_freq: float = 5.5
## Where a vein knot overexposes toward white.
@export_range(0.0, 1.0) var hot_threshold: float = 0.32
@export_range(0.0, 2.0) var hot_amount: float = 0.80

@export_group("Recipe — colour field")
## Size of the colour zones. LOWER = larger regions of one hue family.
@export var hue_zone_freq: float = 0.55
## How far the zones travel across the palette when the face is at rest.
## This is the IDLE spectrum width — the whole stone runs full opal colour.
@export_range(0.0, 2.0) var hue_spread_idle: float = 1.04
## The narrow window a face collapses to when it is hovered — its identity.
## Idle wide, hover narrow: attention COLLAPSES the spectrum to a signature.
@export_range(0.0, 1.0) var hue_spread_hover: float = 0.18
@export var palette_gamma: float = 1.7

@export_group("Recipe — wisps")
## Brightness of the flow filaments.
@export_range(0.0, 3.0) var wisp_amount: float = 2.16
## Contrast on the wisps. Higher = thinner bright cores.
@export var wisp_power: float = 1.6
## Divides the baked field. Raise if the wisps clip.
@export var wisp_norm: float = 0.045
## Chromatic dispersion — how far the three colour channels separate. This is
## what makes a refracted filament fringe instead of look chopped.
@export var dispersion: float = 0.0016
## Scale of the baked field on screen.
@export var wisp_scale: float = 1.0

@export_group("Wisp bake")
## Resolution of the one-time bake. Higher = finer filaments, more VRAM.
@export var wisp_bake_size: Vector2i = Vector2i(1024, 576)
## LIC march length. More steps = longer filaments. One-time cost only.
@export_range(4, 48) var lic_steps: int = 24
@export var lic_step: float = 0.0042
@export var lic_taper: float = 0.8
## Scale of the curl-noise flow. Lower = broader, calmer streamlines.
@export var flow_scale: float = 1.0
## Seed population. Sparse is essential — a dense field smeared along a flow
## is just streaky fog, not distinct filaments.
@export_range(0.001, 0.2) var seed_density: float = 0.013
@export var seed_freq: float = 30.0
## Seed tightness. Higher = thinner threads.
@export var seed_tight: float = 90.0
## Stored gain, to use the 8-bit range. The facet divides it back out.
@export var bake_gain: float = 8.0

@export_group("Recipe — glass")
## How many cleavage orientations the crystal has. 2 gives rhombi, 3 gives
## the hexagonal/triangular tiling of a real cleaved stone, 4 is busier and
## more kaleidoscopic. This is the shape language of the shards.
@export_range(2, 4) var cleavage_families: int = 3

## Global multiplier on every sheet's cell size. Lower = bigger shards.
@export var glass_scale: float = 0.25
## How many of the five fracture sheets are drawn. THE SECOND PERFORMANCE
## DIAL — each sheet is a Voronoi lookup. 3 still reads as layered glass.
## THE SECOND DIAL. Each sheet is a cleavage lookup per pixel. 3 still reads
## as layered glass and is the cheaper thing to give up if the material ever
## has to be cut down — the top two sheets are fine crazing, not composition.
@export_range(1, 5) var glass_layers: int = 5
## How complete each sheet is. Lower = sparser, more of the gas unbroken.
@export_range(0.1, 1.0) var glass_presence: float = 1.0
## Refraction strength for the gas.
## The PRIMARY interior cue now. A plane inside a crystal is visible because
## the image behind it steps sideways as you cross it — nothing else.
@export var refraction: float = 1.8
## Refraction applied to the WISPS, from the coarse sheets only. Fine shards
## displace a thin filament by less than its own width in random directions,
## which minces it; big panes shift whole runs sideways, which is what glass
## actually does. Keep this low — high values sever the filaments.
@export_range(0.0, 1.5) var wisp_refraction: float = 0.45
## Edge light along the joins.
@export_range(0.0, 2.0) var glass_glint: float = 0.08
## How often a whole pane catches the light — the visible "layer" of the gem.
@export_range(0.0, 1.5) var pane_flash: float = 0.30
## How far each pane's own normal tilts. This is what makes panes catch the
## lamp differently from their neighbours.
## LEGACY, off by default. This lights each pane with dot(normal, lamp) — a
## SURFACE shading model, which makes shards read as bumps on the outside of
## the stone rather than planes inside it. Raise only if you deliberately want
## faceted relief on the outer face.
@export_range(0.0, 2.0) var pane_tilt: float = 0.0
## Brightness gradient ACROSS each pane's face. A facet is a plane, so it
## should brighten toward one edge — flat panes read as tinted film.
@export_range(0.0, 1.5) var pane_ramp: float = 0.0
## How fast per-pane brightness variation DIES with sheet depth. A fine shard
## swinging 0.25..2.4 in brightness is a fleck, and five layers of flecks
## multiply into noise — that is the speckle overlay. Coarse panes should
## carry the variation; fine ones should only whisper. 1.0 = no damping.
## How cloudy panes get, at random. This is ABSORPTION — a fracture plane
## inside a stone dims what passes through it slightly, and has no idea where
## the lamp is. This is what makes the shards read as interior.
@export_range(0.0, 1.0) var pane_absorb: float = 0.30
@export_range(0.2, 1.0) var pane_depth_damp: float = 0.392
## How much the fracture sheets shift as the stone turns. 0 pins them, which
## reads as a static overlay; higher makes the crystal feel like a solid the
## light moves through.
@export var glass_parallax: float = 0.6

@export_group("Recipe — stars")
@export_range(0.0, 3.0) var star_amount: float = 3.0

@export_group("Recipe — tone")
## These four deliberately have NEW names. The old exposure / saturation /
## shoulder / output_gamma keep whatever a scene saved during earlier tuning,
## and they sit at the very END of the chain where a stale value turns a
## controlled image into neon. A new name cannot carry a stale override.
@export var stone_gamma: float = 0.4545
@export var stone_exposure: float = 0.68
## 1.55 was tuned for the old mosaic. The gas is already saturated by its
## palette; pushing it again is most of the stained-glass look.
@export_range(0.5, 2.0) var stone_saturation: float = 1.58
@export_range(0.4, 1.0) var stone_shoulder: float = 0.62
## Deepens the mid-tones so the stone is black OR blazing, never murky.
@export_range(0.0, 1.5) var tonal_curve: float = 1.50

@export_group("Recipe — depth")
@export_range(0,100) var layer_occlusion: float = 65.0
@export_range(0,100) var sheen: float = 0.0
@export_range(0,100) var dome: float = 0.0
@export_range(0,100) var rim_dispersion: float = 0.0
@export var parallax_gain: float = 2.5

@export_group("Cut lighting")
## Where the light sits. 145 / 44 puts it upper-left, like a jeweller's lamp.
@export_range(0.0, 360.0) var light_azimuth: float = 145.0
@export_range(0.0, 90.0) var light_elevation: float = 44.0
## The lamp is FIXED by default: the cursor turns the stone, and faces catch or
## lose the light as they swing through it. That is how a gem actually flashes.
@export var light_follows_cursor: bool = false
## A second lamp, roughly opposite the key. One fixed light leaves every face
## on the far side permanently dark, and +/-18 degrees of rotation is not
## enough to swing them through it — so the light could never "catch
## everywhere". Real stones are lit from more than one direction.
@export_range(0.0, 360.0) var fill_azimuth: float = 330.0
@export_range(0.0, 90.0) var fill_elevation: float = 26.0
@export_range(0.0, 1.5) var fill_gain: float = 0.60
## How far the mouse tilts the whole stone. 0 = light sweeps only when facets move.
@export var tilt_response: float = 0.9
@export var specular_power: float = 30.0
@export var specular_gain: float = 0.45
## Floor stops unlit faces going pure black. Lower = harsher cut.
@export_range(0.0, 1.0) var diffuse_floor: float = 0.22
@export var diffuse_gain: float = 1.0
## How much a steeply tilted face shifts its fire as you move. The depth cue.
@export var tilt_parallax: float = 1.6
## Contrast curve on lighting. 1.0 = flat multiply (washes out through the
## gamma encode). ~2.2 cancels that encode so face-to-face contrast survives.
@export var light_contrast: float = 2.2
## How much an unlit face loses FIRE rather than brightness. This is what keeps
## dark faces saturated instead of grey.
@export_range(0.0, 0.3) var light_coverage: float = 0.12
## How fast the stone turns when the selection changes.
@export var view_bias_lerp: float = 0.10
## How fast the lamp swings toward a newly selected face.
@export var light_aim_lerp: float = 0.16
## Rotation velocity to fluid force. 0 = turning does not stir.
## UNIFORM translation of the whole velocity field. Keep this at or near zero:
## translating every cell equally does not slosh anything, it slides the entire
## fluid as one rigid block, which reads as the gem shaking and completely
## drowns out the local cursor stir.
@export var slosh_gain: float = 0.0
## Rigid rotation of the WHOLE field. Keep at 0.
## TRIED AND REVERTED TWICE: driving this from the stone's turn to get slosh
## produces exactly the artifact described below — a swirl that builds in the
## centre and biases every trail, because a rigid rotation is applied to every
## cell at once and has nowhere local to act. Inertia has to come from the
## MATERIAL lagging the view, not from the solver.
## Because the cursor also drives rotation,
## any value here injects a global curl with a handedness set by your movement
## direction — which is a systematic curve bias on every trail, applied
## everywhere at once. Keep at 0 unless you specifically want that.
@export var swirl_gain: float = 0.0
@export var slosh_decay: float = 0.88

@export_group("Color")
## Use the built-in jewel ramp even when a palette is assigned in the scene.
## A scene-assigned GradientTexture1D otherwise wins silently, which is easy
## to miss: the material then runs on whatever ramp was authored months ago.
## Set false once you have a palette you want to art-direct with.
@export var use_builtin_palette: bool = true
@export var palette: GradientTexture1D
@export var output_gamma: float = 0.4545
## Applied after the gamma encode, to recover what additive layering,
## particles and specular take out. 1.0 = untouched.
@export var saturation: float = 1.55
@export var exposure: float = 1.06
## Where highlight compression begins. Lower = more headroom, less clipping.
@export_range(0.5, 1.0) var shoulder: float = 0.66
## How much extra FIRE a selected face gets. Not brightness — the light does that.
@export_range(0.0, 0.25) var wake_fire: float = 0.10

@export_group("Selection flair")
## How rare the identity colour is on any face. 0.87 = about 13% of regions.
@export_range(0.5, 0.97) var rare_threshold: float = 0.87
## How far the selected face's whole palette leans toward its identity colour.
## Continuous, so it cannot pop the way a threshold does.
@export_range(0.0, 1.0) var wake_hue_shift: float = 0.80
## How much brighter the selected face's edges catch the light.
@export_range(0.0, 2.5) var flare_gain: float = 0.9
## Brightness of the catch of light on selection.
@export_range(0.0, 3.0) var pulse_gain: float = 0.85

@export_group("State")
@export_range(0.0, 1.0) var hardened: float = 0.0
@export var hard_hue_shift: float = 61.0
@export var focus_lerp: float = 0.12
@export var hard_lerp: float = 0.05

@export_group("Oil film")
## A thin iridescent film ON the surface of each facet — separate from the fire
## inside the stone. Driven by the fluid, so it is the layer that actually
## reads as liquid. 0 disables it entirely.
@export_range(0.0, 2.0) var film_amount: float = 0.75
## How far from the cursor the film stays awake. Screen-heights.
@export var film_radius: float = 0.22
## How much moving fluid wakes the film on its own, away from the cursor.
@export var film_flow_wake: float = 0.9
@export var film_scale: float = 2.6
## Interference banding. More cycles = tighter rainbow.
@export var film_cycles: float = 1.5
## How much the film's own thickness bends its highlights.
@export var film_bump: float = 0.075
@export var film_spec_power: float = 34.0
@export var film_spec_gain: float = 1.0
## Dye concentration to film thickness. This is the main "how much colour does
## a stroke have" dial now.
@export var film_thickness: float = 3.4
## Force the cursor puts into the fluid. Higher = the stroke shoves harder.
@export var stir_force: float = 1.0
## The speed a stroke is measured against. Movement near this reads as a
## normal stir; well under it is a gentle curl, well over it is a flick.
@export var stir_reference: float = 0.9
## Curve on the response. 1 = linear. Below 1 makes slow movement more
## expressive; above 1 reserves the strong reaction for genuine flicks.
@export_range(0.3, 3.0) var stir_response: float = 1.35
## Soft ceiling. The old hard clamp made every stroke above walking pace
## identical — this compresses instead, so harder always reads as harder.
@export var stir_ceiling: float = 3.2
## THE WAKE. A moving object drags fluid past itself and leaves a shear layer
## running forward on one side of its path and back on the other. That layer
## is unstable and the solver rolls it into counter-rotating vortices, so a
## stroke leaves curls that outlive it instead of a puff that fades. 0 gives
## the old uniform jet.
@export_range(0.0, 6.0) var stir_curl: float = 2.2
## How much a fast stroke widens its trail. Slow strokes stay fine threads.
@export_range(0.0, 4.0) var stir_spread: float = 1.4
## How much the disturbance LAGS the cursor. 1.0 = rigidly attached (an
## emitter). Lower values give the contact point mass, so the fluid reacts to
## where the probe is rather than where your hand is — which is most of what
## separates "something pushing through" from "something emitting".
@export_range(0.06, 1.0) var stir_lag: float = 0.12
## Size of the contact patch. Broad reads as a field; tight reads as an object.
@export var stir_radius: float = 0.0022
## How much of the film comes from VELOCITY rather than the slow accumulator.
## Velocity responds the same frame you move — this is the responsiveness dial.
@export var film_velocity: float = 1.5
## Vorticity. The pressure solve already produces eddies; this surfaces them.
## Swirl is what makes a film read as oil rather than as tint.
@export var film_curl: float = 0.9
## The old full-screen flow tint — colour that follows the fluid directly.
@export var flow_tint: float = 0.75

@export_group("Fluid solver")
## Pressure iterations PER FRAME. 2 was the old value and it never converged —
## that is what produced a ring instead of a swirl. 20 is a normal figure.
## Read once at startup; changing it needs a restart.
## Pressure iterations per frame. Cheap individually — the sim is 256x144 —
## but 16 passes is more convergence than a menu needs, and each is still a
## render target switch.
@export_range(2, 40) var jacobi_iterations: int = 16
## Vorticity confinement. Grid fluids numerically smear their own vortices
## away; this measures curl and pushes the energy back in. 0 disables.
@export_range(0.0, 1.0) var vorticity: float = 0.08
## Below this curl-gradient level, confinement does nothing. Without a gate it
## normalises numerical noise into random forces — which reads as grain.
@export var vorticity_gate: float = 0.02
## Keeps converged pressure inside the fixed-point range. Lower if the field
## ever erupts from the borders; higher is slightly more accurate.
@export_range(0.05, 1.0) var pressure_scale: float = 0.30
@export var max_speed: float = 3.0
## Width of the boundary taper, in UV. Must be WIDE and smooth — a narrow one
## draws a visible rectangle around the screen.
## 0 = fully open, flow leaves the screen. Only raise this if you want the
## fluid held in.
@export_range(0.0, 0.35) var border_width: float = 0.0
## How fast the velocity field dies. 0.995 was ~3.3 seconds, so everything you
## did recently was still on screen and summed. 0.965 is about half a second.
@export_range(0.90, 0.999) var velocity_decay: float = 0.982
## How fast the displacement wake dies. This is the one that made the whole
## viewport read as a persistent deforming sheet.
@export_range(0.85, 0.999) var offset_decay: float = 0.965

@export_group("Dye")
## How long a stroke stays visible. 0.985 is about a second.
@export_range(0.90, 0.999) var dye_decay: float = 0.984
## How much dye each stroke lays down.
@export var dye_inject: float = 0.85
## Width of the stroke.
@export var dye_radius: float = 0.0065

@export_group("Rest behaviour")
## Below this local flow speed the stone is CLEAR — no displacement at all.
@export var flow_gate_lo: float = 0.06
## At and above this it distorts fully.
@export var flow_gate_hi: float = 0.55
## How much distortion remains in perfectly still fluid. 0 = glass-clear.
@export_range(0.0, 1.0) var flow_at_rest: float = 0.0
## Fades the fluid's influence out before it reaches the screen edge, so the
## sim's rectangular domain never becomes visible as a frame.
@export_range(0.0, 0.3) var edge_fade: float = 0.0
## Multiplies dye before it is gated. Higher = a stroke reads for longer.
@export var dye_gain: float = 1.9

@export_group("Behavior")
@export var cursor_drives_fluid: bool = true
@export var ambient_currents: bool = false

var _vp := {}
var _mat := {}
var _facets: Array[OpalFacet] = []
var _jacobi: Array[String] = []

var _last_mouse := Vector2.INF
var _last_t := 0.0
var _pending_force := Vector2.ZERO
var _pending_pos := Vector2(0.5, 0.5)
var _pending_n := 0
var _view := Vector2.ZERO
var _view_t := Vector2.ZERO
var _bias := Vector2.ZERO
var _bias_t := Vector2.ZERO
var _body := Vector2.ZERO
var _swirl := 0.0
var _mouse_uv := Vector2(0.5, 0.5)
var _seg_a := Vector2(0.5, 0.5)
var _seg_b := Vector2(0.5, 0.5)
var _probe := Vector2(0.5, 0.5)
var _probe_prev := Vector2(0.5, 0.5)
var _probe_init := false
var _stroke_speed := 0.0
var _stroke_dir := Vector2.ZERO
var _light := Vector3(0, 0, 1)
var _light_t := Vector3(0, 0, 1)
var _light_ready := false
var _t := 0.0
var _t0 := 0.0
var _boot := 6

var _f_hue := 19.0
var _f_hue_t := 19.0
var _f_spread := 100.0
var _f_spread_t := 100.0
var _f_amt := 0.0
var _f_amt_t := 0.0
var _hard := 0.0
var _hard_t := 0.0
var _pal_norm := 0.132

const SH_COMMON := """
vec2 pk(float v){
	float n = clamp(v * 0.125 + 0.5, 0.0, 1.0);
	float hi = floor(n * 255.0) / 255.0;
	return vec2(hi, clamp(fract(n * 255.0), 0.0, 1.0));
}
float upk(vec2 c){ return ((c.x + c.y / 255.0) - 0.5) * 8.0; }
vec2 dec2(vec4 t){ return vec2(upk(t.rg), upk(t.ba)); }
vec4 enc2(vec2 v){ vec2 a = pk(v.x); vec2 b = pk(v.y); return vec4(a.x, a.y, b.x, b.y); }
float dec1(vec4 t){ return upk(t.rg); }
vec4 enc1(float v){ vec2 a = pk(v); return vec4(a.x, a.y, 0.0, 1.0); }
vec2 bl2(sampler2D s, vec2 uv, vec2 tx){
	vec2 g = uv / tx - 0.5;
	vec2 f = fract(g);
	vec2 base = (floor(g) + 0.5) * tx;
	vec2 A = dec2(texture(s, base));
	vec2 B = dec2(texture(s, base + vec2(tx.x, 0.0)));
	vec2 C = dec2(texture(s, base + vec2(0.0, tx.y)));
	vec2 D = dec2(texture(s, base + tx));
	return mix(mix(A, B, f.x), mix(C, D, f.x), f.y);
}
"""

const SH_ADVECT := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform float uDt;
uniform float uDis;
uniform vec2 uTx;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	vec2 vel = dec2(texture(uV, UV));
	vec2 c = UV - uDt * vel;
	// outside the domain is EMPTY, not a mirror of the edge cell
	if (c.x < 0.0 || c.x > 1.0 || c.y < 0.0 || c.y > 1.0){
		COLOR = enc2(vec2(0.0));
	} else {
		COLOR = enc2(bl2(uV, c, uTx) * uDis);
	}
	}
}
"""

const SH_SPLAT := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uS : filter_nearest, repeat_disable;
uniform vec2 uP;
uniform vec2 uP1;
uniform vec2 uF;
uniform vec2 uBody = vec2(0.0);
uniform float uSwirl = 0.0;
uniform float uShear = 0.0;
uniform vec2 uDir = vec2(0.0);
uniform float uR;
uniform float uAsp;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	// capsule, not a point: a fast flick lays down a continuous stroke
	// instead of one isolated blob per frame
	vec2 seg = uP1 - uP;
	float sl = max(dot(seg, seg), 1e-7);
	float ts = clamp(dot(UV - uP, seg) / sl, 0.0, 1.0);
	vec2 cp = uP + seg * ts;
	vec2 d = UV - cp;
	d.x *= uAsp;
	float g = exp(-dot(d,d)/uR);
	// Divergence-free forces only — anything else is deleted by the pressure
	// solve. Uniform translation and rigid rotation both survive it.
	vec2 r = UV - vec2(0.5);
	r.x *= uAsp;
	vec2 bf = uBody + uSwirl * vec2(-r.y, r.x);
	// ── THE WAKE ──
	// A uniform force over a Gaussian is a JET: a blob of fluid shoved one
	// way, identical every time. A real object dragged through fluid pushes
	// past itself and leaves a SHEAR LAYER — flow running forward on one side
	// of its path and back on the other. That layer is unstable, so the
	// solver's own vorticity rolls it into a train of counter-rotating
	// vortices. Same input energy, completely different behaviour: curls and
	// eddies that outlive the stroke instead of a puff that fades.
	vec2 dirn = uDir;
	if (dot(dirn, dirn) > 1e-6){
		dirn = normalize(dirn);
		vec2 perp = vec2(-dirn.y, dirn.x);
		float across = dot(d, perp) / max(sqrt(uR), 1e-4);
		vec2 shearF = dirn * -clamp(across, -1.0, 1.0) * uShear * g;
		COLOR = enc2(dec2(texture(uS, UV)) + uF * g + shearF + bf);
	} else {
		COLOR = enc2(dec2(texture(uS, UV)) + uF * g + bf);
	}
	}
}
"""

const SH_VORT := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform vec2 uTx;
uniform float uEps;
uniform float uVortGate = 0.02;
uniform float uReset = 1.0;
%s
float curlAt(sampler2D s, vec2 uv, vec2 tx){
	vec2 r = dec2(texture(s, uv + vec2(tx.x, 0.0)));
	vec2 l = dec2(texture(s, uv - vec2(tx.x, 0.0)));
	vec2 u = dec2(texture(s, uv + vec2(0.0, tx.y)));
	vec2 d = dec2(texture(s, uv - vec2(0.0, tx.y)));
	return 0.5 * ((r.y - l.y) - (u.x - d.x));
}
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	vec2 v = dec2(texture(uV, UV));
	if (uEps < 0.0001){ COLOR = enc2(v); } else {
	float c = curlAt(uV, UV, uTx);
	float cr = abs(curlAt(uV, UV + vec2(uTx.x, 0.0), uTx));
	float cl = abs(curlAt(uV, UV - vec2(uTx.x, 0.0), uTx));
	float cu = abs(curlAt(uV, UV + vec2(0.0, uTx.y), uTx));
	float cd = abs(curlAt(uV, UV - vec2(0.0, uTx.y), uTx));
	// gradient of |curl| points toward the centre of each vortex
	vec2 g = vec2(cr - cl, cu - cd) * 0.5;
	float gl = length(g);
	// GATE. Where curl is near zero this gradient is numerical noise, and
	// normalising noise yields random unit vectors — so ungated confinement
	// injects a random force into every quiet cell, which is exactly what
	// grain looks like. Only confine where there is a real vortex to confine.
	float w = smoothstep(uVortGate, uVortGate * 3.5, gl) * smoothstep(uVortGate, uVortGate * 3.0, abs(c));
	vec2 n = g / max(gl, 1e-5);
	vec2 f = uEps * w * vec2(n.y * c, -n.x * c);
	COLOR = enc2(v + clamp(f, vec2(-0.35), vec2(0.35)));
	}}
}
"""

const SH_DIV := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform vec2 uTx;
uniform float uPScale = 0.3;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc1(0.0); } else {
	float l = dec2(texture(uV, UV - vec2(uTx.x,0.0))).x;
	float r = dec2(texture(uV, UV + vec2(uTx.x,0.0))).x;
	float b = dec2(texture(uV, UV - vec2(0.0,uTx.y))).y;
	float t = dec2(texture(uV, UV + vec2(0.0,uTx.y))).y;
	// Scaled DOWN here and back UP in the gradient pass. Converged pressure
	// overshoots the +/-4 fixed-point range; a clamped pressure gives a wrong
	// gradient, and the error piles up at the borders until it lets go.
	COLOR = enc1(0.5 * (r - l + t - b) * uPScale);
	}
}
"""

const SH_JACOBI := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uP : filter_nearest, repeat_disable;
uniform sampler2D uD : filter_nearest, repeat_disable;
uniform vec2 uTx;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc1(0.0); } else {
	// OPEN BOUNDARY: pressure pinned to zero at the edge. Reflecting it here
	// is what turned the domain into a box.
	if (UV.x < uTx.x || UV.x > 1.0 - uTx.x || UV.y < uTx.y || UV.y > 1.0 - uTx.y){
		COLOR = enc1(0.0);
	} else {
	float l = dec1(texture(uP, UV - vec2(uTx.x,0.0)));
	float r = dec1(texture(uP, UV + vec2(uTx.x,0.0)));
	float b = dec1(texture(uP, UV - vec2(0.0,uTx.y)));
	float t = dec1(texture(uP, UV + vec2(0.0,uTx.y)));
	float d = dec1(texture(uD, UV));
	COLOR = enc1((l + r + b + t - d) * 0.25);
	}}
}
"""

const SH_GRAD := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uP : filter_nearest, repeat_disable;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform vec2 uTx;
uniform float uPScale = 0.3;
uniform float uVMax = 3.0;
uniform float uBorder = 0.10;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	float l = dec1(texture(uP, UV - vec2(uTx.x,0.0)));
	float r = dec1(texture(uP, UV + vec2(uTx.x,0.0)));
	float b = dec1(texture(uP, UV - vec2(0.0,uTx.y)));
	float t = dec1(texture(uP, UV + vec2(0.0,uTx.y)));
	vec2 vel = dec2(texture(uV, UV));
	vel -= 0.5 * vec2(r - l, t - b) / max(uPScale, 0.01);

	// No wall. Pressure is pinned to zero at the boundary in the Jacobi pass,
	// which is the correct open condition — flow simply leaves. uBorder is
	// kept only as an optional soft taper; 0 means fully open.
	if (uBorder > 0.001){
		vec2 bd = min(UV, vec2(1.0) - UV);
		vel *= smoothstep(0.0, uBorder, min(bd.x, bd.y));
	}

	// hard speed ceiling: nothing here should ever run away
	float sp = length(vel);
	if (sp > uVMax) vel *= uVMax / sp;

	COLOR = enc2(vel);
	}
}
"""

const SH_OFFSET := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uO : filter_nearest, repeat_disable;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform float uDt;
uniform float uDecay = 0.94;
uniform vec2 uTx;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	vec2 vel = dec2(texture(uV, UV));
	vec2 oc = UV - uDt * vel;
	vec2 prev = vec2(0.0);
	if (oc.x >= 0.0 && oc.x <= 1.0 && oc.y >= 0.0 && oc.y <= 1.0){
		prev = bl2(uO, oc, uTx);
	}
	COLOR = enc2(prev * uDecay + vel * uDt);
	}
}
"""

const SH_DYE := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uD : filter_nearest, repeat_disable;
uniform sampler2D uV : filter_nearest, repeat_disable;
uniform vec2 uP;
uniform vec2 uP1;
uniform float uR;
uniform float uAsp;
uniform float uInject;
uniform float uDecay;
uniform float uDt;
uniform vec2 uTx;
uniform float uReset = 1.0;
%s
float bl1(sampler2D s, vec2 uv, vec2 tx){
	vec2 g = uv / tx - 0.5;
	vec2 f = fract(g);
	vec2 b = (floor(g) + 0.5) * tx;
	float A = dec1(texture(s, b));
	float B = dec1(texture(s, b + vec2(tx.x, 0.0)));
	float C = dec1(texture(s, b + vec2(0.0, tx.y)));
	float D = dec1(texture(s, b + tx));
	return mix(mix(A, B, f.x), mix(C, D, f.x), f.y);
}
void fragment(){
	if (uReset > 0.5){ COLOR = enc1(0.0); } else {
	vec2 vel = dec2(texture(uV, UV));
	// carried by the flow, and fading
	vec2 bc = UV - uDt * vel;
	float d = 0.0;
	if (bc.x >= 0.0 && bc.x <= 1.0 && bc.y >= 0.0 && bc.y <= 1.0){
		d = bl1(uD, bc, uTx) * uDecay;
	}
	// injected along the cursor's path this frame
	vec2 seg = uP1 - uP;
	float sl = max(dot(seg, seg), 1e-7);
	float ts = clamp(dot(UV - uP, seg) / sl, 0.0, 1.0);
	vec2 cp = uP + seg * ts;
	vec2 dd = UV - cp;
	dd.x *= uAsp;
	d += exp(-dot(dd,dd) / uR) * uInject;
	COLOR = enc1(clamp(d, 0.0, 1.0));
	}
}
"""

const SH_WISP := """
shader_type canvas_item;
render_mode blend_disabled;
uniform int uSteps = 24;
uniform float uStep = 0.0042;
uniform float uTaper = 0.8;
uniform float uFlow = 1.0;
uniform float uSeedDens = 0.013;
uniform float uSeedFreq = 30.0;
uniform float uSeedTight = 90.0;
uniform float uGain = 8.0;
uniform float uAsp = 1.7778;

// Hoskins hashes — a sin-based hash loses precision on D3D12 and bands.
float h1(vec2 p){
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}
vec2 h2(vec2 p){
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}
float nz(vec2 p){
	vec2 i = floor(p); vec2 f = fract(p); f = f*f*(3.0-2.0*f);
	return mix(mix(h1(i),h1(i+vec2(1,0)),f.x), mix(h1(i+vec2(0,1)),h1(i+vec2(1,1)),f.x), f.y);
}
float fb(vec2 p){
	float v = 0.0; float a = 0.5;
	for(int i=0;i<4;i++){ v += a*nz(p); p = p*2.05 + vec2(11.3,7.9); a *= 0.5; }
	return v;
}
// scalar potential; the flow is its perpendicular gradient, so the field is
// divergence-free and streamlines never terminate — they curve and braid
float psi(vec2 p){
	return nz(p * uFlow) + 0.5 * nz(p * uFlow * 2.1 + vec2(3.0, 8.0));
}
vec2 curl(vec2 p){
	float e = 0.02;
	float dx = (psi(p + vec2(e,0.0)) - psi(p - vec2(e,0.0))) / (2.0*e);
	float dy = (psi(p + vec2(0.0,e)) - psi(p - vec2(0.0,e))) / (2.0*e);
	vec2 v = vec2(dy, -dx);
	return v / max(length(v), 1e-6);
}
// sparse seeds, clustered into corridors so that neighbouring seeds do not
// all fall into the same curl vortex (that is what made concentric arcs)
float seeds(vec2 p){
	vec2 g = p * uSeedFreq;
	vec2 i = floor(g); vec2 f = fract(g);
	vec2 hh = h2(i);
	float corr = fb(i / uSeedFreq * 0.9 + 17.0);
	float dens = uSeedDens * clamp((corr - 0.30) / 0.16, 0.15, 2.2);
	if (h1(i + 5.3) > dens) return 0.0;
	vec2 d = f - hh;
	return exp(-dot(d,d) * uSeedTight);
}
void fragment(){
	vec2 p = UV * vec2(uAsp, 1.0);
	float acc = 0.0;
	float wsum = 0.0;
	// march forward and backward along the streamline through this pixel
	for (int s = 0; s < 2; s++){
		float sgn = (s == 0) ? 1.0 : -1.0;
		vec2 c = p;
		for (int k = 0; k < uSteps; k++){
			c += curl(c) * sgn * uStep;
			float w = pow(1.0 - float(k) / float(uSteps), uTaper);
			acc += seeds(c) * w;
			wsum += w;
		}
	}
	float v = acc / max(wsum, 1e-6) * uGain;
	COLOR = vec4(vec3(clamp(v, 0.0, 1.0)), 1.0);
}
"""

const SH_COPY := """
shader_type canvas_item;
render_mode blend_disabled;
uniform sampler2D uS : filter_nearest, repeat_disable;
uniform float uReset = 1.0;
%s
void fragment(){
	if (uReset > 0.5){ COLOR = enc2(vec2(0.0)); } else {
	COLOR = texture(uS, UV);
	}
}
"""


func _ready() -> void:
	print(">>> opal_core BUILD 18")
	_t0 = Time.get_ticks_msec() / 1000.0
	if use_builtin_palette or palette == null:
		palette = _default_palette()
	_pal_norm = _palette_norm()
	_hard = hardened
	_hard_t = hardened
	print("    palette=%s  norm=%.3f  gas_gain=%.2f -> effective %.1f" % [
		"builtin jewel" if use_builtin_palette else "scene-assigned",
		_pal_norm, gas_gain, gas_gain / _pal_norm])
	print("    tone: gamma=%.4f exposure=%.2f saturation=%.2f shoulder=%.2f zoom=%.2f" % [
		stone_gamma, stone_exposure, stone_saturation, stone_shoulder, material_zoom])
	_build_pipeline()


# ── Public API ────────────────────────────────────────────────────────────────
func stir(uv: Vector2, vel: Vector2) -> void:
	stir_segment(uv, uv, vel)


## Stir along a path rather than at a point, so fast movement is continuous.
func stir_segment(a: Vector2, b: Vector2, vel: Vector2) -> void:
	if _pending_n == 0:
		_seg_a = a
		_seg_b = b
	else:
		_seg_a = _seg_a.lerp(a, 0.5)
		_seg_b = b
	_pending_pos = (_pending_pos * _pending_n + b) / float(_pending_n + 1)
	_pending_force += vel
	_pending_n += 1


func set_focus(hue: float, spread: float, amount: float) -> void:
	_f_hue_t = fposmod(hue, 100.0)
	_f_spread_t = clampf(spread, 0.0, 100.0)
	_f_amt_t = clampf(amount, 0.0, 1.0)


func set_hard(amount: float) -> void:
	_hard_t = clampf(amount, 0.0, 1.0)


func get_hard() -> float:
	return _hard


func get_view() -> Vector2:
	return _view + _bias


## Turn the stone. The menu calls this so the chosen face catches the light.
func set_view_bias(v: Vector2) -> void:
	_bias_t = v


func get_time() -> float:
	return _t


func mouse_uv() -> Vector2:
	return _mouse_uv


func focus_hue() -> float:
	return _f_hue


func focus_spread() -> float:
	return _f_spread


func focus_amount() -> float:
	return _f_amt


func dye_tex() -> Texture2D:
	return _vp["dye1"].get_texture() if _vp.has("dye1") else null


func flow_tex() -> Texture2D:
	return _vp["off1"].get_texture() if _vp.has("off1") else null


func vel_tex() -> Texture2D:
	return _vp["grad"].get_texture() if _vp.has("grad") else null


func sim_texel() -> Vector2:
	return Vector2(1.0 / SIM_W, 1.0 / SIM_H)


func register_facet(f: OpalFacet) -> void:
	if not _facets.has(f):
		_facets.append(f)


func unregister_facet(f: OpalFacet) -> void:
	_facets.erase(f)


func base_light() -> Vector3:
	var az := deg_to_rad(light_azimuth)
	var el := deg_to_rad(light_elevation)
	var c := cos(el)
	return Vector3(cos(az) * c, sin(az) * c, sin(el)).normalized()


func light_vector() -> Vector3:
	return _light


func fill_vector() -> Vector3:
	var az := deg_to_rad(fill_azimuth)
	var el := deg_to_rad(fill_elevation)
	var c := cos(el)
	return Vector3(cos(az) * c, sin(az) * c, sin(el)).normalized()


## Swing the lamp toward a face — in AZIMUTH ONLY, holding elevation.
## Pointing the lamp straight at a normal aims it at the camera, which lights
## every face at once; keeping elevation fixed moves which face is lit instead
## of adding light to all of them.
func aim_light(n: Vector3, amount: float) -> void:
	var b := base_light()
	var flat := Vector2(n.x, n.y)
	if flat.length() < 0.02:
		_light_t = b
	else:
		flat = flat.normalized()
		var b_az := atan2(b.y, b.x)
		var t_az := atan2(flat.y, flat.x)
		var d := fposmod(t_az - b_az + PI, TAU) - PI
		var az := b_az + d * clampf(amount, 0.0, 1.0)
		var el := deg_to_rad(light_elevation)
		var c := cos(el)
		_light_t = Vector3(cos(az) * c, sin(az) * c, sin(el)).normalized()
	if not _light_ready:
		_light = _light_t
		_light_ready = true


func set_swirl(s: float) -> void:
	_swirl += s


## Point the lamp in an explicit direction. Used to track the cursor, so the
## highlight slides continuously across the stone rather than jumping between
## faces on selection.
func point_light(dir: Vector3) -> void:
	if dir.length() < 0.01:
		return
	_light_t = dir.normalized()
	if not _light_ready:
		_light = _light_t
		_light_ready = true


## Turning the stone sloshes what is inside it.
func set_body_force(f: Vector2) -> void:
	_body += f


## Main menu: cursor turns the stone. Submenu: cursor touches the fluid.
func set_cursor_stir(on: bool) -> void:
	cursor_drives_fluid = on
	if not on:
		_last_mouse = Vector2.INF


func push_globals(m: ShaderMaterial) -> void:
	m.set_shader_parameter("uPal", palette)
	m.set_shader_parameter("uScale", fire_scale)
	m.set_shader_parameter("uDark", darkness)
	m.set_shader_parameter("uRest", rest_zones)
	m.set_shader_parameter("uRoll", rolling_flash)
	m.set_shader_parameter("uShard", crystal_shards)
	m.set_shader_parameter("uPart", particles)
	m.set_shader_parameter("uFlu", fluid_power / 30.0)
	m.set_shader_parameter("uRockAmt", rock_matrix)
	m.set_shader_parameter("uGamma", fire_gamma)
	m.set_shader_parameter("uCrystal", crystallize)
	m.set_shader_parameter("uCov", 0.78 - fire_coverage * 0.0045)
	m.set_shader_parameter("uBalance", 1.0 + hue_balance * 0.03)
	# gas
	m.set_shader_parameter("uMatZoom", material_zoom)
	m.set_shader_parameter("uShared", shared_composition)
	m.set_shader_parameter("uPerspective", perspective)
	m.set_shader_parameter("uSeamGlow", seam_glow)
	m.set_shader_parameter("uSeamCore", seam_core)
	m.set_shader_parameter("uSeamDisp", seam_dispersion)
	m.set_shader_parameter("uVertexR", vertex_radius)
	m.set_shader_parameter("uVertexGain", vertex_gain)
	m.set_shader_parameter("uFocus", focus_falloff)
	m.set_shader_parameter("uFocusIn", focus_inner)
	m.set_shader_parameter("uFocusOut", focus_outer)
	m.set_shader_parameter("uFaceKey", face_key)
	m.set_shader_parameter("uFaceFill", face_fill)
	m.set_shader_parameter("uFaceFloor", face_floor)
	m.set_shader_parameter("uFacePow", face_contrast)
	m.set_shader_parameter("uSurfGain", surface_specular)
	m.set_shader_parameter("uSurfPow", surface_sharpness)
	m.set_shader_parameter("uSurfRough", surface_roughness)
	m.set_shader_parameter("uSurfRim", surface_rim)
	m.set_shader_parameter("uDisperse", edge_dispersion)
	m.set_shader_parameter("uDisperseGain", dispersion_gain)
	m.set_shader_parameter("uFlowDepth", flow_depth)
	m.set_shader_parameter("uFlowSmooth", flow_smoothing)
	m.set_shader_parameter("uFlowLead", flow_lead)
	m.set_shader_parameter("uFlowHeat", flow_heat)
	m.set_shader_parameter("uFlowHue", flow_hue)
	m.set_shader_parameter("uFlowShear", flow_shear)
	m.set_shader_parameter("uAmbient", body_ambient)
	m.set_shader_parameter("uGlintFloor", glint_floor)
	m.set_shader_parameter("uIOR", index_of_refraction)
	m.set_shader_parameter("uDepth", interior_depth)
	m.set_shader_parameter("uGlassDepth", glass_depth)
	m.set_shader_parameter("uWispDepth", wisp_depth)
	m.set_shader_parameter("uGasScale", gas_scale)
	m.set_shader_parameter("uGasWarp", gas_warp)
	m.set_shader_parameter("uGasFreq", gas_freq)
	m.set_shader_parameter("uGasCov", gas_coverage)
	m.set_shader_parameter("uGasShoulder", gas_shoulder)
	m.set_shader_parameter("uIslFreq", island_freq)
	m.set_shader_parameter("uIslCov", island_coverage)
	m.set_shader_parameter("uIslFloor", island_floor)
	m.set_shader_parameter("uSlices", gas_slices)
	m.set_shader_parameter("uGasOccl", gas_occlusion)
	m.set_shader_parameter("uZShift", gas_depth_hue)
	m.set_shader_parameter("uGasGain", gas_gain / _pal_norm)
	# veins
	m.set_shader_parameter("uVeinStream", vein_stream)
	m.set_shader_parameter("uVeinFlowScale", vein_flow_scale)
	m.set_shader_parameter("uVeinMajor", vein_major_scale)
	m.set_shader_parameter("uVeinHier", vein_hierarchy)
	m.set_shader_parameter("uVeinFreq", vein_freq)
	m.set_shader_parameter("uVeinSharp", vein_sharpness)
	m.set_shader_parameter("uVeinAmt", vein_amount)
	m.set_shader_parameter("uBodyFall", body_falloff)
	m.set_shader_parameter("uHaze", haze)
	m.set_shader_parameter("uGrainF", grain_freq)
	m.set_shader_parameter("uHot0", hot_threshold)
	m.set_shader_parameter("uHotAmt", hot_amount)
	# colour field
	m.set_shader_parameter("uHueFreq", hue_zone_freq)
	m.set_shader_parameter("uSpreadIdle", hue_spread_idle)
	m.set_shader_parameter("uSpreadHover", hue_spread_hover)
	m.set_shader_parameter("uPGam", palette_gamma)
	# wisps
	m.set_shader_parameter("uWisp", wisp_tex())
	m.set_shader_parameter("uWispAmt", wisp_amount)
	m.set_shader_parameter("uWispPow", wisp_power)
	m.set_shader_parameter("uWispNorm", wisp_norm * bake_gain)
	m.set_shader_parameter("uDisp", dispersion)
	m.set_shader_parameter("uWispScale", wisp_scale)
	# glass
	m.set_shader_parameter("uGlassScale", glass_scale)
	m.set_shader_parameter("uCleaveFams", cleavage_families)
	m.set_shader_parameter("uGlassLayers", glass_layers)
	m.set_shader_parameter("uPresence", glass_presence)
	m.set_shader_parameter("uRefr", refraction)
	m.set_shader_parameter("uSRefr", wisp_refraction)
	m.set_shader_parameter("uGlint", glass_glint)
	m.set_shader_parameter("uFlash", pane_flash)
	m.set_shader_parameter("uPaneTilt", pane_tilt)
	m.set_shader_parameter("uPaneRamp", pane_ramp)
	m.set_shader_parameter("uPaneDamp", pane_depth_damp)
	m.set_shader_parameter("uPaneAbsorb", pane_absorb)
	m.set_shader_parameter("uGlassPar", glass_parallax)
	# stars and tone
	m.set_shader_parameter("uStarAmt", star_amount)
	m.set_shader_parameter("uSCurve", tonal_curve)
	m.set_shader_parameter("uOccl", layer_occlusion * 0.01)
	m.set_shader_parameter("uSheen", sheen * 0.01)
	m.set_shader_parameter("uDome", dome * 0.01)
	m.set_shader_parameter("uRim", rim_dispersion * 0.01)
	m.set_shader_parameter("uParGain", parallax_gain)
	m.set_shader_parameter("uOutGamma", stone_gamma)
	m.set_shader_parameter("uHardHue", hard_hue_shift)
	m.set_shader_parameter("uOff", flow_tex())
	m.set_shader_parameter("uDye", dye_tex())
	m.set_shader_parameter("uDyeGain", dye_gain)
	m.set_shader_parameter("uVel", vel_tex())
	m.set_shader_parameter("uTxS", sim_texel())
	# cut lighting
	m.set_shader_parameter("uLight", _light)
	m.set_shader_parameter("uLight2", fill_vector())
	m.set_shader_parameter("uFill", fill_gain)
	m.set_shader_parameter("uTiltResp", tilt_response)
	m.set_shader_parameter("uSpecPow", specular_power)
	m.set_shader_parameter("uSpecGain", specular_gain)
	m.set_shader_parameter("uDiffFloor", diffuse_floor)
	m.set_shader_parameter("uDiffGain", diffuse_gain)
	m.set_shader_parameter("uTiltPar", tilt_parallax)
	m.set_shader_parameter("uCursor", _mouse_uv)
	m.set_shader_parameter("uFilm", film_amount)
	m.set_shader_parameter("uFilmRadius", film_radius)
	m.set_shader_parameter("uFilmFlow", film_flow_wake)
	m.set_shader_parameter("uFilmScale", film_scale)
	m.set_shader_parameter("uFilmCycles", film_cycles)
	m.set_shader_parameter("uFilmBump", film_bump)
	m.set_shader_parameter("uFilmSpecPow", film_spec_power)
	m.set_shader_parameter("uFilmSpecGain", film_spec_gain)
	m.set_shader_parameter("uFilmThick", film_thickness)
	m.set_shader_parameter("uFilmVel", film_velocity)
	m.set_shader_parameter("uFilmCurl", film_curl)
	m.set_shader_parameter("uFlowTint", flow_tint)
	m.set_shader_parameter("uActLo", flow_gate_lo)
	m.set_shader_parameter("uActHi", flow_gate_hi)
	m.set_shader_parameter("uRestFlow", flow_at_rest)
	m.set_shader_parameter("uEdgeFade", edge_fade)
	m.set_shader_parameter("uLitPow", light_contrast)
	m.set_shader_parameter("uLitCov", light_coverage)
	m.set_shader_parameter("uSat", stone_saturation)
	m.set_shader_parameter("uExposure", stone_exposure)
	m.set_shader_parameter("uShoulder", stone_shoulder)
	m.set_shader_parameter("uWakeFire", wake_fire)
	m.set_shader_parameter("uRareBase", rare_threshold)
	m.set_shader_parameter("uWakeHue", wake_hue_shift)
	m.set_shader_parameter("uFlareGain", flare_gain)
	m.set_shader_parameter("uPulseGain", pulse_gain)


## THE BAKE IS NOT PART OF THE SIM. This used to switch every SubViewport to
## UPDATE_ALWAYS, which included the wisp bake — line integral convolution at
## roughly 48 noise-marched steps per pixel over 1024x576, re-rendered EVERY
## FRAME the menu was open. It is meant to run exactly once at startup; the
## field is static structure and nothing about it changes. This was by a wide
## margin the most expensive thing happening.
const BAKE_PASSES := ["wisp"]

func set_active(active: bool) -> void:
	set_process(active)
	for k in _vp:
		if k in BAKE_PASSES:
			# a bake only ever renders on demand, via rebake_wisps()
			continue
		_vp[k].render_target_update_mode = SubViewport.UPDATE_ALWAYS if active else SubViewport.UPDATE_DISABLED


# ── Pipeline ──────────────────────────────────────────────────────────────────
func _mk_shader(src: String) -> Shader:
	var s := Shader.new()
	s.code = src % SH_COMMON if src.contains("%s") else src
	return s


func _mk_pass(name_: String, shader_src: String) -> void:
	var vp := SubViewport.new()
	vp.name = name_
	vp.size = Vector2i(SIM_W, SIM_H)
	vp.disable_3d = true
	vp.use_hdr_2d = false
	vp.transparent_bg = true
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	add_child(vp)
	var rect := ColorRect.new()
	rect.size = Vector2(SIM_W, SIM_H)
	var mat := ShaderMaterial.new()
	mat.shader = _mk_shader(shader_src)
	rect.material = mat
	vp.add_child(rect)
	_vp[name_] = vp
	_mat[name_] = mat


func _build_pipeline() -> void:
	var n_it: int = maxi(jacobi_iterations, 2)
	_jacobi.clear()

	_mk_pass("advect", SH_ADVECT)
	_mk_pass("splat", SH_SPLAT)
	_mk_pass("vort", SH_VORT)
	_mk_pass("div", SH_DIV)
	# Tree order is render order, so each iteration reads the one before it in
	# the SAME frame. Only the first reads across a frame boundary, which just
	# warm-starts it from last frame's solution.
	for i in n_it:
		var nm := "j%d" % i
		_mk_pass(nm, SH_JACOBI)
		_jacobi.append(nm)
	_mk_pass("grad", SH_GRAD)
	_mk_pass("dye1", SH_DYE)
	_mk_pass("dye2", SH_COPY)
	_mk_pass("off1", SH_OFFSET)
	_mk_pass("off2", SH_COPY)
	_build_wisp_bake()

	var tx := Vector2(1.0 / SIM_W, 1.0 / SIM_H)
	var dt := 1.0 / 60.0

	_mat["advect"].set_shader_parameter("uV", _vp["grad"].get_texture())
	_mat["advect"].set_shader_parameter("uDt", dt)
	_mat["advect"].set_shader_parameter("uTx", tx)
	_mat["advect"].set_shader_parameter("uDis", velocity_decay)

	_mat["splat"].set_shader_parameter("uS", _vp["advect"].get_texture())
	_mat["splat"].set_shader_parameter("uR", stir_radius)
	_mat["splat"].set_shader_parameter("uAsp", float(SIM_W) / float(SIM_H))
	_mat["splat"].set_shader_parameter("uF", Vector2.ZERO)
	_mat["splat"].set_shader_parameter("uP", Vector2(0.5, 0.5))
	_mat["splat"].set_shader_parameter("uP1", Vector2(0.5, 0.5))

	_mat["vort"].set_shader_parameter("uV", _vp["splat"].get_texture())
	_mat["vort"].set_shader_parameter("uTx", tx)
	_mat["vort"].set_shader_parameter("uEps", vorticity)

	_mat["div"].set_shader_parameter("uV", _vp["vort"].get_texture())
	_mat["div"].set_shader_parameter("uTx", tx)
	_mat["div"].set_shader_parameter("uPScale", pressure_scale)

	var last: String = _jacobi[_jacobi.size() - 1]
	for i in _jacobi.size():
		var nm: String = _jacobi[i]
		var prev: String = last if i == 0 else _jacobi[i - 1]
		_mat[nm].set_shader_parameter("uP", _vp[prev].get_texture())
		_mat[nm].set_shader_parameter("uD", _vp["div"].get_texture())
		_mat[nm].set_shader_parameter("uTx", tx)

	_mat["grad"].set_shader_parameter("uP", _vp[last].get_texture())
	_mat["grad"].set_shader_parameter("uV", _vp["vort"].get_texture())
	_mat["grad"].set_shader_parameter("uTx", tx)
	_mat["grad"].set_shader_parameter("uPScale", pressure_scale)
	_mat["grad"].set_shader_parameter("uVMax", max_speed)

	_mat["dye1"].set_shader_parameter("uD", _vp["dye2"].get_texture())
	_mat["dye1"].set_shader_parameter("uV", _vp["grad"].get_texture())
	_mat["dye1"].set_shader_parameter("uDt", dt)
	_mat["dye1"].set_shader_parameter("uTx", tx)
	_mat["dye1"].set_shader_parameter("uR", dye_radius)
	_mat["dye1"].set_shader_parameter("uAsp", float(SIM_W) / float(SIM_H))
	_mat["dye1"].set_shader_parameter("uDecay", dye_decay)
	_mat["dye1"].set_shader_parameter("uInject", 0.0)
	_mat["dye1"].set_shader_parameter("uP", Vector2(0.5, 0.5))
	_mat["dye1"].set_shader_parameter("uP1", Vector2(0.5, 0.5))
	_mat["dye2"].set_shader_parameter("uS", _vp["dye1"].get_texture())

	_mat["off1"].set_shader_parameter("uO", _vp["off2"].get_texture())
	_mat["off1"].set_shader_parameter("uV", _vp["grad"].get_texture())
	_mat["off1"].set_shader_parameter("uDt", dt)
	_mat["off1"].set_shader_parameter("uTx", tx)
	_mat["off1"].set_shader_parameter("uDecay", offset_decay)
	_mat["off2"].set_shader_parameter("uS", _vp["off1"].get_texture())


## The wisp field: rendered ONCE into its own SubViewport at startup. Line
## integral convolution is far too expensive to evaluate per pixel per frame,
## and it is static structure anyway — nothing about it needs to change once
## it exists. Facets sample it with mirrored tiling.
func _build_wisp_bake() -> void:
	var vp := SubViewport.new()
	vp.name = "wisp"
	vp.size = wisp_bake_size
	vp.disable_3d = true
	vp.use_hdr_2d = false
	vp.transparent_bg = false
	# ONCE. Not ALWAYS — this is a bake, not a simulation.
	vp.render_target_update_mode = SubViewport.UPDATE_ONCE
	add_child(vp)
	var rect := ColorRect.new()
	rect.size = Vector2(wisp_bake_size)
	var mat := ShaderMaterial.new()
	var sh := Shader.new()
	sh.code = SH_WISP
	mat.shader = sh
	mat.set_shader_parameter("uSteps", lic_steps)
	mat.set_shader_parameter("uStep", lic_step)
	mat.set_shader_parameter("uTaper", lic_taper)
	mat.set_shader_parameter("uFlow", flow_scale)
	mat.set_shader_parameter("uSeedDens", seed_density)
	mat.set_shader_parameter("uSeedFreq", seed_freq)
	mat.set_shader_parameter("uSeedTight", seed_tight)
	mat.set_shader_parameter("uGain", bake_gain)
	mat.set_shader_parameter("uAsp", float(wisp_bake_size.x) / float(maxi(wisp_bake_size.y, 1)))
	rect.material = mat
	vp.add_child(rect)
	_vp["wisp"] = vp
	_mat["wisp"] = mat


func wisp_tex() -> Texture2D:
	return _vp["wisp"].get_texture() if _vp.has("wisp") else null


## Re-run the bake. Call after changing any Wisp bake export at runtime.
func rebake_wisps() -> void:
	if not _vp.has("wisp"):
		return
	var m: ShaderMaterial = _mat["wisp"]
	m.set_shader_parameter("uSteps", lic_steps)
	m.set_shader_parameter("uStep", lic_step)
	m.set_shader_parameter("uTaper", lic_taper)
	m.set_shader_parameter("uFlow", flow_scale)
	m.set_shader_parameter("uSeedDens", seed_density)
	m.set_shader_parameter("uSeedFreq", seed_freq)
	m.set_shader_parameter("uSeedTight", seed_tight)
	m.set_shader_parameter("uGain", bake_gain)
	_vp["wisp"].render_target_update_mode = SubViewport.UPDATE_ONCE


## Mean luminance of the palette, through the same gamma the material applies.
## Dividing gas_gain by this makes brightness independent of which ramp is
## loaded — swap palettes freely without re-tuning.
func _palette_norm() -> float:
	if palette == null or palette.gradient == null:
		return 0.132
	var acc := 0.0
	var n := 48
	for i in n:
		var t := float(i) / float(n - 1)
		var c: Color = palette.gradient.sample(t)
		acc += pow(maxf(c.r, 0.0), palette_gamma) * 0.2126
		acc += pow(maxf(c.g, 0.0), palette_gamma) * 0.7152
		acc += pow(maxf(c.b, 0.0), palette_gamma) * 0.0722
	return maxf(acc / float(n), 0.01)


## Call after changing the palette or palette_gamma at runtime.
func refresh_palette() -> void:
	if use_builtin_palette:
		palette = _default_palette()
	_pal_norm = _palette_norm()


func _default_palette() -> GradientTexture1D:
	# A JEWEL ramp, not a neon rainbow. The previous default was a cosine ramp
	# at full saturation, which gives pure primaries — that palette is most of
	# why the faces read as stained glass rather than as a stone. This one
	# travels the same hue circle but holds saturation and value down, so the
	# fire is deep teal, gold, ember and violet.
	# LOW CHROMA is the point, not low value. Gain is normalised against the
	# palette, so a dark ramp simply gets scaled back up and lands just as
	# saturated — the muted quality has to live in the channel RATIOS. These
	# sit near 0.35 saturation after the full chain: stone, with the hot vein
	# knots and glints supplying the few genuinely vivid accents.
	var stops := [
		Color(0.16, 0.22, 0.26), Color(0.15, 0.25, 0.25), Color(0.17, 0.27, 0.22),
		Color(0.22, 0.28, 0.20), Color(0.28, 0.29, 0.19), Color(0.32, 0.27, 0.19),
		Color(0.33, 0.23, 0.19), Color(0.32, 0.20, 0.21), Color(0.29, 0.18, 0.24),
		Color(0.24, 0.17, 0.27), Color(0.19, 0.18, 0.28), Color(0.16, 0.22, 0.26)]
	var g := Gradient.new()
	g.remove_point(0)
	for i in stops.size():
		var t := float(i) / float(stops.size() - 1)
		if i == 0:
			g.set_color(0, stops[0])
			g.set_offset(0, 0.0)
		else:
			g.add_point(t, stops[i])
	var gt := GradientTexture1D.new()
	gt.gradient = g
	gt.width = 256
	return gt


# ── Per-frame ─────────────────────────────────────────────────────────────────
func _process(_delta: float) -> void:
	_t = Time.get_ticks_msec() / 1000.0 - _t0

	if _boot >= 0:
		var r := 1.0 if _boot > 0 else 0.0
		for k in _mat:
			_mat[k].set_shader_parameter("uReset", r)
		_boot -= 1

	_track_cursor()
	if ambient_currents:
		var ph := _t * 0.05
		stir(Vector2(0.5 + 0.34 * sin(ph), 0.5 + 0.3 * cos(ph * 0.83)),
			Vector2(cos(ph) * 0.045, sin(ph * 0.83) * 0.04))

	if _pending_n > 0:
		_mat["splat"].set_shader_parameter("uP", _seg_a)
		_mat["splat"].set_shader_parameter("uP1", _seg_b)
		_mat["splat"].set_shader_parameter("uF", _pending_force.limit_length(12.0))
		_mat["dye1"].set_shader_parameter("uP", _seg_a)
		_mat["dye1"].set_shader_parameter("uP1", _seg_b)
		# a slow stroke lays a fine thread, a fast one a broad turbulent trail
		var spd: float = clampf(_stroke_speed, 0.0, 3.0)
		_mat["dye1"].set_shader_parameter("uInject",
			dye_inject * clampf(0.20 + spd * 0.9, 0.10, 2.0))
		_mat["dye1"].set_shader_parameter("uR",
			dye_radius * (1.0 + spd * stir_spread))
		_mat["splat"].set_shader_parameter("uShear", stir_curl * clampf(spd, 0.0, 2.0))
		_mat["splat"].set_shader_parameter("uDir", _stroke_dir)
		_pending_force = Vector2.ZERO
		_pending_pos = Vector2(0.5, 0.5)
		_pending_n = 0
	else:
		_mat["splat"].set_shader_parameter("uF", Vector2.ZERO)
		_mat["splat"].set_shader_parameter("uShear", 0.0)
		_mat["dye1"].set_shader_parameter("uInject", 0.0)
		_stroke_speed *= 0.85

	_mat["splat"].set_shader_parameter("uBody", _body.limit_length(0.7))
	_mat["splat"].set_shader_parameter("uSwirl", clampf(_swirl, -1.2, 1.2))
	_body *= slosh_decay
	_swirl *= slosh_decay

	if not _light_ready:
		_light = base_light()
		_light_t = _light
		_light_ready = true
	if not light_follows_cursor:
		_light_t = base_light()
	_light = _light.lerp(_light_t, light_aim_lerp).normalized()

	_mat["vort"].set_shader_parameter("uEps", vorticity)
	_mat["vort"].set_shader_parameter("uVortGate", vorticity_gate)
	_mat["advect"].set_shader_parameter("uDis", velocity_decay)
	_mat["off1"].set_shader_parameter("uDecay", offset_decay)
	_mat["div"].set_shader_parameter("uPScale", pressure_scale)
	_mat["grad"].set_shader_parameter("uPScale", pressure_scale)
	_mat["grad"].set_shader_parameter("uVMax", max_speed)
	_mat["grad"].set_shader_parameter("uBorder", border_width)

	_view += (_view_t - _view) * 0.05
	_bias += (_bias_t - _bias) * view_bias_lerp
	_f_hue = _lerp_hue(_f_hue, _f_hue_t, focus_lerp)
	_f_spread += (_f_spread_t - _f_spread) * focus_lerp
	_f_amt += (_f_amt_t - _f_amt) * focus_lerp
	_hard += (_hard_t - _hard) * hard_lerp


func _lerp_hue(a: float, b: float, w: float) -> float:
	var d := fposmod(b - a + 50.0, 100.0) - 50.0
	return fposmod(a + d * w, 100.0)


func _track_cursor() -> void:
	var vp := get_viewport()
	if vp == null:
		return
	var rs := vp.get_visible_rect().size
	if rs.x <= 0.0 or rs.y <= 0.0:
		return
	var uv := vp.get_mouse_position() / rs
	if uv.x < -0.2 or uv.x > 1.2 or uv.y < -0.2 or uv.y > 1.2:
		_last_mouse = Vector2.INF
		return
	_mouse_uv = uv
	_view_t = Vector2((uv.x - 0.5) * 0.5, (uv.y - 0.5) * 0.5)
	if not cursor_drives_fluid:
		_last_mouse = uv
		_last_t = _t
		return

	# The probe is what actually touches the fluid, and it TRAILS the cursor.
	# Stirring straight from the mouse position gives a massless emitter that
	# is always exactly where your hand is; a lagging probe has to catch up,
	# so the fluid responds to a thing moving through it.
	if not _probe_init:
		_probe = uv
		_probe_prev = uv
		_probe_init = true
	_probe_prev = _probe
	_probe = _probe.lerp(uv, stir_lag)

	if _last_mouse != Vector2.INF:
		var dt: float = maxf(_t - _last_t, 0.001)
		var vel: Vector2 = (_probe - _probe_prev) / dt
		var sp: float = vel.length()
		if sp > 0.02:
			# NO HARD CLAMP. limit_length() capped every stroke at the same
			# magnitude, so a slow drag and a fast flick injected an identical
			# impulse — which is why every movement produced the same jet.
			# A soft knee keeps the response monotonic: gentle movement gives
			# a gentle curl, a flick genuinely hits harder, and nothing ever
			# saturates into a wall.
			var norm: float = sp / maxf(stir_reference, 0.001)
			var shaped: float = pow(norm, stir_response) / (1.0 + pow(norm, stir_response) / maxf(stir_ceiling, 0.01))
			var f: Vector2 = vel / maxf(sp, 1e-5) * shaped * stir_force
			stir_segment(_probe_prev, _probe, f)
			_stroke_speed = norm
			_stroke_dir = vel / maxf(sp, 1e-5)
	_last_mouse = uv
	_last_t = _t
