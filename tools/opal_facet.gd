class_name OpalFacet
extends Control
# ── opal_facet.gd — BUILD 21 ──────────────────────────────────────────────────
# BUILD 21 — THE LAYERED GEM. The shard-mosaic fire is gone. It was the wrong
# material class: a mosaic of opaque coloured tiles can never read as a stone
# with light inside it, and no amount of tuning changes that. What replaces it:
#
#   THE GAS      an emissive volume in depth slices, each parallaxing at its
#                own rate, so the light is visibly INSIDE the stone. One
#                shared hue field, so every slice agrees on the local colour
#                and regions hold a hue family instead of muddying each other.
#   THE VEINS    ridged multi-octave crests: the colour runs in sharp
#                branching filaments, not billows.
#   THE WISPS    curving fire threads, baked once by OpalCore via line
#                integral convolution along a curl-noise flow. Read three
#                times at slightly different offsets — chromatic dispersion,
#                so a refracted filament fringes into colour instead of
#                looking chopped.
#   THE GLASS    five sparse, INCOMPLETE fracture sheets at different depths.
#                Each present pane refracts the gas behind it and catches the
#                shared lamp with its own normal AND a gradient across its
#                face. Because the sheets are incomplete and overlapping, no
#                lattice ever forms: you cannot tell where a shard begins.
#
# Two findings from the offline prototype that constrain this code:
#
#   * fb() has a MEASURED mean of 0.3854 and sd 0.1196. It is not uniform on
#     0..1 centred at 0.5. Every signed field written as (fb - 0.5) was
#     therefore biased low and compressed — which is why the colour window
#     could only ever wander through blue-violet. Signed fields now go through
#     nfb(), normalised against those measured statistics.
#   * Refraction on THIN features must come from the coarse sheets only. Fine
#     shards displace a filament by less than its own width in random
#     directions, which minces it. Big panes shift whole runs sideways, which
#     is what glass actually does.
#
# Hover model (locked): idle runs the full spectrum, hover COLLAPSES a face to
# its narrow identity window. uSpreadIdle -> uSpreadHover by wake.
#
# ── BUILD 19 notes ────────────────────────────────────────────────────────────
# BUILD 19 — THE DARK BODY. BUILD 18 partitioned the fire correctly but the
# stone stayed foggy and the shards came out as confetti. Three causes:
#
#   1. THE GAMMA ENCODE LIFTS THE BODY. body was 0.012-0.042 linear, and
#      pow(0.03, 0.4545) is about 0.20 — the final encode turns "near-black"
#      into 20 percent grey before saturation and exposure even run. No fire
#      contrast dial can fight that: the void must be nearly ZERO in linear so
#      it stays black through the encode. body's dark endpoint is now ~0.002.
#
#   2. THE FACET SPECULAR IS A UNIFORM WASH. Nf is constant across a face, so
#      the specular add was a flat milky sheen over the ENTIRE facet whenever
#      it aligned with a lamp — the sage fog on the brightest faces. It is now
#      gated by fire presence: highlights ride the fire, the void stays void.
#
#   3. THE PER-SHARD HUE NUDGE WAS RANDOM, AND HUGE. A hash per cell scaled by
#      spread meant +/-16 percent of the whole palette per shard at spread 72:
#      confetti. It now samples fbm at the cell centre — neighbouring shards
#      land on nearby palette phases, so colour runs through the stone in
#      fields of kin hues, stepping at each shard instead of scattering.
#
#   Also: output dither halved — it was visible grain on a black void.
#
# ── BUILD 18 notes ────────────────────────────────────────────────────────────
# BUILD 18 — THE SHARD PARTITION. The fire stops being soft blobs and becomes
# angular micro-facets. This is the material rework toward the target render.
#
# What changed and why:
#
#   1. STRETCHED VORONOI CELLS, not lath quantization. The old crystallize
#      snapped the SAMPLING coordinate onto a rotated grid, which crazes the
#      surface — veins on fog — because the underlying field stays smooth.
#      Now the fire is PARTITIONED: a Voronoi division in lath space (cells
#      elongated along the fire direction, so the cut orientation survives),
#      and colour, brightness and on/off are all CELL-CONSTANT. A patch of
#      fire is a set of hard-edged shards, not a gradient.
#
#   2. PER-SHARD PSEUDO-NORMALS. Each cell derives a stable random tilt from
#      its ID and is relit by the existing key + fill lamps. Two neighbouring
#      shards of the same hue now differ in brightness the way real
#      sub-facets catch a lamp differently — this is the glassy read, and it
#      moves when the stone turns because the lamps are shared.
#
#   3. FIRE CONTRAST. A power curve on fire density pushes mid-density fire
#      toward black. The old flat response is what produced the pale foggy
#      faces: half-density fire rendered as half-bright wash. Dark body,
#      fire as the event.
#
#   4. MICRO-SPARKLE. Pinpoint star-glints across the whole stone, twinkling
#      slowly, independent of the fire. Replaces most of what particles was
#      doing, in-material.
#
#   crystallize is now the blend between the old smooth fire (0) and the full
#   shard partition (100). It is pushed from OpalCore with the rest of the
#   recipe, along with the new "Recipe — shards" group. Scenes that saved
#   crystallize=35 as an Inspector override need it raised (~80) to see this.
#
# ── BUILD 17 notes ────────────────────────────────────────────────────────────
# BUILD 17 — SELECTION FLAIR, second attempt. BUILD 16 had three problems:
#   * flooding `rare` pops. rid quantises into 7 regions and rare is a hard
#     step(), so a lerping threshold flips regions one at a time — a visible
#     staircase, not a transition.
#   * cycling the palette across the bevel reads as chromatic aberration, i.e.
#     a rendering artifact. A real facet edge catches a bright specular line
#     with colour only at its margins; it does not fan a spectrum.
#   * and it ran on the table facet too, because nothing ever set the table's
#     wake, so it sat at the default 1.0 the whole time.
#
# Now: the woken face SHIFTS ITS HUE toward its own identity colour. Continuous,
# so it cannot pop, and it is a tint on one face rather than a permanent palette
# difference between faces. Its seams brighten in that same colour instead of
# fanning a rainbow. Same shape language, louder and tinted.
#
# ── BUILD 7 notes ─────────────────────────────────────────────────────────────
# BUILD 7: THE OIL FILM.
#
# Until now the fluid could only displace where the fire was sampled, so it
# read as warping rather than liquid. Now there are two materials:
#
#   the FIRE  is inside the stone — slow, deep, occluded
#   the FILM  is on the surface of each facet — thin, fast, iridescent
#
# The film is thin-film interference: its colour comes from thickness, and
# thickness comes from the flow field, so hue slides as the fluid moves. It
# carries its OWN perturbed normal, built from the gradient of that thickness,
# so its highlights bend and ripple independently of the facet beneath.
# Thickness also scales by 1/N.z — at a glancing angle you look through more
# film, so tilted faces band harder. That is what ties it to the geometry.
#
# It wakes near the cursor and where the fluid is genuinely moving, and sleeps
# everywhere else, so the stone stays calm until touched.
# BUILD 6:
#   1. SOFT SHOULDER instead of a hard clamp. A hard clamp chops one channel
#      before the others, which slides bright colours toward white — that is
#      why the most lit face was the most washed out. The shoulder scales all
#      three channels together, so a face gets brighter without losing its hue.
#   2. RARE FLASH OFFSET per facet. Every face now shares one base palette; its
#      identity is the colour its rare regions flash. Colour coding without
#      turning the stone into stained glass.
#   3. Wake no longer dims by 0.30 — that multiply was a second washing-out.
#      Sleeping faces lose FIRE, not brightness.
# BUILD 5: lighting reworked. Three fixes to the washed-out look —
#   1. diff is applied through a power curve that PRE-COMPENSATES for the final
#      gamma encode, so brightness contrast between faces survives to screen.
#      A flat multiply before pow(0.4545) crushes a 2x ratio down to 1.37x.
#   2. Lighting now drives fire DENSITY as well as brightness. A dark face gets
#      fewer flashes, not dimmer ones — which is what an unlit facet actually
#      looks like, and it keeps every flash saturated.
#   3. Saturation and exposure are applied AFTER the gamma encode, to claw back
#      what additive layering, particles and specular take out.
# BUILD 4: seams only on INTERIOR edges. An edge lying on the screen border is
# not a cut — nothing is on the other side of it — so it gets no seam and no
# bevel refraction. The w component of each uCut carries that flag.
# Seams are also calmer: less hue sweep, more white in the core.
# BUILD 3: convex polygons of 3 to 6 points, not just quads. The table facet is
# a hexagon — six bezels means six inner edges — and OpalMenu shares those
# vertices with the bezels so expansion never opens a gap.
# BUILD 2: TILT. Each facet is now a plane in 3D with a surface normal, lit by
# the single light on OpalCore. One face blazes, the next goes nearly black —
# that contrast is what reads as a cut object rather than a flat mosaic.
# Tilt also scales parallax: a steep face shifts its fire more as you move.
# One cut face of the stone. Place several to build a faceted composition.
#
# The facet is a CONVEX POLYGON given in viewport UV (0..1), so it is
# resolution independent — the same numbers work at any window size.
#
# It samples OpalCore in SCREEN space, so the fluid is continuous across every
# facet: one stir travels through the whole stone as one body. What the facet
# changes is TWO rotations —
#   * the fire basis   (pattern, region shapes, lath direction)
#   * the sampled flow (so flow-aligned laths point differently too)
# — which is why the fire visibly snaps direction at every seam while the
# motion stays coherent. Continuous interior, different viewing angle: exactly
# what happens when you cut a real opal.
#
# Cheap knobs worth knowing:
#   layers    3 for big background facets, 1 for small UI cells (3x cheaper)
#   wake      0 = damped/asleep, 1 = full. OpalMenu drives this per cell.
#   seam_gain the bright edge. This is what makes it read as CUT rather than
#             just differently coloured — do not set it to zero and wonder why.

@export var sim_path: NodePath

@export_group("Shape")
## Convex polygon, 3 to 6 points, in viewport UV (0..1).
## Order matters: go around the perimeter, not across.
@export var poly: PackedVector2Array = PackedVector2Array([
	Vector2(0.0, 0.0), Vector2(1.0, 0.0), Vector2(1.0, 1.0), Vector2(0.0, 1.0)])
@export var seam_width: float = 0.030
@export var seam_gain: float = 0.7
@export var refract: float = 1.0

@export_group("Cut")
## The face's slope. This is what lights it — a face tilted toward the light
## blazes, one tilted away goes dark. Range about -0.6 .. 0.6.
@export var tilt: Vector2 = Vector2.ZERO
@export_range(-90.0, 90.0) var angle_deg: float = 0.0
@export var pattern_offset: Vector2 = Vector2.ZERO
@export var hue: float = 19.0
@export var spread: float = 100.0
@export var scale_mul: float = 1.0
@export_range(-0.3, 0.3) var coverage_bias: float = 0.0
## Where this face's RARE regions flash to on the palette. This is the facet's
## identity — the base stone is the same everywhere.
@export var rare_offset: float = 0.45
## This face's colour window CENTRE on the shared palette, 0..1. Idle, the
## window is wide and the face runs the full spectrum around this point; on
## hover it collapses to a narrow band here — its signature. OpalMenu sets it
## per root, so every bezel owns a hue without the stone becoming stained glass.
@export var identity_hue: float = 0.19
@export var brightness: float = 1.0
## Retired: depth is now global (OpalCore.gas_slices). Kept so existing
## scenes still load without an Inspector error.
@export_range(1, 3) var layers: int = 3

## How much fire this face gives up. The table should be the CALMEST face on
## the stone — the centre of a radial layout wants to be rest while the
## perimeter carries the motion. Raising this removes fire rather than dimming
## it, so the face goes dark and clean instead of grey.
@export_range(0.0, 1.0) var calm: float = 0.0

## How much this face's seams RIDE the fire behind them. 0 is a seam of even
## brightness along its whole length — correct for the hub, whose cuts are
## short and meet at vertex flares constantly. Above 0 the cut is bright only
## where there is light behind it to catch and goes out in the dark stretches,
## which is what one long cut across a single large face needs in order to
## read as a cut rather than as a drawn line.
@export_range(0.0, 1.0) var seam_ride: float = 0.0
## How hard it falls off in the dark. Higher = more of the cut disappears.
@export_range(0.5, 4.0) var seam_ride_falloff: float = 1.7
## Scale of the drift along the seam, so the catch travels.
@export var seam_grain: float = 2.6

@export_group("The eye")
## Only the table facet turns this on. The eye is a region of THIS facet's
## material warped into polar form, not a node drawn over it.
@export var eye_enabled: bool = false
## Centre, in viewport UV.
@export var eye_centre: Vector2 = Vector2(0.5, 0.5)
## Aperture half-width and half-height, in aspect-corrected UV.
@export var eye_width: float = 0.16
@export var eye_height: float = 0.062
## 1 = as open as it gets, 0 = closed. Blinking drives this.
@export_range(0.0, 1.0) var eye_open: float = 1.0
## How low the upper lid sits when fully "open". Well under 1: a heavy lid
## reads as watchful, a raised one reads as alarmed.
@export_range(0.1, 1.0) var eye_lid_low: float = 0.62
## Iris radius.
@export var eye_radius: float = 0.085
## Where the iris is looking, -1..1 within the aperture.
@export var eye_look: Vector2 = Vector2.ZERO
## How the material is stretched around the iris. Angle scale sets how many
## fibres run out from the pupil; radial scale sets how fast they change
## along their length.
## LARGE on purpose. The angular axis must cross many material periods for
## the iris to have fibres running out from the pupil; a small value packs
## everything into a smear.
@export var eye_angle_scale: float = 2.6
## SMALL on purpose. Features should change slowly along a fibre's length, so
## they stretch outward instead of breaking into blobs.
@export var eye_radial_scale: float = 0.22
## Pupil radius as a fraction of the iris. The material is erased inside it.
@export_range(0.05, 0.8) var eye_pupil: float = 0.34
@export var eye_edge_soft: float = 0.012
## How hard the material dims toward the iris rim. This is what gives the iris
## an edge; without it the warp reads as a smear.
@export_range(0.0, 1.0) var eye_limbal: float = 0.75
## How far the body inside the aperture sits back from the iris.
@export_range(0.0, 1.0) var eye_sclera_dim: float = 0.35
## His colour. The iris collapses to a narrow window around this, so it is one
## hue with a tonal range rather than the whole palette at once.
@export_range(0.0, 1.0) var eye_hue: float = 0.62
@export_range(0.0, 0.6) var eye_hue_spread: float = 0.10
## Floor of the iris tonal ramp: dark at the pupil, bright through the mid
## band, dark at the rim. This is what makes the iris read as a form.
@export_range(0.0, 1.0) var eye_ramp_floor: float = 0.28
## Shadow cast by the upper lid onto what is behind it. An opening without one
## reads as a hole cut in paper.
@export_range(0.0, 1.0) var eye_lid_shadow: float = 0.55
@export_range(0.05, 1.0) var eye_lid_shadow_depth: float = 0.42
## Asymmetry between the inner and outer corner. A symmetrical lens is an
## ornament; an eye is not symmetrical.
@export_range(-0.6, 0.6) var eye_skew: float = 0.16
## THE SOCKET: how much material is suppressed around the aperture, and how
## far the pool reaches. This is the calm surround the eye needs to read
## against — it does more for legibility than anything inside the iris.
@export_range(0.0, 1.0) var socket_depth: float = 0.8
@export var socket_inner: float = 1.0
@export var socket_outer: float = 3.4
@export var eye_lash_width: float = 0.006
## Weight of the lash line. This is a CUT — it should sit at about the same
## brightness as a facet seam, not brighter than the stone.
@export var eye_lash_gain: float = 0.5
@export var eye_vertex_gain: float = 0.35

@export_group("Runtime")
@export_range(0.0, 1.0) var wake: float = 1.0
@export var wake_lerp: float = 0.09
@export var pulse_decay: float = 0.90

var _sim: OpalCore
var _mat: ShaderMaterial
var _wake := 1.0
var _pulse := 0.0

const SH_COMMON := """
float dec1f(vec4 t){ return ((t.r + t.g / 255.0) - 0.5) * 8.0; }
float bl1(sampler2D s, vec2 uv, vec2 tx){
	vec2 g = uv / tx - 0.5;
	vec2 f = fract(g);
	vec2 b = (floor(g) + 0.5) * tx;
	float A = dec1f(texture(s, b));
	float B = dec1f(texture(s, b + vec2(tx.x, 0.0)));
	float C = dec1f(texture(s, b + vec2(0.0, tx.y)));
	float D = dec1f(texture(s, b + tx));
	return mix(mix(A, B, f.x), mix(C, D, f.x), f.y);
}
vec2 bl2(sampler2D s, vec2 uv, vec2 tx){
	vec2 g = uv / tx - 0.5;
	vec2 f = fract(g);
	vec2 b = (floor(g) + 0.5) * tx;
	vec2 A = vec2(((texture(s,b).r + texture(s,b).g/255.0)-0.5)*8.0, ((texture(s,b).b + texture(s,b).a/255.0)-0.5)*8.0);
	vec4 tB = texture(s, b + vec2(tx.x,0.0));
	vec4 tC = texture(s, b + vec2(0.0,tx.y));
	vec4 tD = texture(s, b + tx);
	vec2 B = vec2(((tB.r + tB.g/255.0)-0.5)*8.0, ((tB.b + tB.a/255.0)-0.5)*8.0);
	vec2 C = vec2(((tC.r + tC.g/255.0)-0.5)*8.0, ((tC.b + tC.a/255.0)-0.5)*8.0);
	vec2 D = vec2(((tD.r + tD.g/255.0)-0.5)*8.0, ((tD.b + tD.a/255.0)-0.5)*8.0);
	return mix(mix(A,B,f.x), mix(C,D,f.x), f.y);
}
"""

const SH_FACET := """
shader_type canvas_item;

uniform sampler2D uOff : filter_nearest, repeat_disable;
uniform sampler2D uVel : filter_nearest, repeat_disable;
uniform vec2 uTxS = vec2(0.00390625, 0.00694444);
uniform sampler2D uPal : repeat_enable;
uniform float uT = 0.0;
uniform vec2 uView = vec2(0.0);
uniform float uAsp = 1.7778;

uniform float uScale = 7.0;
uniform float uDark = 100.0;
uniform float uRest = 100.0;
uniform float uRoll = 68.0;
uniform float uShard = 25.0;
uniform float uPart = 4.0;
uniform float uFlu = 0.0667;
uniform float uRockAmt = 0.0;
uniform float uGamma = 2.2;
uniform float uCrystal = 80.0;
uniform float uCov = 0.51;
uniform float uBalance = 2.5;
uniform float uOccl = 0.65;
uniform float uSheen = 0.45;
uniform float uDome = 0.35;
uniform float uRim = 0.40;
uniform float uParGain = 2.5;
uniform float uOutGamma = 0.4545;
uniform float uHard = 0.0;
uniform float uHardHue = 61.0;



uniform float uFHue = 19.0;
uniform float uFSpread = 100.0;
uniform float uFAng = 0.0;
uniform vec2 uFOfs = vec2(0.0);
uniform float uFScale = 1.0;
uniform float uFCov = 0.0;
uniform float uFBright = 1.0;
uniform float uFWake = 1.0;
uniform int uFLayers = 3;
uniform vec4 uCut0 = vec4(0.0, 0.0, 1.0, 0.0);
uniform vec4 uCut1 = vec4(0.0, 0.0, 1.0, 0.0);
uniform vec4 uCut2 = vec4(0.0, 0.0, 1.0, 0.0);
uniform vec4 uCut3 = vec4(0.0, 0.0, 1.0, 0.0);
uniform vec4 uCut4 = vec4(0.0, 0.0, 1.0, 0.0);
uniform vec4 uCut5 = vec4(0.0, 0.0, 1.0, 0.0);
uniform float uSeamW = 0.03;
uniform float uSeamGain = 1.0;
uniform float uRefract = 1.0;

uniform vec2 uFTilt = vec2(0.0);
uniform vec3 uLight = vec3(-0.59, 0.41, 0.70);
uniform vec3 uLight2 = vec3(0.62, -0.35, 0.44);
uniform float uFill = 0.60;
uniform float uTiltResp = 0.9;
uniform float uSpecPow = 26.0;
uniform float uSpecGain = 1.0;
uniform float uDiffFloor = 0.22;
uniform float uDiffGain = 1.0;
uniform float uTiltPar = 1.6;
uniform float uLitPow = 2.0;
uniform float uLitCov = 0.10;
uniform float uSat = 1.35;
uniform float uExposure = 1.15;
uniform float uShoulder = 0.80;
uniform vec2 uCursor = vec2(0.5);
uniform float uFilm = 1.0;
uniform float uFilmRadius = 0.30;
uniform float uFilmFlow = 1.6;
uniform float uFilmScale = 2.6;
uniform float uFilmCycles = 2.4;
uniform float uFilmBump = 0.055;
uniform float uFilmSpecPow = 34.0;
uniform float uFilmSpecGain = 1.0;
uniform float uFilmVel = 1.5;
uniform float uFilmCurl = 0.9;
uniform float uFlowTint = 0.75;
uniform float uActLo = 0.06;
uniform float uActHi = 0.55;
uniform float uRestFlow = 0.0;
uniform float uEdgeFade = 0.10;
uniform sampler2D uDye : filter_nearest, repeat_disable;
uniform float uDyeGain = 1.9;
uniform float uFilmThick = 3.4;
uniform float uRareBase = 0.87;
uniform float uRareAwake = 0.60;
uniform float uFlareGain = 1.0;
uniform float uWakeHue = 0.30;
uniform float uWakeTarget = 0.19;
uniform float uPulse = 0.0;
uniform float uPulseGain = 1.0;
uniform float uRareHue = 0.45;
uniform float uWakeFire = 0.10;
uniform sampler2D uWisp : filter_linear, repeat_disable;
uniform float uGasScale = 2.1;
uniform float uGasWarp = 2.6;
uniform float uGasFreq = 1.0;
uniform float uGasCov = 0.40;
uniform float uGasShoulder = 0.30;
uniform float uIslFreq = 0.8;
uniform float uIslCov = 0.42;
uniform float uIslFloor = 0.08;
uniform int uSlices = 5;
uniform float uGasOccl = 1.4;
uniform float uZShift = 0.03;
uniform float uGasGain = 8.5;
uniform float uVeinFreq = 3.0;
uniform float uVeinSharp = 4.5;
uniform float uVeinAmt = 5.2;
uniform float uBodyFall = 2.4;
uniform float uHaze = 0.06;
uniform float uGrainF = 5.5;
uniform float uHot0 = 0.32;
uniform float uHotAmt = 0.65;
uniform float uHueFreq = 0.55;
uniform float uSpreadIdle = 0.85;
uniform float uSpreadHover = 0.18;
uniform float uPGam = 1.7;
uniform float uWispAmt = 0.5;
uniform float uWispPow = 1.6;
uniform float uWispNorm = 0.36;
uniform float uDisp = 0.0016;
uniform float uWispScale = 1.0;
uniform float uGlassScale = 1.0;
uniform int uGlassLayers = 5;
uniform float uPresence = 1.0;
uniform float uRefr = 1.0;
uniform float uSRefr = 0.45;
uniform float uGlint = 0.5;
uniform float uFlash = 0.30;
uniform float uPaneTilt = 0.85;
uniform float uPaneRamp = 0.55;
uniform float uStarAmt = 1.0;
uniform float uSCurve = 0.55;
uniform float uMatZoom = 3.0;
uniform float uPaneDamp = 0.55;
uniform float uGlassPar = 0.6;
uniform float uPaneAbsorb = 0.30;
uniform int uCleaveFams = 3;
uniform float uIOR = 1.45;
uniform float uDepth = 0.35;
uniform float uGlassDepth = 1.0;
uniform float uWispDepth = 0.55;
uniform float uShared = 1.0;
uniform float uPerspective = 1.0;
uniform float uAmbient = 0.035;
uniform float uGlintFloor = 0.02;
uniform float uDisperse = 0.22;
uniform float uDisperseGain = 1.0;
uniform float uFlowHeat = 1.6;
uniform float uFlowHue = 0.10;
uniform float uFlowShear = 3.0;
uniform float uFaceKey = 1.0;
uniform float uFaceFill = 0.28;
uniform float uFaceFloor = 0.10;
uniform float uFacePow = 1.5;
uniform float uSurfGain = 0.35;
uniform float uSurfPow = 42.0;
uniform float uSurfRough = 6.0;
uniform float uSurfRim = 0.06;
uniform float uSeamGlow = 0.9;
uniform float uSeamCore = 1.6;
uniform float uSeamDisp = 0.30;
uniform float uSeamRide = 0.8;
uniform float uSeamGrain = 2.6;
uniform float uSeamRidePow = 1.7;
uniform float uVertexR = 0.10;
uniform float uVertexGain = 1.1;
uniform float uFocus = 0.55;
uniform float uFocusIn = 0.25;
uniform float uFocusOut = 1.05;
uniform float uVeinStream = 0.55;
uniform float uVeinFlowScale = 0.85;
uniform float uVeinMajor = 0.38;
uniform float uVeinHier = 0.62;
uniform float uEyeOn = 0.0;
uniform vec2 uEyeC = vec2(0.9, 0.5);
uniform vec2 uEyeLook = vec2(0.0);
uniform float uEyeW = 0.22;
uniform float uEyeH = 0.085;
uniform float uEyeOpen = 1.0;
uniform float uEyeLidLow = 0.62;
uniform float uEyeR = 0.115;
uniform float uEyeAng = 0.85;
uniform float uEyeRad = 0.55;
uniform float uEyePupil = 0.34;
uniform float uEyeEdge = 0.012;
uniform float uEyeLidW = 0.010;
uniform float uEyeLidGain = 2.2;
uniform float uEyeVtx = 1.6;
uniform float uEyeLimbal = 0.75;
uniform float uEyeSclera = 0.35;
uniform float uEyeHue = 0.62;
uniform float uEyeSpread = 0.10;
uniform float uEyeRampLo = 0.28;
uniform float uEyeLidShade = 0.55;
uniform float uEyeShadeD = 0.42;
uniform float uEyeSkew = 0.16;
uniform float uCalm = 0.0;
uniform float uSocket = 0.8;
uniform float uSocketIn = 1.0;
uniform float uSocketOut = 3.4;
uniform float uFlowDepth = 1.4;
uniform float uFlowSmooth = 0.045;
uniform float uFlowLead = 0.35;

%s
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
vec3 pal(float t){ return texture(uPal, vec2(t, 0.5)).rgb; }

// ── measured normalisation ───────────────────────────────────────────────────
// fb() has a MEASURED mean of 0.3854 and standard deviation 0.1196 — it is
// NOT uniform on 0..1 centred at 0.5. Every signed field derived as
// (fb - 0.5) was therefore biased low and compressed, which is why the colour
// window could never leave the blue end of the palette. Anything that needs a
// signed value uses this instead.
float nfb(vec2 p){ return clamp((fb(p) - 0.3854) / 0.24, -1.2, 1.2); }

// ridged fbm — branching vein crests, sharp and connected
float rfb(vec2 p){
	float v = 0.0; float a = 0.55;
	for(int i=0;i<3;i++){
		float n = 1.0 - abs(2.0 * nz(p) - 1.0);
		v += a * n * n;
		p = p * 2.13 + vec2(4.7, 9.3);
		a *= 0.5;
	}
	return v;
}

// mirrored tiling: facets rotate and offset their sampling of the baked wisp
// field, so a clamped or repeated fetch would either smear an edge or show a
// seam. Mirroring folds the coordinate back on itself instead.
float wispAt(vec2 uv){
	vec2 m = abs(fract(uv * 0.5) * 2.0 - 1.0);
	return texture(uWisp, m).r;
}

// ── THE GAS ──────────────────────────────────────────────────────────────────
// Emissive volume in depth slices. Each slice parallaxes at its own rate,
// which is what puts the light INSIDE the stone rather than on its surface.
// One shared hue field means every slice agrees on the local colour: regions
// hold a single hue family instead of muddying each other.
// ── THE CURRENT ──────────────────────────────────────────────────────────────
// Ridged noise is ISOTROPIC: it has no preferred direction anywhere, so every
// filament wanders on its own and the fire comes out as uniform squiggles with
// no design. Real fire in a stone RUNS — it follows currents. This is the same
// divergence-free curl field the wisps are baked from, so the veins and the
// wisps finally belong to one flow instead of being two unrelated scribbles.
float veinPsi(vec2 p){
	return nz(p) + 0.5 * nz(p * 2.13 + vec2(3.0, 8.0));
}
vec2 veinCurl(vec2 p){
	float e = 0.06;
	float dx = (veinPsi(p + vec2(e, 0.0)) - veinPsi(p - vec2(e, 0.0))) / (2.0 * e);
	float dy = (veinPsi(p + vec2(0.0, e)) - veinPsi(p - vec2(0.0, e))) / (2.0 * e);
	vec2 v = vec2(dy, -dx);
	return v / max(length(v), 1e-5);
}
// Walk the sampling coordinate along the current before reading the vein
// field. Two steps is enough to turn scribble into streamers: the filaments
// stretch along the flow, converge where it converges, and stream in bands.
vec2 veinAdvect(vec2 p, float scale, float amt){
	vec2 q = p;
	q += veinCurl(q * scale) * amt;
	q += veinCurl(q * scale * 1.7 + vec2(5.0, 2.0)) * amt * 0.55;
	return q;
}

vec3 gasAt(vec2 uvRaw, vec2 view, vec2 uViewShift, vec2 flowSlow, vec2 flowFast, float t, float hue0, float spread, out float densMax, out float hueOut){
	// one zoom, applied before anything else, so the gas / island / hue
	// frequency RATIOS stay exactly as calibrated
	vec2 uv = uvRaw * uMatZoom;
	vec3 col = vec3(0.0);
	float occl = 0.0;
	densMax = 0.0;
	float isl = fb(uv * uIslFreq + vec2(131.0, 61.0));
	isl = pow(clamp((isl - uIslCov) / 0.26, 0.0, 1.0), 0.8);
	isl = uIslFloor + (1.0 - uIslFloor) * isl;
	float hueF = hue0 + nfb(uv * uHueFreq + vec2(3.0, 71.0)) * spread * 0.5;
	hueOut = hueF;
	// the current is a property of the stone, not of a slice — compute the
	// displacement once and let every depth share it, so the layers stream
	// together instead of each inventing its own direction
	vec2 flowOfs = vec2(0.0);
	if (uVeinStream > 0.0001){
		flowOfs = veinAdvect(uv, uVeinFlowScale, uVeinStream) - uv;
	}
	int n = uSlices;
	for (int i = 0; i < n; i++){
		float z = float(i) / max(float(n - 1), 1.0);
		// z: 0 = deepest, 1 = just under the surface. A layer at the surface
		// does not shift at all; the deepest one shifts most. That gradient
		// IS the sense of looking into a volume.
		vec2 par = uViewShift * (1.0 - z) + view * 0.12;
		vec2 s = uv * uGasScale * (1.0 + 0.45 * z) + par + vec2(float(i) * 7.31, float(i) * 3.77);
		// ── VOLUMETRIC FLOW ───────────────────────────────────────────────
		// Every depth was being shoved by the same offset, so a stroke slid
		// one flat sheet. A finger in a thick liquid does not do that: the
		// near fluid answers immediately, the deep fluid lags and keeps
		// moving after you stop.
		//
		// The two fields we already run give exactly that for free. uVel is
		// instantaneous — this frame's motion. uOff is an accumulator, a
		// smeared record of where the flow has been. So the FRONT slices
		// follow the velocity and snap, the DEEP slices follow the
		// accumulator and drag. The layers slide over each other.
		vec2 fl = mix(flowSlow, flowFast, pow(z, uFlowDepth));
		s += fl;
		float w1 = fb(s * 0.55 + vec2(11.0, 5.0));
		float w2 = fb(s * 0.55 + vec2(47.0, 29.0));
		vec2 p = s + uGasWarp * (vec2(w1, w2) - 0.5);
		// CALM. Raising the coverage threshold removes fire rather than dimming
		// it, which is the difference between a quiet face and a grey one.
		float covq = uGasCov + uCalm * 0.22;
		float d = clamp((fb(p * uGasFreq) - covq) / uGasShoulder, 0.0, 1.0);
		// sampled along the CURRENT, so the veins stream instead of scribble
		vec2 vp = p + flowOfs * uGasScale;
		// HIERARCHY: major arteries at a coarse scale, capillaries only where
		// an artery already runs. Uniform-width veins everywhere is the other
		// half of why the fire had no design — real structure has a few
		// dominant forms with detail subordinate to them.
		float major = clamp(rfb(vp * uVeinFreq * uVeinMajor + vec2(91.0, 17.0)) * 1.15, 0.0, 1.0);
		float minor = clamp(rfb(vp * uVeinFreq + vec2(37.0, 61.0)) * 1.15, 0.0, 1.0);
		float ridge = pow(major * (1.0 - uVeinHier + uVeinHier * minor), uVeinSharp);
		float dens = pow(d, 1.6);
		densMax = max(densMax, dens * (0.25 + 1.05 * ridge) * isl);
		float grain = 0.70 + 0.60 * fb(p * uGrainF + vec2(7.0, 19.0));
		float e = (uVeinAmt * ridge * dens * grain + pow(dens, uBodyFall) * 0.30 + dens * uHaze) * isl;
		vec3 c = pow(pal(hueF + z * uZShift), vec3(uPGam));
		col += c * e * uGasGain * (1.0 - uCalm * 0.72) / float(n) * (1.0 - occl);
		occl = clamp(occl + dens * uGasOccl / float(n), 0.0, 0.85);
	}
	return col;
}

// The five fracture sheets. A function rather than global const arrays —
// array constants at global scope are not portable across Godot versions.
void sheet(int i, out float c, out float st, out float rot, out float pres, out float refr, out float ew){
	c = 1.15; st = 1.45; rot = 0.20; pres = 0.34; refr = 0.150; ew = 0.034;
	if (i == 1){ c = 2.4;  st = 1.55; rot = 0.42; pres = 0.62; refr = 0.115; ew = 0.030; }
	else if (i == 2){ c = 4.6;  st = 1.7; rot = 1.35; pres = 0.52; refr = 0.065; ew = 0.024; }
	else if (i == 3){ c = 9.0;  st = 1.6; rot = 2.35; pres = 0.46; refr = 0.034; ew = 0.018; }
	else if (i == 4){ c = 17.0; st = 1.8; rot = 0.95; pres = 0.38; refr = 0.016; ew = 0.014; }
}

// ── CLEAVAGE, not Voronoi ────────────────────────────────────────────────────
// Jittered Voronoi gives organic blobs with wandering edges — the opposite of
// a crystal. A real stone fractures along CLEAVAGE PLANES: a small number of
// discrete orientations, each a family of parallel flat planes. Intersecting
// two or three such families partitions the plane into rhombi, triangles and
// hexagons with dead-straight edges, which is where the geometric beauty of a
// cut stone comes from.
void cleave(vec2 p, float sp, int fams, out vec2 id, out float edge){
	float best = 1e9;
	float i0 = 0.0;
	float i1 = 0.0;
	int nf = max(fams, 1);
	for (int k = 0; k < 4; k++){
		if (k >= nf) break;
		// families spread evenly over 180 degrees: 3 gives the hexagonal
		// rhombic tiling a real crystal shows
		float a = float(k) * 3.14159265 / float(nf);
		vec2 n = vec2(cos(a), sin(a));
		// each family gets its own spacing, so the tiling never reads as
		// graph paper
		float s = sp * (0.74 + 0.52 * h1(vec2(float(k) * 7.3, 2.1)));
		float d = dot(p, n) / s;
		float b = floor(d);
		float f = d - b;
		// distance to the nearest plane of this family, back in p units
		best = min(best, min(f, 1.0 - f) * s);
		i0 += b * (1.0 + float(k) * 3.0);
		i1 += b * (2.0 + float(k) * 5.0);
	}
	id = vec2(i0, i1);
	edge = best;
}

void voro(vec2 p, out vec2 id, out float f1, out float f2){
	vec2 ip = floor(p); vec2 fp = fract(p);
	f1 = 8.0; f2 = 8.0; id = ip;
	for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++){
		vec2 o = vec2(float(x), float(y));
		vec2 h = h2(ip + o);
		vec2 c = o + h - fp;
		float d = dot(c,c);
		if (d < f1){ f2 = f1; f1 = d; id = ip + o; }
		else if (d < f2){ f2 = d; }
	}
	f1 = sqrt(f1); f2 = sqrt(f2);
}

// only edges flagged interior (w == 1) can carry a seam
float cutSDF(vec2 p){
	float s = -9.0;
	s = max(s, mix(-9.0, dot(uCut0.xy, p) - uCut0.z, uCut0.w));
	s = max(s, mix(-9.0, dot(uCut1.xy, p) - uCut1.z, uCut1.w));
	s = max(s, mix(-9.0, dot(uCut2.xy, p) - uCut2.z, uCut2.w));
	s = max(s, mix(-9.0, dot(uCut3.xy, p) - uCut3.z, uCut3.w));
	s = max(s, mix(-9.0, dot(uCut4.xy, p) - uCut4.z, uCut4.w));
	s = max(s, mix(-9.0, dot(uCut5.xy, p) - uCut5.z, uCut5.w));
	return s;
}
// The two smallest distances to this facet's interior cut planes. One small
// distance means you are on a SEAM; two small distances at once means you are
// at a VERTEX where edges meet — which is where a real stone throws its
// brightest flare, and the thing that gives the layout its architecture.
void cutPair(vec2 p, out float d1, out float d2){
	d1 = 9.0; d2 = 9.0;
	for (int i = 0; i < 6; i++){
		vec4 c = uCut0;
		if (i == 1) c = uCut1;
		else if (i == 2) c = uCut2;
		else if (i == 3) c = uCut3;
		else if (i == 4) c = uCut4;
		else if (i == 5) c = uCut5;
		if (c.w < 0.5) continue;
		float d = abs(dot(c.xy, p) - c.z);
		if (d < d1){ d2 = d1; d1 = d; }
		else if (d < d2){ d2 = d; }
	}
}

vec2 cutGrad(vec2 p){
	float e = 0.0025;
	vec2 g = vec2(
		cutSDF(p + vec2(e,0.0)) - cutSDF(p - vec2(e,0.0)),
		cutSDF(p + vec2(0.0,e)) - cutSDF(p - vec2(0.0,e)));
	return normalize(g + vec2(1e-6, 1e-6));
}

void fragment(){
	vec2 suv = SCREEN_UV;
	vec2 A = vec2(uAsp, 1.0);
	vec2 pw = suv * A;

	float sd = cutSDF(pw);
	vec2 gN = cutGrad(pw);
	float rimD = clamp(1.0 - smoothstep(0.0, uSeamW, -sd), 0.0, 1.0);

	vec2 flow = bl2(uVel, suv, uTxS);
	// THE CLEAR-AT-REST GATE. The offset field is an accumulator covering the
	// whole viewport, so applying it ungated warps everything forever — which
	// reads as one rectangular sheet deforming. Gating by LOCAL flow speed
	// means the stone is glass-clear wherever the fluid has settled, and only
	// distorts where the cursor has just been.
	// DYE IS THE MASK. The velocity field is global — incompressible flow
	// couples the whole domain instantly — so gating on velocity could never
	// be local. Dye is injected at the cursor and carried by that field, so it
	// marks where the fluid actually IS. Outside it, the stone is untouched.
	float dye = clamp(bl1(uDye, suv, uTxS) * uDyeGain, 0.0, 1.0);
	float act = mix(uRestFlow, 1.0, smoothstep(uActLo, uActHi, dye));
	vec2 eb = min(suv, vec2(1.0) - suv);
	act *= smoothstep(0.0, max(uEdgeFade, 0.001), min(eb.x, eb.y));
	vec2 off = bl2(uOff, suv, uTxS) * uFlu * 0.9 * act;

	float ca = cos(uFAng); float sa = sin(uFAng);
	mat2 R = mat2(vec2(ca, -sa), vec2(sa, ca));

	vec2 flowR = R * flow;
	float fmag = length(flowR);
	float fw = smoothstep(0.02, 0.40, fmag);
	vec2 fdir = mix(vec2(cos(uT*0.05), sin(uT*0.05)), flowR / max(fmag, 1e-4), fw);
	float flowAng = atan(fdir.y, fdir.x);

	// ── this face's angle to the light ──
	vec2 tiltT = uFTilt + uView * uTiltResp * 2.0;
	vec3 Nf = normalize(vec3(-tiltT.x, -tiltT.y, 1.0));
	vec3 Ld = normalize(uLight);
	vec3 Hv = normalize(Ld + vec3(0.0, 0.0, 1.0));
	vec3 Ld2 = normalize(uLight2);
	vec3 Hv2 = normalize(Ld2 + vec3(0.0, 0.0, 1.0));
	// key + fill. With one lamp the far side of the stone is permanently dark
	// however much you turn it.
	float key = max(dot(Nf, Ld), 0.0) * uDiffGain;
	float fil = max(dot(Nf, Ld2), 0.0) * uFill;
	float diff = uDiffFloor + (1.0 - uDiffFloor) * clamp(key + fil, 0.0, 1.0);
	float spec = pow(max(dot(Nf, Hv), 0.0), uSpecPow) * uSpecGain
		+ pow(max(dot(Nf, Hv2), 0.0), uSpecPow) * uSpecGain * uFill;

	// ── LOOKING INTO THE STONE ────────────────────────────────────────────
	// The interior is seen THROUGH this facet's surface, and that surface
	// bends the line of sight. Without this every face slides its interior by
	// the same global offset, which is a flat stack viewed from directly
	// above — the "sliding on ice" read. Refracting the view through Nf gives
	// each face its own angle into the material, so the same interior appears
	// displaced differently on every facet, the displacement grows with
	// depth, and it swings as the stone turns.
	// ── OPTICAL DISTURBANCE ───────────────────────────────────────────────
	// The flow moved the material but never the OPTICS. Disturbing the
	// surface normal with the velocity field instead means the stone's own
	// lensing wobbles: the interior swims behind glass that is being stirred,
	// the refracted line of sight bends as the flow crosses it, and the
	// surface highlight ripples. This is where "looking through disturbed
	// water" lives, and it only works because there is real depth behind it.
	// NOTE — do not add an optical bend driven by dye or velocity here.
	// Tried three times, reverted three times. Both fields are injected as a
	// blob around the cursor, so any per-pixel GRADIENT of them is concentric
	// near the injection point: the result is always a ring of distortion
	// following the mouse, and because the gradient is instantaneous it also
	// jitters per frame instead of carrying any weight. Weight comes from
	// large-scale, slow displacement — see the smoothed flow below.
	vec3 Vi = vec3(0.0, 0.0, -1.0);
	vec3 Rr = refract(Vi, Nf, 1.0 / max(uIOR, 1.0));
	vec2 viewShift = vec2(0.0);
	if (dot(Rr, Rr) > 0.0001){
		// screen displacement per unit of depth into the stone
		viewShift = Rr.xy / max(-Rr.z, 0.15) * uDepth;
	}

	vec2 p = pw - vec2(off.x*uAsp, off.y);
	p = R * p + uFOfs + uFTilt * 0.35;
	p += gN * rimD * rimD * uRefract * 0.075;

	float t = uT;
	vec2 view = R * (uView * 0.55 * uParGain * (1.0 + length(uFTilt) * uTiltPar));

	// ── the colour window ──
	// Each bezel owns a window CENTRE on the shared palette (identity_hue).
	// Idle: the window is wide and the whole face runs the full spectrum.
	// Hover: it collapses to a narrow band — attention makes a face declare
	// its identity, rather than identity being permanent paint.
	float hueA = uWakeTarget + uHardHue * 0.01 * uHard;
	float spreadEff = mix(uSpreadIdle, uSpreadHover, uFWake);

	// ── THE GLASS: sparse fractured sheets at five depths ─────────────────
	// Each sheet is an INCOMPLETE Voronoi plane: only some cells exist. A
	// present cell bends the light behind it, catches the lamp with its own
	// normal, and shows a gradient across its face. Because the sheets are
	// incomplete and overlap, no continuous lattice ever forms — you cannot
	// tell where one shard ends, which is the layered-gem read.
	vec2 gOff = vec2(0.0);
	vec2 wOff = vec2(0.0);
	float trans = 1.0;
	float glint = 0.0;
	vec3 edgeCol = vec3(0.0);
	vec3 flash = vec3(0.0);
	for (int li = 0; li < 5; li++){
		if (li >= uGlassLayers) break;
		float lc; float lst; float lrot; float lpres; float lrefr; float lew;
		sheet(li, lc, lst, lrot, lpres, lrefr, lew);
		lc *= uGlassScale;
		lpres *= uPresence;
		float cr = cos(lrot); float sr = sin(lrot);
		// FACET SPACE, not screen space. Computed in screen space the fracture
		// sheets are one pattern shared by all six faces, continuous across
		// every seam and dead still when the stone turns — which is exactly
		// what an overlay looks like. In facet space each face owns its own
		// crystal, the cut orientation snaps at the seams, and the sheets
		// parallax as the stone moves. The fluid offset is deliberately NOT
		// included: the gas moves inside the stone, the stone does not flow.
		// each sheet is at a different DEPTH in the stone, so each is displaced
		// by a different amount through the refracting surface. The finest
		// sheet sits at the surface and barely moves (surface crazing); the
		// coarse ones are deep and swim as the stone turns. That differential
		// is what separates the layers in motion.
		float sheetDepth = 1.0 - float(li) / 4.0;
		vec2 gp = R * pw + uFOfs + uFTilt * 0.35
			+ view * uGlassPar * 0.25
			+ viewShift * sheetDepth * uGlassDepth;
		vec2 rp = vec2(gp.x * cr + gp.y * sr, -gp.x * sr + gp.y * cr);
		vec2 vid; float vEdge;
		cleave(vec2(rp.x + float(li) * 23.1, rp.y * lst + float(li) * 41.7),
			1.0 / max(lc, 0.001), uCleaveFams, vid, vEdge);
		float hp = h1(vid + vec2(6.6, 1.9));
		float pm = step(hp, lpres);
		vec2 jj = h2(vid + vec2(31.0, 17.0));
		// refraction: constant per shard. A gradient here shreds the image —
		// the coordinate is already inside the partition.
		vec2 dl = (jj - 0.5) * lrefr * uRefr;
		vec2 dd = vec2(dl.x * cr - dl.y * sr, dl.x * sr + dl.y * cr) * pm;
		gOff += dd;
		// THIN features take refraction from the COARSE sheets only. A fine
		// shard displaces a filament by less than its own width in a random
		// direction, which minces it; a big pane shifts whole runs sideways,
		// which is what glass does.
		if (li < 3) wOff += dd * uSRefr;
		// ── INTERNAL structure, not surface shading ──
		// The previous version lit each pane with dot(normal, light) plus a
		// brightness gradient across its face. That is a SURFACE shading
		// model — it is how you shade a bump — so the shards read as bumps on
		// the outside of the stone no matter how far they are turned down.
		//
		// A fracture plane INSIDE a crystal is invisible except for what it
		// does to the light passing through it: it displaces the image behind
		// it (already handled above, as refraction), it absorbs a little, and
		// its rim catches a thin bright line. None of those depend on a
		// surface normal facing a lamp.
		vec2 np = h2(vid + vec2(55.0, 71.0));
		float damp = pow(uPaneDamp, float(li));
		// absorption: some panes are very slightly cloudier than others. No
		// light direction — a pane deep in the stone does not know where the
		// lamp is.
		float absorb = 1.0 + (np.x - 0.5) * 2.0 * uPaneAbsorb * damp;
		// the legacy surface term, off by default. Raise pane_tilt only if you
		// deliberately want the outer face to read as faceted relief.
		float surf = 1.0;
		if (uPaneTilt > 0.001){
			vec2 nrm2 = (np - 0.5) * 2.0 * uPaneTilt;
			float nl = sqrt(dot(nrm2, nrm2) + 1.0);
			float lam = clamp((nrm2.x * Ld.x + nrm2.y * Ld.y + Ld.z) / nl, 0.0, 1.0);
			vec2 cen = vec2((vid.x + np.x) / lc, (vid.y + np.y) / (lc * lst));
			vec2 gv = clamp(vec2((rp.x - cen.x) * lc, (rp.y - cen.y) * lc * lst), vec2(-0.7), vec2(0.7));
			float ramp = 1.0 + uPaneRamp * (gv.x * Ld.x + gv.y * Ld.y);
			surf = 1.0 + ((0.55 + 1.15 * pow(lam, 1.6)) * ramp - 1.0) * damp;
		}
		trans *= mix(1.0, clamp(absorb * surf, 0.25, 2.4), pm);
		// broken edge light: present cells only, and faded along the edge by
		// noise so no join ever draws as a continuous line
		float ed = pow(1.0 - clamp(vEdge / lew, 0.0, 1.0), 2.0);
		// PRISMATIC FRINGE. A cut edge splits light — the hue sweeps across
		// the edge profile rather than the whole edge being one bright line.
		// This is most of what makes a cut stone look edible, and it is the
		// thing the neutral white joins were missing.
		float acrossEdge = clamp(vEdge / max(lew, 1e-4), 0.0, 1.0);
		edgeCol += pal(hueA + acrossEdge * uDisperse + float(li) * 0.07)
			* ed * pm * uGlint * damp;
		float br = fb(rp * 3.1 + float(li) * 9.0);
		glint += ed * pm * clamp((br - 0.38) / 0.34, 0.0, 1.0) * uGlint * (1.0 - 0.16 * float(li)) * damp;
		// a pane occasionally catches the light whole — the visible layer
		float pf = clamp((hp - 0.80) / 0.20, 0.0, 1.0) * pm * ((li < 2) ? 0.55 : 0.3);
		flash += (pal(hueA + (h1(vid + vec2(2.1, 8.8)) - 0.5) * 0.5) * 0.65 + 0.35) * pf * uFlash * damp;
	}

	// ── THE GAS behind the glass ──────────────────────────────────────────
	// The fluid drags the whole volume: stirring shears the gas and the wisps
	// together, and it settles when the dye decays.
	// The offsets are DISPLACEMENTS in screen units, calibrated as such. gasAt
	// multiplies its input by uMatZoom, so passing them raw amplifies every
	// refraction by the zoom: each shard then samples a completely unrelated
	// region of the gas and of the hue field instead of a slightly shifted
	// one, and the face turns into confetti. Divide them out first.
	// ── ONE STONE, NOT SIX PICTURES ───────────────────────────────────────
	// p carries this facet's own rotation and pattern_offset, so sampling the
	// interior with it gives every bezel an unrelated image and the seams read
	// as borders between different pictures. The INTERIOR of a real stone is
	// one continuous body: the formations, the voids and the colour zones
	// belong to the stone, not to the face you happen to be looking through.
	//
	// So the gas is sampled in STONE space (pw — shared by every facet), and
	// the only thing that differs per face is how its surface bends the view:
	// viewShift, which is already per-facet. The fire therefore runs across
	// the whole gem and BREAKS at each seam because each face refracts it
	// differently — which is what a cut stone does, and what the reference
	// shows. uShared blends back toward per-facet space if it ever needs it.
	vec2 stoneP = mix(p, pw, uShared);

	// ── THE EYE ───────────────────────────────────────────────────────────
	// Not a drawn eye. A REGION OF THE STONE that behaves like one.
	//
	// A flat vector eye — radial fan, black limbal ring, hexagon pupil — has
	// none of what makes this material work: no sparse cleavage, no layered
	// depth, no refraction between layers, no dispersion, no real black. It
	// would sit on the stone rather than in it, and it would drift further
	// from the material with every improvement made to the material.
	//
	// So instead the sampling coordinate is warped into POLAR form inside an
	// aperture. The same gas, veins, sheets, dispersion and depth then run
	// radially out from a point, and the result is an iris made of opal
	// rather than an iris drawn on top of opal. Everything below falls out of
	// that one decision:
	//   the LIMBAL RING is where the warp ends and normal stone resumes
	//   the PUPIL is a region where the material's density is crushed to zero
	//   the LIDS are cleavage planes, lit by the same lamp as every facet
	float eyeIn = 0.0;
	float eyeR = 0.0;
	float eyeLimb = 0.0;
	float eyeLidShade = 1.0;
	float lidCore = 0.0;
	float lidVtx = 0.0;
	if (uEyeOn > 0.5){
		vec2 eq = (pw - uEyeC * A);
		// the APERTURE: two cleavage planes meeting at points. Straight
		// edges, because that is what this stone does — a curved lid would
		// be the only curve anywhere in the material.
		float hu = uEyeH * mix(0.04, uEyeLidLow, uEyeOpen);
		float hl = uEyeH * mix(0.04, 1.0, uEyeOpen) * 0.72;
		// ASYMMETRY. A symmetrical lens is a decorative shape; an eye has an
		// inner corner and an outer one, and they are not alike. Skewing
		// where the apex sits is what stops it reading as an ornament.
		float sx = eq.x + uEyeSkew * uEyeW;
		float ax = abs(sx) / max(uEyeW, 1e-4);
		float upper = -hu * (1.0 - ax);
		float lower = hl * (1.0 - ax);
		// positive outside the aperture, negative within it
		float esd = max(upper - eq.y, eq.y - lower);
		if (ax > 1.0) esd = max(esd, (ax - 1.0) * uEyeH);
		eyeIn = 1.0 - smoothstep(-uEyeEdge, uEyeEdge * 0.35, esd);

		// the lash line: the cut where the lid plane meets the aperture,
		// treated exactly like a seam because that is what it is
		lidCore = pow(1.0 - clamp(abs(esd) / max(uEyeLidW, 1e-4), 0.0, 1.0), 2.4);
		// and the corners, where the two planes meet
		vec2 c1 = eq - vec2(uEyeW - uEyeSkew * uEyeW, 0.0);
		vec2 c2 = eq + vec2(uEyeW + uEyeSkew * uEyeW, 0.0);
		float vr = uEyeLidW * 2.6;
		lidVtx = exp(-dot(c1, c1) / (vr * vr)) + exp(-dot(c2, c2) / (vr * vr));

		if (eyeIn > 0.002){
			// gaze moves the IRIS inside a fixed aperture — the opening is
			// cut in the stone and cannot travel
			vec2 iq = eq - uEyeLook * uEyeR;
			eyeR = length(vec2(iq.x, iq.y / max(uEyeH / uEyeW, 0.25))) / max(uEyeR, 1e-4);
			float ang = atan(iq.y, iq.x);
			// polar: angle becomes one axis, radius the other, so the fire
			// runs outward from the pupil in fibres
			vec2 eyeP = uEyeC * A + vec2(ang * uEyeAng, eyeR * uEyeRad);
			// fade the warp back into normal stone at the limbus, so there
			// is no seam where the iris ends
			eyeLimb = 1.0 - smoothstep(0.88, 1.06, eyeR);
			stoneP = mix(stoneP, eyeP, eyeIn * eyeLimb);
			// SHADOW UNDER THE UPPER LID. An opening with no shadow beneath
			// its edge reads as a hole cut in paper. This is what gives the
			// aperture a lip and puts the eyeball behind it.
			float below = (eq.y - upper) / max(uEyeH, 1e-4);
			eyeLidShade = mix(1.0 - uEyeLidShade, 1.0, clamp(below / max(uEyeShadeD, 0.01), 0.0, 1.0));
		}
	}
	// ── FORESHORTENING ────────────────────────────────────────────────────
	// The stone is a projected solid: when it turns, each face compresses
	// along its tilt direction. Sampled in flat screen space the interior
	// keeps a fixed scale no matter what the face does, so the geometry moves
	// and its contents do not — which is the static feeling. A plane seen at
	// an angle compresses by its normal's z, so compress the sampling
	// coordinate the same way and the interior belongs to the surface.
	if (uPerspective > 0.001){
		vec2 tdir = tiltT;
		float tl = length(tdir);
		if (tl > 0.0005){
			tdir /= tl;
			vec2 tperp = vec2(-tdir.y, tdir.x);
			float along = dot(stoneP, tdir);
			float across = dot(stoneP, tperp);
			// dividing by Nf.z stretches the sampled field along the tilt,
			// which reads on screen as the interior compressing with the face
			float fore = mix(1.0, 1.0 / max(Nf.z, 0.25), uPerspective);
			stoneP = tdir * (along * fore) + tperp * across;
		}
	}
	vec2 gasUV = stoneP + gOff / max(uMatZoom, 0.001);
	float densMax = 0.0;
	float hueF = hueA;
	// slow = the accumulator's smeared history (deep, laggy)
	// fast = this frame's velocity (near the surface, immediate)
	// ── WEIGHT ────────────────────────────────────────────────────────────
	// A stroke should move a MASS of fluid, not vibrate the pixels under the
	// cursor. Mass means large scale and slow: sampling the accumulator at a
	// spread of points averages away the fine, fast structure and leaves the
	// broad motion, so the interior shifts as one body with momentum instead
	// of jittering locally. The radius is what "heavy" is made of here.
	float sr = uFlowSmooth;
	vec2 oS = bl2(uOff, suv, uTxS)
		+ bl2(uOff, suv + vec2(sr, 0.0), uTxS)
		+ bl2(uOff, suv - vec2(sr, 0.0), uTxS)
		+ bl2(uOff, suv + vec2(0.0, sr), uTxS)
		+ bl2(uOff, suv - vec2(0.0, sr), uTxS)
		+ bl2(uOff, suv + vec2(sr, sr) * 0.7, uTxS)
		+ bl2(uOff, suv - vec2(sr, sr) * 0.7, uTxS)
		+ bl2(uOff, suv + vec2(sr, -sr) * 0.7, uTxS)
		+ bl2(uOff, suv - vec2(sr, -sr) * 0.7, uTxS);
	oS *= uFlu * 0.9 * act / 9.0;
	vec2 flowSlow = vec2(oS.x * uAsp, oS.y) * 2.2 * uFlowShear * uMatZoom;
	// the "fast" layer is the same smoothed field, only less of it — the
	// front of the volume leads, but nothing here is instantaneous
	vec2 flowFast = flowSlow * uFlowLead;
	// HUE DISCIPLINE. The iris was taking the whole palette, which is why it
	// read as polychrome confetti rather than as an eye. A real iris is one
	// colour with a tonal range, so inside the aperture the window collapses
	// to a narrow band around his own hue — the same mechanism the bezels use
	// on hover, pointed at him instead.
	float eyeMask = eyeIn * eyeLimb;
	float hueUse = mix(hueA, uEyeHue, eyeMask);
	float spreadUse = mix(spreadEff, uEyeSpread, eyeMask);
	vec3 col = gasAt(gasUV, view, viewShift * uMatZoom, flowSlow, flowFast,
		t, hueUse, spreadUse, densMax, hueF);
	col *= trans;
	// A dim interior body, so the CUT is readable everywhere rather than only
	// where fire happens to be. Without it the void is empty and every
	// refraction, pane and cleavage plane is invisible over most of the gem.
	// It is multiplied by trans, so pane-to-pane differences show even in the
	// dark — the stone reads as a stone before anything lights up inside it.
	col += pow(pal(hueF + 0.45), vec3(uPGam)) * uAmbient * trans;
	// the edge light needs a floor too, or joins vanish against black
	col += (col * 1.6 + vec3(uGlintFloor)) * glint;
	// the prismatic fringe rides on top, brightest where there is fire behind
	col += edgeCol * (0.35 + 0.85 * clamp(dot(col, vec3(0.4)), 0.0, 1.0)) * uDisperseGain;

	// ── THE FLUID, MADE LEGIBLE ───────────────────────────────────────────
	// Displacement alone is a weak signal: it asks the eye to notice a subtle
	// warp in a field it has never seen undisturbed. Dye is also HEAT — where
	// the stone has been stirred the fire brightens and the hue climbs, the
	// way breathing on an ember does. That reads instantly, and the shear
	// then reads as a consequence of it rather than as the whole effect.
	float heat = clamp(dye * uFlowHeat, 0.0, 3.0);
	if (heat > 0.001){
		col *= 1.0 + heat;
		col += pow(pal(hueF + uFlowHue), vec3(uPGam)) * heat * heat * 0.35 * densMax;
	}
	col += flash * (0.25 + 0.75 * clamp(dot(col, vec3(0.333)), 0.0, 1.0));

	// ── THE WISPS ─────────────────────────────────────────────────────────
	// Baked once by OpalCore. Three reads at slightly different offsets give
	// chromatic dispersion: glass bends red and blue by different amounts, so
	// a refracted filament fringes into colour at every break instead of
	// looking like a chopped line.
	vec2 wUV = (stoneP + viewShift * uWispDepth + (wOff + vec2(off.x * uAsp, off.y) * 2.2 * uFlowShear) / max(uMatZoom, 0.001)) * uWispScale * 0.14 * uMatZoom;
	float wR = pow(clamp(wispAt(wUV + vec2(uDisp, uDisp * 0.4)) / max(uWispNorm, 1e-4), 0.0, 1.0), uWispPow);
	float wG = pow(clamp(wispAt(wUV) / max(uWispNorm, 1e-4), 0.0, 1.0), uWispPow);
	float wB = pow(clamp(wispAt(wUV - vec2(uDisp, uDisp * 0.4)) / max(uWispNorm, 1e-4), 0.0, 1.0), uWispPow);
	vec3 wv = vec3(wR, wG, wB);
	float wamt = uWispAmt * (0.30 + 0.70 * clamp(densMax * 3.0, 0.0, 1.0));
	col += pow(pal(hueF + 0.02), vec3(1.5)) * wv * wamt;
	col += wv * wamt * 0.10;

	// ── hot knots ─────────────────────────────────────────────────────────
	float hot = pow(clamp((densMax - uHot0) / 0.10, 0.0, 1.0), 1.4);
	col += vec3(1.0, 0.97, 0.92) * hot * uHotAmt * (1.0 + heat * 1.5);

	// ── stars, clustered where the gas is dense ───────────────────────────
	if (uStarAmt > 0.001){
		for (int si = 0; si < 3; si++){
			float sc = (si == 0) ? 46.0 : ((si == 1) ? 23.0 : 11.0);
			float kk = (si == 0) ? 2600.0 : ((si == 1) ? 1400.0 : 700.0);
			float amp = (si == 0) ? 0.9 : ((si == 1) ? 0.7 : 0.55);
			vec2 sg = stoneP * sc;
			vec2 sidx = floor(sg);
			vec2 sh = h2(sidx + 19.7);
			vec2 sdv = fract(sg) - sh;
			float d2 = dot(sdv, sdv);
			float pstar = 0.06 + 0.80 * densMax;
			if (h1(sidx + vec2(3.0, 9.0)) < pstar && sh.x > 0.35){
				vec3 tint = pal(hueF + (sh.y - 0.5) * 0.25) * 0.5 + 0.5;
				col += tint * exp(-d2 * kk) * amp * uStarAmt;
				if (sh.y > 0.86) col += tint * exp(-d2 * kk * 0.12) * amp * 0.30 * uStarAmt;
			}
		}
	}

	// (the rock-matrix layer is retired with the mosaic fire — rock_matrix
	// described a host stone around opaque tiles, which no longer exist)

	// stone surface — dome, edge dispersion, sheen (screen-wide, not per facet)
	vec2 rv = (suv - 0.5) * A;
	float r2 = dot(rv, rv);
	col *= 1.0 - uDome * r2 * 0.55;
	float rimR = smoothstep(0.50, 0.95, sqrt(r2));
	col += pal(hueA + atan(rv.y, rv.x)/6.28318 * 0.6) * rimR * uRim * 0.22;
	vec2 sv = rv - uView*0.9;
	float sh1 = exp(-dot(sv,sv)*2.2);
	vec2 sv2 = sv - vec2(0.20,-0.13);
	float sh2 = exp(-dot(sv2,sv2)*11.0);
	col += vec3(0.85,0.90,1.0) * (sh1*0.05 + sh2*0.10) * uSheen;

	// how much light is present HERE. Nf is constant across a facet, so an
	// ungated specular lays a uniform milky wash over the entire face; it has
	// to ride the gas instead of the geometry.
	float fireP = clamp(0.08 + 1.6 * dot(col, vec3(0.2126, 0.7152, 0.0722)), 0.0, 1.0);

	// ── 1. THE KEY LIGHT ACROSS THE STONE ─────────────────────────────────
	// The only illumination was the fire itself, so all six faces sat at the
	// same brightness and the image had no value hierarchy — which is most of
	// why it reads as texture rather than as a photographed object. The lamps
	// and the true face normals already exist; this connects them to the new
	// material. Because the lamp is FIXED and the cursor turns the stone,
	// faces now swing through the light: one blazes while its neighbour falls
	// away, and the brightest face changes as you move.
	//
	// Deliberately NOT reusing light_contrast / diffuse_floor / fill_gain:
	// those names carry whatever a scene saved during old tuning, and this is
	// the term that decides whether the image has contrast at all.
	float fkey = max(dot(Nf, Ld), 0.0);
	float ffill = max(dot(Nf, Ld2), 0.0) * uFaceFill;
	float faceLit = uFaceFloor + (1.0 - uFaceFloor) * clamp(fkey * uFaceKey + ffill, 0.0, 1.0);
	faceLit = pow(faceLit, uFacePow);
	col *= faceLit;
	// highlight takes its colour from the stone, not from white. GATED by
	// fire presence: Nf is constant across a face, so ungated this was a
	// uniform milky wash over the whole facet — the sage fog. Highlights ride
	// the fire; the void stays void.
	col += mix(pal(hueA + 0.12), vec3(1.0), 0.25) * spec * (0.45 + 0.55 * diff) * fireP;

	// ── 3. THE POLISHED SURFACE ───────────────────────────────────────────
	// Until now you looked straight into the interior everywhere, with no
	// sense that the stone has an outside. A hard highlight raking across a
	// face as it turns is the strongest "this is a cut gem" cue there is, and
	// it gives the eye one bright simple thing to rest on among all the fire.
	// Ungated by the fire on purpose: a reflection sits ON the surface and is
	// brightest exactly where the interior is dark.
	if (uSurfGain > 0.0001){
		vec3 Hs1 = normalize(Ld + vec3(0.0, 0.0, 1.0));
		vec3 Hs2 = normalize(Ld2 + vec3(0.0, 0.0, 1.0));
		// Deliberately the UNDISTURBED normal. Feeding a perturbed normal into
		// a specular this sharp turns every ripple into chaotic glitter —
		// the tin-foil storm. The disturbance belongs in the refracted view,
		// which is smooth, not in the highlight, which is not.
		float sp = pow(max(dot(Nf, Hs1), 0.0), uSurfPow)
			+ pow(max(dot(Nf, Hs2), 0.0), uSurfPow) * uFaceFill;
		// a little roughness, so the highlight has structure instead of
		// reading as a pasted-on gradient
		float rough = 0.72 + 0.56 * fb(pw * uSurfRough + vec2(3.0, 8.0));
		col += vec3(1.0, 0.99, 0.96) * sp * rough * uSurfGain;
		// FRESNEL: a face turned away from you reflects more of the room.
		// This is what puts a bright edge on the far facets and keeps the
		// silhouette of the solid readable.
		float fresRim = pow(1.0 - clamp(Nf.z, 0.0, 1.0), 3.2);
		col += vec3(0.78, 0.85, 1.0) * fresRim * uSurfRim;
	}

	// ── THE OIL FILM ─────────────────────────────────────────────────────
	if (uFilm > 0.001){
		vec2 fo = bl2(uOff, suv, uTxS);
		vec2 fv = bl2(uVel, suv, uTxS);

		// Awake near the cursor, and where the fluid is genuinely moving.
		// The velocity field decodes into the +/-8 range, so this MUST be a
		// smoothstep — multiplying it raw pins the whole screen at full wake.
		vec2 cd = (suv - uCursor) * A;
		float cg = exp(-dot(cd, cd) / max(uFilmRadius * uFilmRadius, 1e-4));
		// length() is fine HERE: this gates visibility, it is not a colour
		// coordinate, so it cannot produce banding
		float flowWake = smoothstep(uActLo, uActHi * 2.0, length(fv) * uFilmFlow);
		// the cursor's own halo only counts where the fluid is actually moving
		float wake = clamp(max(cg * 0.55 * flowWake, flowWake), 0.0, 1.0);
		float a = wake * uFilm * act;

		if (a > 0.002){
			// glancing angle = more film to look through = harder banding
			float glance = 1.0 / max(Nf.z, 0.34);
			vec2 fp = (pw + uFTilt * 0.25) * uFilmScale;
			// No time term. A constant scroll here reads as material being
			// emitted from the cursor; the grain should only move because the
			// fluid carries it.
			float grain = fb(fp + fo * 2.2);
			// DYE CONCENTRATION IS FILM THICKNESS. This is the whole point of
			// an oil film: its colour comes from how thick it is. Dye is
			// injected at the cursor and carried by the flow, so the bands are
			// laid down where you drag and stretched as the fluid moves them.
			// Driving thickness from the velocity history instead gives a
			// smeared global record — wisps, far from the cursor.
			float dxe = 0.0042;
			float dC = bl1(uDye, suv, uTxS);
			float dL = bl1(uDye, suv - vec2(dxe, 0.0), uTxS);
			float dR = bl1(uDye, suv + vec2(dxe, 0.0), uTxS);
			float dD2 = bl1(uDye, suv - vec2(0.0, dxe), uTxS);
			float dU2 = bl1(uDye, suv + vec2(0.0, dxe), uTxS);
			vec2 dg = vec2(dR - dL, dU2 - dD2) / (2.0 * dxe);

			float ce = 0.0055;
			vec2 fvR = bl2(uVel, suv + vec2(ce, 0.0), uTxS);
			vec2 fvL = bl2(uVel, suv - vec2(ce, 0.0), uTxS);
			vec2 fvU = bl2(uVel, suv + vec2(0.0, ce), uTxS);
			vec2 fvD = bl2(uVel, suv - vec2(0.0, ce), uTxS);
			float curl = ((fvR.y - fvL.y) - (fvU.x - fvD.x)) * 0.5;

			// accumulator gives structure, velocity gives the same-frame snap
			// NO length() here. The magnitude of a radial splat is a set of
			// concentric contours, and cycling a palette over concentric
			// contours draws concentric rainbow rings — same bug, same cause
			// as before. Directional projections give a dipole, not rings.
			vec2 dA = vec2(0.62, -0.78);
			float th = (dC * uFilmThick
				+ grain * 0.22
				+ length(dg) * uFilmVel * 0.05
				+ curl * uFilmCurl) * glance;
			vec3 filmC = pal(hueA + th * uFilmCycles);

			// the film's own normal, from the gradient of its thickness
			// highlights bend on the shape of the STROKE, not on a global field
			vec2 gr = dg;
			// velocity shear bends the highlights too, so they ripple on the
			// frame you move rather than eighty frames later
			vec2 vg = vec2(dot(fvR, dA) - dot(fvL, dA), dot(fvU, dA) - dot(fvD, dA)) / (2.0 * ce);
			vec3 Nfl = normalize(Nf
				+ vec3(-gr.x, -gr.y, 0.0) * uFilmBump
				+ vec3(-vg.x, -vg.y, 0.0) * uFilmBump * 0.25);
			float fspec = pow(max(dot(Nfl, Hv), 0.0), uFilmSpecPow) * uFilmSpecGain
				+ pow(max(dot(Nfl, Hv2), 0.0), uFilmSpecPow) * uFilmSpecGain * uFill;

			// A film on a dark stone is a REFLECTION — it shows at glancing
			// angles and barely at all on a face pointing straight at you.
			// Without this the film reads as paint rather than as a surface.
			float fres = 0.16 + 0.84 * pow(1.0 - clamp(Nf.z, 0.0, 1.0), 1.7);
			float aa = a * fres;
			col += filmC * aa * 0.24 * (0.35 + 0.65 * diff);
			col += mix(vec3(1.0), filmC, 0.60) * fspec * aa * 0.85;
		}
	}

	// wake barely touches brightness — sleeping faces lose fire, not light
	col *= uFBright * mix(0.88, 1.0, uFWake);

	// ── THE ARCHITECTURE: seams and vertex flares ─────────────────────────
	// The facet boundaries were faint grey lines, so the six faces met with
	// nothing holding them together and the image had no skeleton. A cut
	// stone throws its hardest light along the edges where two planes meet,
	// and hardest of all at the vertices. That structure is what the eye
	// reads first — it is the architecture the composition was missing.
	float nAng = atan(gN.y, gN.x) / 6.28318;
	// A CUT DOES NOT GLOW UNIFORMLY. seamLit depends only on the facet's own
	// lighting, so along a straight edge it is the same value at every point
	// and the seam reads as a drawn line. The hub hides this because its
	// seams are short and meet at vertices constantly; one long cut across a
	// single face exposes it at once.
	//
	// A real edge is bright where there is light behind it to catch and dark
	// where the stone is. So the seam RIDES the fire it runs through: local
	// luminance modulates it, plus a slow drift along the edge so the catch
	// travels rather than sitting still.
	// The first attempt at this barely showed, because the luminance term
	// saturated: anywhere with moderate fire behind it still clamped to full
	// brightness, so most of the cut stayed lit and it went on reading as a
	// line. A POWER CURVE is what makes the dark stretches actually lose
	// their seam — the cut has to go out somewhere for it to read as catching
	// light rather than emitting it.
	float seamLum = clamp(dot(col, vec3(0.2126, 0.7152, 0.0722)) * 2.4, 0.0, 1.0);
	float seamDrift = 0.40 + 0.60 * fb(pw * uSeamGrain + vec2(t * 0.05, -t * 0.03));
	float seamRide = mix(1.0, pow(seamLum, uSeamRidePow) * seamDrift * 2.1, uSeamRide);
	float seamLit = (0.35 + 0.65 * diff) * seamRide;
	float sd1; float sd2;
	cutPair(pw, sd1, sd2);

	// dispersion ACROSS the seam: the hue sweeps as you cross it, so the edge
	// splits light instead of being a white stripe
	float sEdge = 1.0 - clamp(sd1 / max(uSeamW, 1e-4), 0.0, 1.0);
	vec3 seamC = pal(hueA + nAng * 0.16 + sEdge * uSeamDisp);
	col += seamC * pow(sEdge, 2.2) * uSeamGain * uSeamGlow * seamLit;
	// the white-hot core, a thin hard line inside that fringe
	col += mix(seamC, vec3(1.0), 0.80) * pow(sEdge, 8.0) * uSeamGain * uSeamCore * seamLit;

	// VERTEX FLARE: both nearest planes close at once. Blown out, achromatic,
	// with a soft halo — these are the nodes that pin the layout together.
	float vtx = (1.0 - clamp(sd1 / max(uVertexR, 1e-4), 0.0, 1.0))
		* (1.0 - clamp(sd2 / max(uVertexR, 1e-4), 0.0, 1.0));
	if (vtx > 0.0001){
		// the flare keeps a floor — a corner is a structural landmark and
		// should still read where the stone behind it has gone dark
		col += vec3(1.0, 0.99, 0.97) * pow(vtx, 1.6) * uVertexGain
			* mix(1.0, 0.45 + 0.55 * seamRide, uSeamRide * 0.6);
		col += mix(pal(hueA + 0.5), vec3(1.0), 0.5) * pow(vtx, 0.7) * uVertexGain * 0.22;
	}

	// The woken face's edges catch more light, in its own colour. No spectral
	// fan across the bevel — that reads as chromatic aberration, not as a cut.
	float flare = uFlareGain * uFWake;
	if (flare > 0.001){
		vec3 fl = mix(pal(hueA + 0.04), vec3(1.0), 0.35);
		col += fl * pow(rimD, 3.0) * flare * 0.55 * seamLit;
		col += mix(fl, vec3(1.0), 0.5) * pow(rimD, 9.0) * flare * 0.9;
	}

	// 5 — the catch of light on selection, brightest along the edge
	if (uPulse > 0.001){
		vec3 pc = mix(vec3(1.0), pal(hueA + 0.06), 0.55);
		col += pc * uPulse * uPulseGain * (0.10 + 0.85 * pow(rimD, 2.5));
	}

	// ── THE EYE, RESOLVED ─────────────────────────────────────────────────
	if (uEyeOn > 0.5){
		// THE SOCKET. An eye reads by contrast against a calm surround, not by
		// internal detail — real ones sit in a large matte field of skin. With
		// fire running right up to the lids there is no figure and ground, so
		// no amount of iris detail will ever make him legible. This suppresses
		// the material in a pool around the aperture: subtractive, so there is
		// nothing drawn that could look pasted on.
		vec2 sq = (pw - uEyeC * A) / vec2(max(uEyeW, 1e-4), max(uEyeH, 1e-4));
		float sdist = length(sq);
		float socket = smoothstep(uSocketIn, uSocketOut, sdist);
		col *= mix(1.0 - uSocket, 1.0, socket);
		if (eyeIn > 0.002 && eyeR > 0.0){
			// the pupil is a VOID: the stone simply has no fire there.
			// Drawing a dark disc would sit on top of the material; removing
			// the material leaves a hole you can see into.
			float pup = smoothstep(uEyePupil, uEyePupil * 1.22, eyeR);
			col *= mix(1.0, pup, eyeIn);
			// LIMBAL ABSORPTION. I claimed this would fall out of the warp on
			// its own. It does not — without a darkening at the boundary the
			// iris has no edge and reads as a smear of warped material rather
			// than as a disc set into the stone. It is absorption, not a
			// drawn ring: the material dims toward the rim.
			float limbal = 1.0 - uEyeLimbal * smoothstep(0.62, 1.0, eyeR)
				* (1.0 - smoothstep(1.0, 1.16, eyeR));
			col *= mix(1.0, limbal, eyeIn);
			// and the body around the iris sits back, so the iris is the
			// brightest thing inside the aperture
			float sclera = mix(uEyeSclera, 1.0, 1.0 - smoothstep(0.94, 1.3, eyeR));
			col *= mix(1.0, sclera, eyeIn);
			// THE TONAL RAMP. Every fibre being equally bright is why the
			// iris read as a pattern rather than a form: dark against the
			// pupil, brightest through the mid band, falling away to the rim.
			// This single curve does more for it than any amount of detail.
			float band = smoothstep(uEyePupil, uEyePupil + 0.30, eyeR)
				* (1.0 - smoothstep(0.66, 1.0, eyeR));
			float irisRamp = uEyeRampLo + (1.0 - uEyeRampLo) * (0.35 + 1.5 * band);
			col *= mix(1.0, irisRamp, eyeIn);
			col *= mix(1.0, eyeLidShade, eyeIn);
		}
		// the lash line takes the same lamp as every other cut on the stone,
		// so it brightens and dims as the stone turns
		float lidLit = 0.30 + 0.70 * diff;
		// a thin hard line with a dispersive edge, at the same weight as the
		// facet seams. It was previously brighter than anything else on the
		// screen, which turned the eye into a neon outline of itself.
		vec3 lidC = pal(hueA + 0.30 * lidCore);
		col += mix(lidC, vec3(1.0), 0.55) * pow(lidCore, 2.0) * uEyeLidGain * lidLit;
		col += vec3(1.0, 0.99, 0.97) * pow(lidCore, 7.0) * uEyeLidGain * 0.5 * lidLit;
		col += vec3(1.0, 0.99, 0.97) * lidVtx * uEyeVtx * lidLit;
	}

	// ── FOCAL FALLOFF ─────────────────────────────────────────────────────
	// Six faces bleeding to the frame edge at equal intensity gives the eye
	// nowhere to go. Concentrating the light toward the middle of the stone
	// and letting the outer reaches fall away is what makes a composition
	// read as an OBJECT sitting in a space rather than as wallpaper.
	if (uFocus > 0.001){
		vec2 fcv = (suv - 0.5) * A;
		float rad = length(fcv);
		float fall = smoothstep(uFocusOut, uFocusIn, rad);
		col *= mix(1.0, 0.15 + 0.85 * fall, uFocus);
	}

	col += (h1(FRAGCOORD.xy + fract(t)) - 0.5)*0.004;
	col = pow(max(col, vec3(0.0)), vec3(uOutGamma)) * uExposure;
	// tonal separation: deepen the mid-tones so a face is black OR blazing.
	// Without this the gaps between formations fill with bright murk, which
	// is what turns the stone into stained glass.
	if (uSCurve > 0.001){
		col = col * (col * (uSCurve + 1.0) - uSCurve + 1.0) / (col * uSCurve + 1.0);
	}
	float Lm = dot(col, vec3(0.2126, 0.7152, 0.0722));
	col = max(mix(vec3(Lm), col, uSat), vec3(0.0));
	// soft shoulder: compress the peak, scale all channels together so a
	// bright face keeps its hue instead of clipping toward white
	float pk = max(col.r, max(col.g, col.b));
	if (pk > uShoulder){
		float over = pk - uShoulder;
		float rolled = uShoulder + over / (1.0 + over / max(1.0 - uShoulder, 0.001));
		col *= rolled / pk;
	}
	COLOR = vec4(clamp(col, vec3(0.0), vec3(1.0)), 1.0);
}
"""


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	var sh := Shader.new()
	sh.code = SH_FACET % SH_COMMON
	_mat = ShaderMaterial.new()
	_mat.shader = sh
	material = _mat
	_wake = wake
	_resolve_sim()
	set_process(true)


func _exit_tree() -> void:
	if _sim != null:
		_sim.unregister_facet(self)


func _resolve_sim() -> void:
	if sim_path != NodePath():
		_sim = get_node_or_null(sim_path) as OpalCore
	if _sim == null:
		var root := get_tree().current_scene if get_tree() != null else null
		if root != null:
			var found := root.find_children("*", "OpalCore", true, false)
			if found.size() > 0:
				_sim = found[0] as OpalCore
	if _sim == null and get_tree() != null:
		# search the WHOLE tree, not just current_scene: the core may live in
		# an autoload, which sits beside the running scene rather than inside
		# it, so a current_scene search misses it entirely
		var all := get_tree().get_root().find_children("*", "OpalCore", true, false)
		if all.size() > 0:
			_sim = all[0] as OpalCore
	if _sim != null:
		_sim.register_facet(self)
	else:
		push_warning("OpalFacet '%s': no OpalCore found — set sim_path." % name)


# ── Public ────────────────────────────────────────────────────────────────────
func set_poly(q: PackedVector2Array) -> void:
	if q.size() >= 3 and q.size() <= 6:
		poly = q
		queue_redraw()


func set_wake(w: float) -> void:
	wake = clampf(w, 0.0, 1.0)


## A brief catch of light. Call on selection change.
func set_pulse(p: float) -> void:
	_pulse = maxf(_pulse, clampf(p, 0.0, 1.0))


# ── Per-frame ─────────────────────────────────────────────────────────────────
func _process(_delta: float) -> void:
	if _sim == null or _mat == null:
		return
	# facets sample in screen space, so pin them to the viewport rather than
	# relying on anchor layout (which has not run yet when they are spawned)
	var vr := get_viewport_rect().size
	if size != vr:
		position = Vector2.ZERO
		size = vr
	_wake += (wake - _wake) * wake_lerp
	_pulse *= pulse_decay
	if _pulse < 0.002:
		_pulse = 0.0

	var asp := 1.7778
	if size.y > 0.0:
		asp = size.x / size.y

	_sim.push_globals(_mat)
	_mat.set_shader_parameter("uT", _sim.get_time())
	_mat.set_shader_parameter("uView", _sim.get_view())
	_mat.set_shader_parameter("uHard", _sim.get_hard())
	_mat.set_shader_parameter("uAsp", asp)

	var fa := _sim.focus_amount()
	_mat.set_shader_parameter("uFHue", _blend_hue(hue, _sim.focus_hue(), fa))
	_mat.set_shader_parameter("uFSpread", lerpf(spread, _sim.focus_spread(), fa))
	_mat.set_shader_parameter("uFTilt", tilt)
	_mat.set_shader_parameter("uFAng", deg_to_rad(angle_deg))
	_mat.set_shader_parameter("uFOfs", pattern_offset)
	_mat.set_shader_parameter("uFScale", scale_mul)
	_mat.set_shader_parameter("uFCov", coverage_bias)
	_mat.set_shader_parameter("uRareHue", rare_offset)
	_mat.set_shader_parameter("uWakeTarget", identity_hue)
	_mat.set_shader_parameter("uFBright", brightness)
	_mat.set_shader_parameter("uFWake", _wake)
	_mat.set_shader_parameter("uPulse", _pulse)
	_mat.set_shader_parameter("uFLayers", layers)
	_mat.set_shader_parameter("uSeamW", seam_width)
	_mat.set_shader_parameter("uSeamGain", seam_gain)
	_mat.set_shader_parameter("uRefract", refract)

	_mat.set_shader_parameter("uCalm", calm)
	# set AFTER push_globals, so a face can override the material default
	_mat.set_shader_parameter("uSeamRide", seam_ride)
	_mat.set_shader_parameter("uSeamRidePow", seam_ride_falloff)
	_mat.set_shader_parameter("uSeamGrain", seam_grain)
	_mat.set_shader_parameter("uEyeOn", 1.0 if eye_enabled else 0.0)
	if eye_enabled:
		_mat.set_shader_parameter("uEyeC", eye_centre)
		_mat.set_shader_parameter("uEyeLook", eye_look)
		_mat.set_shader_parameter("uEyeW", eye_width)
		_mat.set_shader_parameter("uEyeH", eye_height)
		_mat.set_shader_parameter("uEyeOpen", eye_open)
		_mat.set_shader_parameter("uEyeLidLow", eye_lid_low)
		_mat.set_shader_parameter("uEyeR", eye_radius)
		_mat.set_shader_parameter("uEyeAng", eye_angle_scale)
		_mat.set_shader_parameter("uEyeRad", eye_radial_scale)
		_mat.set_shader_parameter("uEyePupil", eye_pupil)
		_mat.set_shader_parameter("uEyeEdge", eye_edge_soft)
		_mat.set_shader_parameter("uEyeLidW", eye_lash_width)
		_mat.set_shader_parameter("uEyeLidGain", eye_lash_gain)
		_mat.set_shader_parameter("uEyeVtx", eye_vertex_gain)
		_mat.set_shader_parameter("uEyeLimbal", eye_limbal)
		_mat.set_shader_parameter("uEyeSclera", eye_sclera_dim)
		_mat.set_shader_parameter("uEyeHue", eye_hue)
		_mat.set_shader_parameter("uEyeSpread", eye_hue_spread)
		_mat.set_shader_parameter("uEyeRampLo", eye_ramp_floor)
		_mat.set_shader_parameter("uEyeLidShade", eye_lid_shadow)
		_mat.set_shader_parameter("uEyeShadeD", eye_lid_shadow_depth)
		_mat.set_shader_parameter("uEyeSkew", eye_skew)
		_mat.set_shader_parameter("uSocket", socket_depth)
		_mat.set_shader_parameter("uSocketIn", socket_inner)
		_mat.set_shader_parameter("uSocketOut", socket_outer)

	_push_cuts(asp)
	queue_redraw()


# Half-planes from the polygon, in aspect-corrected UV space.
# inside == dot(n, p) - d <= 0 for every edge.
func _push_cuts(asp: float) -> void:
	var n_pts := poly.size()
	if n_pts < 3 or n_pts > 6:
		return
	var A := Vector2(asp, 1.0)
	var c := Vector2.ZERO
	for q in poly:
		c += q * A
	c /= float(n_pts)
	var names := ["uCut0", "uCut1", "uCut2", "uCut3", "uCut4", "uCut5"]
	var eps := 0.004
	for i in 6:
		if i < n_pts:
			var ra := poly[i]
			var rb := poly[(i + 1) % n_pts]
			# an edge lying along the screen border is not a cut
			var on_edge: bool = false
			if absf(ra.x) < eps and absf(rb.x) < eps:
				on_edge = true
			elif absf(ra.x - 1.0) < eps and absf(rb.x - 1.0) < eps:
				on_edge = true
			elif absf(ra.y) < eps and absf(rb.y) < eps:
				on_edge = true
			elif absf(ra.y - 1.0) < eps and absf(rb.y - 1.0) < eps:
				on_edge = true
			var a := ra * A
			var b := rb * A
			var e := b - a
			var nn := Vector2(-e.y, e.x).normalized()
			if nn.dot(c - a) > 0.0:
				nn = -nn
			var flag := 0.0 if on_edge else 1.0
			_mat.set_shader_parameter(names[i], Vector4(nn.x, nn.y, nn.dot(a), flag))
		else:
			_mat.set_shader_parameter(names[i], Vector4(0.0, 0.0, 1.0, 0.0))


func _blend_hue(a: float, b: float, w: float) -> float:
	if w <= 0.001:
		return a
	var d := fposmod(b - a + 50.0, 100.0) - 50.0
	return fposmod(a + d * w, 100.0)


func _draw() -> void:
	if poly.size() < 3 or size.x <= 0.0 or size.y <= 0.0:
		return
	var pts := PackedVector2Array()
	for q in poly:
		pts.append(Vector2(q.x * size.x, q.y * size.y))
	draw_colored_polygon(pts, Color.WHITE)
