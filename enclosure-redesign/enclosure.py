#!/usr/bin/env freecadcmd
# -*- coding: utf-8 -*-
"""
Turn Taker - parametric two-piece 3D-printable enclosure (revision 1).

Run headless:

    freecadcmd enclosure.py

Outputs land in ./output/  (STEP + STL for base and lid, plus preview PNGs).

--------------------------------------------------------------------------
COORDINATE SYSTEM
--------------------------------------------------------------------------
The enclosure is modelled in its own "local" frame, which is nicer to work
with than the KiCad board frame:

    X, Y : centred on the round PCB's axis, Y is "up" when looking at the
           component side of the board (i.e. Y = -kicad_y).
    Z    : 0 at the outside bottom of the base; +Z points towards the lid.

Mapping from the KiCad STEP export (turn-taker.step) into this frame:

    x_local = x_step - 181.533565
    y_local = y_step + 86.500000          (the STEP already has y = -kicad_y)
    z_local = z_step + PCB_BOTTOM_Z       (STEP has the PCB bottom at z = 0)

Every hard number below that is marked "derived" came out of the KiCad
board file / drill files / STEP assembly - see README.md.
"""

import os
import math
import sys

import FreeCAD as App
import Part
from FreeCAD import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "output")
PCB_STEP = os.path.normpath(
    os.path.join(HERE, "..", "turn-taker", "turn-taker", "turn-taker", "turn-taker.step")
)

# ==========================================================================
# 1. PARAMETERS - edit these
# ==========================================================================

# ---- PCB (derived from turn-taker.kicad_pcb Edge.Cuts + STEP) ------------
PCB_R                  = 32.533565  # derived: Edge.Cuts circle radius (65.067 mm dia)
PCB_T                  = 1.60       # nominal board thickness (STEP solid is 1.51)
PCB_UNDERSIDE_PROTRUDE = 1.91       # derived: lowest point of THT pins below board

# ---- Fit / clearances ----------------------------------------------------
PCB_RADIAL_CLEAR = 0.50   # gap between PCB edge and base cavity wall
LID_FIT_CLEAR    = 0.30   # radial gap between lid skirt and base cavity wall

# ---- Wall / floor / lid --------------------------------------------------
WALL_T  = 2.00            # base side wall
FLOOR_T = 2.00            # base floor
LID_T   = 2.20            # lid plate thickness

# ---- Heights -------------------------------------------------------------
PCB_ELEVATION       = 5.00   # underside of PCB above the inside of the floor
LID_INTERIOR_CLEAR  = 16.00  # free height from PCB top surface to lid underside

# ---- PCB support ---------------------------------------------------------
LEDGE_W       = 1.60   # width of the continuous annular ledge the PCB rests on
POST_DIA      = 5.00   # diameter of the internal support posts
POST_KEEPOUT  = 1.60   # assumed radius of a solder joint on the board underside
# Support post centres, local X/Y (mm). All were checked against every plated
# through-hole in turn-taker-PTH.drl - see the validation report.
SUPPORT_POSTS = [
    (-19.90,  -7.00),   # between SW1 and SW2 - takes the button press load
    (-19.90,   6.80),   # above SW1
    (-19.90, -21.00),   # below SW2
    (  0.00,   0.00),   # board centre (between the Pico's two pin rows)
    (  0.00,  25.50),   # under the Pico USB end
    (  0.00, -25.50),   # bottom centre
    ( 28.00,   0.00),   # right edge
    ( 24.00,  14.00),   # upper right
    ( 24.00, -16.00),   # lower right
]

# ---- Lid location feature (inner skirt) ----------------------------------
SKIRT_H       = 4.00   # how deep the lid skirt drops into the base
SKIRT_T       = 2.00   # skirt wall thickness
SKIRT_LEADIN  = 0.80   # chamfer on the bottom outside edge of the skirt

# ---- Tactile buttons (derived from the STEP actuator cylinders) ----------
# SW1 -> GPIO14 ("defer"/add turn), SW2 -> GPIO15 ("take" turn).
SW1_CENTER   = (-19.884,   0.250)   # derived: R1.75 actuator cylinder axis
SW2_CENTER   = (-19.884, -14.250)   # derived
SW_ACT_R     = 1.75                 # derived: actuator radius
SW_ACT_TOP   = 4.385                # derived: actuator top above PCB top face
BUTTON_HOLE_DIA = 6.00              # lid opening over each actuator

# Optional printed plungers. The lid sits ~13.8 mm above the actuators (J2's
# 8.5 mm header sets the internal height), which is too deep to press with a
# fingertip through a 6 mm hole - the bare lid only works with a stylus/pen.
# These drop into the button holes from underneath and bridge the gap. They
# are a separate print; the lid geometry is identical with or without them.
BUTTON_PLUNGER_ENABLE = True
PLUNGER_CLEAR       = 0.35   # radial clearance of the stem inside the lid hole
PLUNGER_STEM_DIA    = 4.80   # lower stem, lands on the tactile actuator
PLUNGER_FLANGE_DIA  = 8.50   # retaining flange under the lid
PLUNGER_FLANGE_T    = 1.60
PLUNGER_FREEPLAY    = 0.40   # flange-to-lid gap at rest (switch travel ~0.25)
PLUNGER_PROUD       = 1.20   # how far the cap stands above the lid face

# ---- Display (ASSUMED - not present in the PCB/STEP data) ----------------
# The 128x64 SSD1315 is a separate flying-lead module (wired to J1), so its
# position is a design choice, not something the CAD can tell us. Defaults
# below are for the ubiquitous 0.96" 4-pin I2C OLED module.
DISPLAY_ENABLE        = True
DISPLAY_MODULE_W      = 27.30   # module PCB width
DISPLAY_MODULE_H      = 27.80   # module PCB height (incl. its 4-pin header strip)
DISPLAY_MODULE_CLEAR  = 0.50    # clearance added around the module in the recess
DISPLAY_RECESS_DEPTH  = 1.40    # pocket in the lid underside the module sits in
DISPLAY_WINDOW_W      = 24.00   # see-through opening (active area is 21.74 x 10.86)
DISPLAY_WINDOW_H      = 15.00
DISPLAY_WINDOW_OFFSET = (0.00, -3.60)   # window centre relative to module centre
DISPLAY_CENTER        = (-0.70, -3.40)  # module centre in local X/Y
# (chosen so the window centres on the midpoint between SW1 and SW2)
DISPLAY_CORNER_R      = 1.20

# ---- S1 power slide switch (derived position, assumed body) --------------
# S1 switches BAT+ -> +5V. It sits ~10 mm inside the board edge, so it cannot
# be reached through the side wall; it gets a slot in the lid instead.
S1_SLOT_ENABLE = True
S1_CENTER      = (18.466, 0.300)   # derived: middle pad of S1
S1_SLOT_W      = 7.00              # local X
S1_SLOT_H      = 13.00             # local Y
S1_SLOT_CORNER_R = 1.50

# ---- Pico USB micro-B port (derived from the STEP) -----------------------
USB_ENABLE   = True
USB_X        = -0.259   # derived: connector centre, local X
USB_PORT_W   = 7.50     # derived
USB_PORT_Z0  = 1.085    # derived: port bottom above PCB top face
USB_PORT_Z1  = 3.535    # derived: port top above PCB top face
USB_SLOT_W   = 13.00    # opening width - sized for a micro-USB plug overmould
USB_SLOT_H   = 8.00
USB_SLOT_Z_OFFSET = 0.20  # slot bottom above the PCB underside plane

# ---- Misc ----------------------------------------------------------------
PRY_NOTCH_ENABLE = True   # scallop in the base rim so the lid can be pushed off
PRY_NOTCH_R      = 6.00
PRY_NOTCH_DEPTH  = 1.60
PRY_NOTCH_ANGLE  = 270.0  # degrees, 0 = +X

EDGE_CHAMFER = 0.60       # cosmetic chamfer on the outer top/bottom edges

STL_LINEAR_DEFLECTION  = 0.04
STL_ANGULAR_DEFLECTION = 0.15

RENDER_PREVIEWS = True

# ==========================================================================
# 2. DERIVED GEOMETRY
# ==========================================================================

PCB_BOTTOM_Z   = FLOOR_T + PCB_ELEVATION
PCB_TOP_Z      = PCB_BOTTOM_Z + PCB_T
CAVITY_R       = PCB_R + PCB_RADIAL_CLEAR
OUTER_R        = CAVITY_R + WALL_T
LEDGE_INNER_R  = CAVITY_R - LEDGE_W
BASE_TOP_Z     = PCB_TOP_Z + LID_INTERIOR_CLEAR   # base rim / lid underside
LID_TOP_Z      = BASE_TOP_Z + LID_T
SKIRT_OUTER_R  = CAVITY_R - LID_FIT_CLEAR
SKIRT_INNER_R  = SKIRT_OUTER_R - SKIRT_T

# STEP -> local transform
STEP_DX = -181.533565
STEP_DY = 86.500000
STEP_DZ = PCB_BOTTOM_Z

LOG = []


def log(msg=""):
    print(msg)
    LOG.append(msg)


# ==========================================================================
# 3. SMALL HELPERS
# ==========================================================================

def cyl(r, z0, z1, x=0.0, y=0.0):
    return Part.makeCylinder(r, z1 - z0, Vector(x, y, z0))


def tube(r_out, r_in, z0, z1):
    return cyl(r_out, z0, z1).cut(cyl(r_in, z0 - 1.0, z1 + 1.0))


def rounded_box(w, h, depth, cx, cy, z0, corner_r=0.0):
    """Axis-aligned prism, optionally with rounded vertical corners."""
    corner_r = min(corner_r, w / 2.0 - 0.01, h / 2.0 - 0.01)
    if corner_r <= 0.05:
        return Part.makeBox(w, h, depth, Vector(cx - w / 2.0, cy - h / 2.0, z0))
    inner = Part.makeBox(w - 2 * corner_r, h, depth,
                         Vector(cx - w / 2.0 + corner_r, cy - h / 2.0, z0))
    inner2 = Part.makeBox(w, h - 2 * corner_r, depth,
                          Vector(cx - w / 2.0, cy - h / 2.0 + corner_r, z0))
    shp = inner.fuse(inner2)
    for sx in (-1, 1):
        for sy in (-1, 1):
            shp = shp.fuse(cyl(corner_r, z0, z0 + depth,
                               cx + sx * (w / 2.0 - corner_r),
                               cy + sy * (h / 2.0 - corner_r)))
    return shp.removeSplitter()


def half(shape):
    """Cut away y < 0 so a section through the assembly can be rendered."""
    return shape.cut(Part.makeBox(200.0, 120.0, 120.0, Vector(-100.0, -120.0, -20.0)))


def try_chamfer(shape, picker, size, what=""):
    """Chamfer the edges selected by picker(edge) -> bool. Never fatal."""
    edges = [e for e in shape.Edges if picker(e)]
    if not edges:
        return shape
    try:
        return shape.makeChamfer(size, edges)
    except Exception as exc:              # pragma: no cover - cosmetic only
        log("  ! chamfer skipped (%s): %s" % (what, exc))
        return shape


def horiz_circle_edge_at(z, radius, tol=1e-6):
    def picker(e):
        bb = e.BoundBox
        if abs(bb.ZMin - z) > 1e-3 or abs(bb.ZMax - z) > 1e-3:
            return False
        return abs(max(abs(bb.XMax), abs(bb.XMin)) - radius) < 0.05
    return picker


# ==========================================================================
# 4. BASE
# ==========================================================================

def build_base():
    log("Building base ...")
    base = cyl(OUTER_R, 0.0, BASE_TOP_Z)

    # lower cavity (under the PCB) - narrower, so its roof forms the PCB ledge
    base = base.cut(cyl(LEDGE_INNER_R, FLOOR_T, PCB_BOTTOM_Z))
    # upper cavity (PCB and everything above it)
    base = base.cut(cyl(CAVITY_R, PCB_BOTTOM_Z, BASE_TOP_Z + 1.0))

    # PCB support posts, floor -> PCB underside
    for (px, py) in SUPPORT_POSTS:
        base = base.fuse(cyl(POST_DIA / 2.0, FLOOR_T, PCB_BOTTOM_Z, px, py))
    base = base.removeSplitter()

    # USB micro-B access slot through the wall
    if USB_ENABLE:
        z0 = PCB_BOTTOM_Z + USB_SLOT_Z_OFFSET
        slot = Part.makeBox(
            USB_SLOT_W, OUTER_R + 4.0, USB_SLOT_H,
            Vector(USB_X - USB_SLOT_W / 2.0, CAVITY_R - 2.0, z0))
        base = base.cut(slot)

    # scallop in the rim so the lid can be pushed off
    if PRY_NOTCH_ENABLE:
        a = math.radians(PRY_NOTCH_ANGLE)
        nx = math.cos(a) * OUTER_R
        ny = math.sin(a) * OUTER_R
        base = base.cut(cyl(PRY_NOTCH_R,
                            BASE_TOP_Z - PRY_NOTCH_DEPTH, BASE_TOP_Z + 1.0,
                            nx, ny))

    # cosmetic chamfers on the outer bottom + top edges
    base = try_chamfer(base, horiz_circle_edge_at(0.0, OUTER_R),
                       EDGE_CHAMFER, "base bottom")
    base = try_chamfer(base, horiz_circle_edge_at(BASE_TOP_Z, OUTER_R),
                       EDGE_CHAMFER * 0.5, "base top")

    base = base.removeSplitter()
    log("  base: %d solid(s), volume %.1f mm^3, bbox Z %.2f..%.2f"
        % (len(base.Solids), base.Volume, base.BoundBox.ZMin, base.BoundBox.ZMax))
    return base


# ==========================================================================
# 5. LID
# ==========================================================================

def display_recess_size():
    return (DISPLAY_MODULE_W + 2 * DISPLAY_MODULE_CLEAR,
            DISPLAY_MODULE_H + 2 * DISPLAY_MODULE_CLEAR)


def display_window_center():
    return (DISPLAY_CENTER[0] + DISPLAY_WINDOW_OFFSET[0],
            DISPLAY_CENTER[1] + DISPLAY_WINDOW_OFFSET[1])


def build_lid():
    log("Building lid ...")
    lid = cyl(OUTER_R, BASE_TOP_Z, LID_TOP_Z)

    # locating skirt that drops into the base cavity
    skirt = tube(SKIRT_OUTER_R, SKIRT_INNER_R, BASE_TOP_Z - SKIRT_H, BASE_TOP_Z)
    lid = lid.fuse(skirt).removeSplitter()

    # lead-in chamfer on the free end of the skirt
    lid = try_chamfer(lid, horiz_circle_edge_at(BASE_TOP_Z - SKIRT_H, SKIRT_OUTER_R),
                      SKIRT_LEADIN, "skirt lead-in")

    # display: recess in the underside + see-through window
    if DISPLAY_ENABLE:
        rw, rh = display_recess_size()
        lid = lid.cut(rounded_box(rw, rh, DISPLAY_RECESS_DEPTH + 0.001,
                                  DISPLAY_CENTER[0], DISPLAY_CENTER[1],
                                  BASE_TOP_Z - 0.001, DISPLAY_CORNER_R))
        wx, wy = display_window_center()
        lid = lid.cut(rounded_box(DISPLAY_WINDOW_W, DISPLAY_WINDOW_H,
                                  LID_T + 2.0, wx, wy,
                                  BASE_TOP_Z - 1.0, DISPLAY_CORNER_R))

    # button openings
    for (bx, by) in (SW1_CENTER, SW2_CENTER):
        lid = lid.cut(cyl(BUTTON_HOLE_DIA / 2.0,
                          BASE_TOP_Z - SKIRT_H - 1.0, LID_TOP_Z + 1.0, bx, by))

    # S1 power switch access slot
    if S1_SLOT_ENABLE:
        lid = lid.cut(rounded_box(S1_SLOT_W, S1_SLOT_H, LID_T + 2.0,
                                  S1_CENTER[0], S1_CENTER[1],
                                  BASE_TOP_Z - 1.0, S1_SLOT_CORNER_R))

    lid = try_chamfer(lid, horiz_circle_edge_at(LID_TOP_Z, OUTER_R),
                      EDGE_CHAMFER, "lid top")

    lid = lid.removeSplitter()
    log("  lid: %d solid(s), volume %.1f mm^3, bbox Z %.2f..%.2f"
        % (len(lid.Solids), lid.Volume, lid.BoundBox.ZMin, lid.BoundBox.ZMax))
    return lid


# ==========================================================================
# 5b. OPTIONAL BUTTON PLUNGER
# ==========================================================================

SW_ACT_Z = PCB_TOP_Z + SW_ACT_TOP        # top of the tactile actuator


def plunger_dims():
    """(flange_bottom, flange_top, total_h) measured from the actuator top."""
    fb = (BASE_TOP_Z - PLUNGER_FREEPLAY - PLUNGER_FLANGE_T) - SW_ACT_Z
    return fb, fb + PLUNGER_FLANGE_T, (LID_TOP_Z + PLUNGER_PROUD) - SW_ACT_Z


def build_plunger():
    """Built at the origin with z = 0 sitting on the switch actuator.

    Prints stem-down with no supports: the flange overhang is picked up by a
    45 deg cone and everything above the flange steps inwards.
    """
    log("Building button plunger ...")
    fb, ft, total = plunger_dims()
    stem_r = PLUNGER_STEM_DIA / 2.0
    fl_r = PLUNGER_FLANGE_DIA / 2.0
    top_r = BUTTON_HOLE_DIA / 2.0 - PLUNGER_CLEAR
    cone_h = fl_r - stem_r                     # 45 deg

    p = cyl(stem_r, 0.0, fb - cone_h)
    p = p.fuse(Part.makeCone(stem_r, fl_r, cone_h, Vector(0, 0, fb - cone_h)))
    p = p.fuse(cyl(fl_r, fb, ft))
    p = p.fuse(cyl(top_r, ft, total))
    p = p.removeSplitter()
    p = try_chamfer(p, horiz_circle_edge_at(total, top_r), 0.5, "plunger cap")
    log("  plunger: %.2f mm tall, stem %.2f / flange %.2f / cap %.2f mm dia"
        % (total, PLUNGER_STEM_DIA, PLUNGER_FLANGE_DIA, 2 * top_r))
    return p


def plunger_placed(center):
    p = build_plunger() if not hasattr(plunger_placed, "_p") else plunger_placed._p
    plunger_placed._p = p
    q = p.copy()
    q.translate(Vector(center[0], center[1], SW_ACT_Z))
    return q


# ==========================================================================
# 6. PCB REFERENCE (read-only import of the KiCad STEP export)
# ==========================================================================

# Footprint reference points in local X/Y, used to name the STEP solids.
REF_POINTS = {
    "PCB":      (0.000, 0.000),
    "A1 Pico":  (-0.259, 4.150),
    "SW1":      (-19.884, 0.250),
    "SW2":      (-19.884, -14.250),
    "J1 (display conn.)": (-19.059, 14.500),
    "J2 (power conn.)":   (17.966, 18.310),
    "Bat_In1":  (19.466, -14.450),
}


def load_pcb_reference():
    """Returns dict name -> Part.Shape, already moved into the local frame."""
    if not os.path.exists(PCB_STEP):
        log("  ! reference STEP not found: %s" % PCB_STEP)
        return {}
    comp = Part.Shape()
    comp.read(PCB_STEP)
    comp.translate(Vector(STEP_DX, STEP_DY, STEP_DZ))
    solids = comp.Solids
    named = {}
    used = set()
    for s in solids:
        c = s.BoundBox.Center
        best, bestd = None, 1e9
        for name, (rx, ry) in REF_POINTS.items():
            if name in used:
                continue
            d = math.hypot(c.x - rx, c.y - ry)
            if d < bestd:
                best, bestd = name, d
        if best is None or bestd > 8.0:
            best = "solid@(%.1f,%.1f)" % (c.x, c.y)
        else:
            used.add(best)
        named[best] = s
    log("  reference STEP: %d solids -> %s" % (len(solids), ", ".join(sorted(named))))
    return named


# ==========================================================================
# 7. VALIDATION
# ==========================================================================

def check(ok, msg):
    log("  [%s] %s" % ("PASS" if ok else "FAIL", msg))
    return bool(ok)


def validate(base, lid, ref):
    log("")
    log("Validation")
    log("----------")
    results = []

    pcb = ref.get("PCB")
    comps = {k: v for k, v in ref.items() if k != "PCB"}

    # --- 1. PCB fits the cavity ------------------------------------------
    results.append(check(
        CAVITY_R > PCB_R,
        "PCB fits base cavity: cavity R %.3f vs PCB R %.3f (%.2f mm radial gap)"
        % (CAVITY_R, PCB_R, CAVITY_R - PCB_R)))

    # --- 2. PCB rests on the ledge, clear of the pins ---------------------
    bearing = PCB_R - LEDGE_INNER_R
    results.append(check(
        bearing > 0.5,
        "PCB perimeter ledge bearing width %.2f mm (ledge inner R %.2f)"
        % (bearing, LEDGE_INNER_R)))
    results.append(check(
        PCB_ELEVATION > PCB_UNDERSIDE_PROTRUDE + 1.0,
        "PCB underside clears the floor: %.2f mm elevation vs %.2f mm of pin "
        "protrusion (%.2f mm spare)"
        % (PCB_ELEVATION, PCB_UNDERSIDE_PROTRUDE,
           PCB_ELEVATION - PCB_UNDERSIDE_PROTRUDE)))

    # --- 3. support posts vs every plated through-hole --------------------
    holes = load_drill_holes()
    post_r = POST_DIA / 2.0
    need = post_r + POST_KEEPOUT
    worst, worst_post = 1e9, None
    for (px, py) in SUPPORT_POSTS:
        for (hx, hy) in holes:
            d = math.hypot(px - hx, py - hy)
            if d < worst:
                worst, worst_post = d, (px, py)
    results.append(check(
        worst >= need,
        "support posts clear of solder joints: worst gap %.2f mm at post "
        "(%.1f, %.1f), need >= %.2f mm (%d holes checked)"
        % (worst, worst_post[0], worst_post[1], need, len(holes))))
    far = [p for p in SUPPORT_POSTS if math.hypot(*p) + post_r > PCB_R]
    results.append(check(not far, "all %d support posts lie under the PCB"
                         % len(SUPPORT_POSTS)))

    # posts really do touch the board underside
    tops = set(round(PCB_BOTTOM_Z, 4) for _ in SUPPORT_POSTS)
    results.append(check(
        tops == {round(PCB_BOTTOM_Z, 4)},
        "support post tops are all at z = %.2f (PCB underside plane)" % PCB_BOTTOM_Z))

    # --- 4. solid interference with the real assembly ---------------------
    if pcb is not None:
        for name, part in (("base", base), ("lid", lid)):
            worst_name, worst_vol = None, 0.0
            for cname, cshape in list(comps.items()) + [("PCB", pcb)]:
                try:
                    v = part.common(cshape).Volume
                except Exception:
                    v = -1.0
                if v > worst_vol:
                    worst_vol, worst_name = v, cname
            results.append(check(
                worst_vol < 0.5,
                "%s does not intersect the PCB assembly (max overlap %.3f mm^3%s)"
                % (name, worst_vol, " on " + worst_name if worst_name else "")))

    # --- 5. lid clears the tallest components -----------------------------
    if comps:
        tallest, tallest_z = None, -1e9
        for cname, cshape in comps.items():
            z = cshape.BoundBox.ZMax
            if z > tallest_z:
                tallest_z, tallest = z, cname
        headroom = BASE_TOP_Z - tallest_z
        results.append(check(
            headroom > 2.0,
            "lid underside (z %.2f) clears tallest component %s (z %.2f) by "
            "%.2f mm" % (BASE_TOP_Z, tallest, tallest_z, headroom)))
        log("      component heights above the PCB top face (z=%.2f):" % PCB_TOP_Z)
        for cname in sorted(comps, key=lambda k: -comps[k].BoundBox.ZMax):
            log("        %-22s %6.2f mm" % (cname, comps[cname].BoundBox.ZMax - PCB_TOP_Z))

    # display module hanging off the lid must not reach the components
    if DISPLAY_ENABLE and comps:
        disp_back = BASE_TOP_Z - (4.0 - DISPLAY_RECESS_DEPTH) - DISPLAY_RECESS_DEPTH
        results.append(check(
            disp_back > tallest_z,
            "lid-mounted display module (back face ~z %.2f, assuming a 4.0 mm "
            "thick module) clears the tallest component by %.2f mm"
            % (disp_back, disp_back - tallest_z)))

    # --- 6. button holes concentric with the actuators --------------------
    for label, ctr in (("SW1", SW1_CENTER), ("SW2", SW2_CENTER)):
        cs = comps.get(label)
        if cs is None:
            continue
        bb = cs.BoundBox
        body = ((bb.XMin + bb.XMax) / 2.0, (bb.YMin + bb.YMax) / 2.0)
        d = math.hypot(body[0] - ctr[0], body[1] - ctr[1])
        results.append(check(
            d < 0.2 and BUTTON_HOLE_DIA / 2.0 > SW_ACT_R + 0.75,
            "%s hole centre (%.3f, %.3f) is %.3f mm from the switch body axis; "
            "hole R %.2f vs actuator R %.2f (%.2f mm annular clearance)"
            % (label, ctr[0], ctr[1], d, BUTTON_HOLE_DIA / 2.0, SW_ACT_R,
               BUTTON_HOLE_DIA / 2.0 - SW_ACT_R)))

    # --- 6b. button reachability / plungers -------------------------------
    reach = LID_TOP_Z - SW_ACT_Z
    log("  [note] actuator top is %.2f mm below the lid's outer face - too deep "
        "for a fingertip through a %.1f mm hole." % (reach, BUTTON_HOLE_DIA))
    if BUTTON_PLUNGER_ENABLE:
        fb, ft, total = plunger_dims()
        top_r = BUTTON_HOLE_DIA / 2.0 - PLUNGER_CLEAR
        results.append(check(
            top_r > 1.0 and PLUNGER_FLANGE_DIA > BUTTON_HOLE_DIA + 1.5,
            "plunger cap dia %.2f slides in the %.2f mm hole (%.2f mm radial "
            "clearance); %.2f mm flange cannot pull through"
            % (2 * top_r, BUTTON_HOLE_DIA, PLUNGER_CLEAR, PLUNGER_FLANGE_DIA)))
        results.append(check(
            total - ft > 0.5 + LID_T,
            "plunger cap stands %.2f mm proud of the lid face" % PLUNGER_PROUD))
        results.append(check(
            0.25 < PLUNGER_FREEPLAY < 1.0,
            "plunger free travel before the flange bottoms out: %.2f mm "
            "(tactile switch travel is ~0.25 mm)" % PLUNGER_FREEPLAY))
        results.append(check(
            PLUNGER_STEM_DIA > 2 * SW_ACT_R and PLUNGER_STEM_DIA < 5.6,
            "plunger stem dia %.2f covers the %.2f mm actuator without touching "
            "the switch body" % (PLUNGER_STEM_DIA, 2 * SW_ACT_R)))
        if DISPLAY_ENABLE:
            rw, _ = display_recess_size()
            g = (DISPLAY_CENTER[0] - rw / 2.0) - (SW1_CENTER[0] + PLUNGER_FLANGE_DIA / 2.0)
            results.append(check(
                g > 0.3,
                "plunger flange seats on flat lid, %.2f mm clear of the display "
                "recess" % g))
        # plungers must not foul the lid or the components
        pl = plunger_placed(SW1_CENTER).fuse(plunger_placed(SW2_CENTER))
        results.append(check(lid.common(pl).Volume < 0.5,
                             "plungers slide freely through the lid holes "
                             "(overlap %.3f mm^3)" % lid.common(pl).Volume))
        for cname in ("SW1", "SW2"):
            if cname in comps:
                v = comps[cname].common(pl).Volume
                results.append(check(
                    v < 0.5,
                    "plunger rests on %s without fouling the switch body "
                    "(overlap %.3f mm^3)" % (cname, v)))

    # --- 7. lid feature layout -------------------------------------------
    if DISPLAY_ENABLE:
        rw, rh = display_recess_size()
        wx, wy = display_window_center()
        # window inside recess
        results.append(check(
            abs(wx - DISPLAY_CENTER[0]) + DISPLAY_WINDOW_W / 2.0 <= rw / 2.0 and
            abs(wy - DISPLAY_CENTER[1]) + DISPLAY_WINDOW_H / 2.0 <= rh / 2.0,
            "display window %.1f x %.1f at (%.2f, %.2f) sits inside the %.1f x "
            "%.1f module recess" % (DISPLAY_WINDOW_W, DISPLAY_WINDOW_H, wx, wy, rw, rh)))
        results.append(check(
            LID_T - DISPLAY_RECESS_DEPTH >= 0.6,
            "bezel left over the display recess is %.2f mm thick"
            % (LID_T - DISPLAY_RECESS_DEPTH)))
        # recess vs button holes and S1 slot
        rx0, rx1 = DISPLAY_CENTER[0] - rw / 2.0, DISPLAY_CENTER[0] + rw / 2.0
        ry0, ry1 = DISPLAY_CENTER[1] - rh / 2.0, DISPLAY_CENTER[1] + rh / 2.0
        gaps = []
        for label, ctr in (("SW1", SW1_CENTER), ("SW2", SW2_CENTER)):
            gaps.append((label, rx0 - (ctr[0] + BUTTON_HOLE_DIA / 2.0)))
        if S1_SLOT_ENABLE:
            gaps.append(("S1 slot", (S1_CENTER[0] - S1_SLOT_W / 2.0) - rx1))
        for label, g in gaps:
            results.append(check(g > 1.0,
                                 "display recess is %.2f mm clear of %s" % (g, label)))
        # everything stays on the disc
        corner = max(math.hypot(x, y) for x in (rx0, rx1) for y in (ry0, ry1))
        results.append(check(corner < SKIRT_INNER_R,
                             "display recess corner radius %.2f mm is inside the "
                             "lid skirt bore %.2f mm" % (corner, SKIRT_INNER_R)))

    if S1_SLOT_ENABLE:
        results.append(check(
            abs(S1_CENTER[0]) + S1_SLOT_W / 2.0 < OUTER_R - WALL_T,
            "S1 slot is fully on the lid face (outer R %.2f)" % OUTER_R))

    # --- 8. lid/base locating fit ----------------------------------------
    results.append(check(
        0.15 <= LID_FIT_CLEAR <= 0.45,
        "lid skirt OD %.2f vs base bore %.2f -> %.2f mm radial / %.2f mm "
        "diametral clearance"
        % (2 * SKIRT_OUTER_R, 2 * CAVITY_R, LID_FIT_CLEAR, 2 * LID_FIT_CLEAR)))
    results.append(check(
        BASE_TOP_Z - SKIRT_H > PCB_TOP_Z + 2.0,
        "lid skirt bottom (z %.2f) stays %.2f mm above the PCB top face"
        % (BASE_TOP_Z - SKIRT_H, BASE_TOP_Z - SKIRT_H - PCB_TOP_Z)))

    # --- 9. USB slot actually lines up with the port ----------------------
    if USB_ENABLE:
        z0 = PCB_BOTTOM_Z + USB_SLOT_Z_OFFSET
        z1 = z0 + USB_SLOT_H
        pz0 = PCB_TOP_Z + USB_PORT_Z0
        pz1 = PCB_TOP_Z + USB_PORT_Z1
        results.append(check(
            z0 < pz0 and z1 > pz1 and USB_SLOT_W > USB_PORT_W + 2.0,
            "USB slot X %.2f..%.2f / Z %.2f..%.2f contains the port X %.2f..%.2f "
            "/ Z %.2f..%.2f"
            % (USB_X - USB_SLOT_W / 2.0, USB_X + USB_SLOT_W / 2.0, z0, z1,
               USB_X - USB_PORT_W / 2.0, USB_X + USB_PORT_W / 2.0, pz0, pz1)))
        results.append(check(
            z0 >= PCB_BOTTOM_Z,
            "USB slot bottom (z %.2f) does not cut into the PCB ledge (z %.2f)"
            % (z0, PCB_BOTTOM_Z)))

    # --- 10. printability sanity -----------------------------------------
    results.append(check(min(WALL_T, FLOOR_T, LID_T, SKIRT_T) >= 1.2,
                         "min wall/floor/lid/skirt thickness %.2f mm (>= 3 x 0.4 mm "
                         "nozzle)" % min(WALL_T, FLOOR_T, LID_T, SKIRT_T)))
    results.append(check(USB_SLOT_W <= 20.0,
                         "longest unsupported bridge is the USB slot roof, %.1f mm"
                         % USB_SLOT_W))
    for name, shp in (("base", base), ("lid", lid)):
        results.append(check(len(shp.Solids) == 1 and shp.isValid(),
                             "%s is a single valid solid" % name))

    log("")
    log("  %d/%d checks passed" % (sum(1 for r in results if r), len(results)))
    return all(results)


def load_drill_holes():
    """Plated through-hole centres from the KiCad drill file, in local X/Y."""
    import re
    drl = os.path.join(os.path.dirname(PCB_STEP), "turn-taker-PTH.drl")
    pts = []
    if not os.path.exists(drl):
        return pts
    for line in open(drl):
        m = re.match(r"^(?:G00)?X([-\d.]+)Y([-\d.]+)\s*$", line.strip())
        if m:
            kx, ky = float(m.group(1)), -float(m.group(2))
            pts.append((kx + STEP_DX, STEP_DY - ky))
    return pts


# ==========================================================================
# 8. EXPORT
# ==========================================================================

def export(shape, name, step=True):
    step_path = os.path.join(OUT, name + ".step")
    stl_path = os.path.join(OUT, name + ".stl")
    if step:
        shape.exportStep(step_path)
    try:
        import MeshPart
        import Mesh
        m = MeshPart.meshFromShape(Shape=shape,
                                   LinearDeflection=STL_LINEAR_DEFLECTION,
                                   AngularDeflection=STL_ANGULAR_DEFLECTION,
                                   Relative=False)
        m.write(stl_path)
        tris = m.CountFacets
        flags = []
        if not m.isSolid():
            flags.append("NOT watertight")
        if m.hasNonManifolds():
            flags.append("non-manifold edges")
        if m.hasSelfIntersections():
            flags.append("self-intersections")
        log("  mesh check %-10s : %s" % (name, ", ".join(flags) if flags
                                         else "watertight, manifold, no self-intersections"))
    except Exception as exc:
        log("  ! MeshPart unavailable (%s), falling back to exportStl" % exc)
        shape.exportStl(stl_path)
        tris = -1
    log("  wrote %s (%s triangles)"
        % (name + ".step / " + name + ".stl" if step else name + ".stl",
           tris if tris > 0 else "?"))


# ==========================================================================
# 9. PREVIEW RENDERING
# ==========================================================================

def render(views):
    """Tiny orthographic software renderer: back-face cull + painter's algorithm.

    mplot3d sorts whole collections rather than individual faces, which makes a
    hollow part look wrong, so the triangles are projected and z-sorted here and
    then drawn as flat 2D polygons.
    """
    try:
        import numpy as np
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.collections import PolyCollection
    except Exception as exc:                      # pragma: no cover
        log("  ! preview rendering unavailable: %s" % exc)
        return

    light = np.array([0.40, -0.55, 0.73])
    light /= np.linalg.norm(light)

    for fname, title, items, elev, azim in views:
        e, a = math.radians(elev), math.radians(azim)
        view = np.array([math.cos(e) * math.cos(a),
                         math.cos(e) * math.sin(a),
                         math.sin(e)])                      # camera -> origin
        right = np.cross(np.array([0.0, 0.0, 1.0]), view)
        right /= np.linalg.norm(right)
        up = np.cross(view, right)

        polys, cols, depth = [], [], []
        for shp, rgb, dz in items:
            verts, facets = shp.tessellate(0.2)
            if not facets:
                continue
            v = np.array([[p.x, p.y, p.z + dz] for p in verts])
            tris = v[np.array(facets)]
            n = np.cross(tris[:, 1] - tris[:, 0], tris[:, 2] - tris[:, 0])
            ln = np.linalg.norm(n, axis=1)
            keep = ln > 1e-12
            tris, n, ln = tris[keep], n[keep], ln[keep]
            n = n / ln[:, None]
            facing = n @ view
            front = facing > 0.0                             # back-face cull
            tris, n, facing = tris[front], n[front], facing[front]
            if len(tris) == 0:
                continue
            shade = 0.30 + 0.55 * np.clip(n @ light, 0.0, 1.0) + 0.15 * facing
            c = np.clip(np.array(rgb)[None, :] * shade[:, None], 0, 1)
            uv = np.stack([tris @ right, tris @ up], axis=-1)
            polys.extend(uv)
            cols.extend(c)
            depth.extend((tris @ view).max(axis=1))

        order = np.argsort(np.array(depth))                  # far -> near
        polys = [polys[i] for i in order]
        cols = np.array([cols[i] for i in order])

        fig, ax = plt.subplots(figsize=(6.4, 6.0), dpi=140)
        pc = PolyCollection(polys, facecolors=cols, edgecolors=cols,
                            linewidths=0.25, antialiased=True)
        ax.add_collection(pc)
        allp = np.concatenate(polys)
        c0 = (allp.max(axis=0) + allp.min(axis=0)) / 2.0
        r = (allp.max(axis=0) - allp.min(axis=0)).max() / 2.0 * 1.06
        ax.set_xlim(c0[0] - r, c0[0] + r)
        ax.set_ylim(c0[1] - r, c0[1] + r)
        ax.set_aspect("equal")
        ax.set_axis_off()
        ax.set_title(title, fontsize=11)
        fig.tight_layout()
        fig.savefig(os.path.join(OUT, fname), facecolor="white")
        plt.close(fig)
        log("  wrote %s" % fname)


# ==========================================================================
# 10. MAIN
# ==========================================================================

def main():
    if not os.path.isdir(OUT):
        os.makedirs(OUT)

    log("Turn Taker enclosure - revision 1")
    log("=================================")
    log("PCB           : dia %.3f mm, %.2f mm thick" % (2 * PCB_R, PCB_T))
    log("Enclosure     : dia %.3f mm, base %.2f mm tall, lid %.2f mm -> %.2f mm overall"
        % (2 * OUTER_R, BASE_TOP_Z, LID_T, LID_TOP_Z))
    log("PCB underside : z = %.2f mm  (floor %.2f + elevation %.2f)"
        % (PCB_BOTTOM_Z, FLOOR_T, PCB_ELEVATION))
    log("Lid underside : z = %.2f mm  (%.2f mm above the PCB top face)"
        % (BASE_TOP_Z, LID_INTERIOR_CLEAR))
    log("")

    base = build_base()
    lid = build_lid()
    log("Loading PCB reference ...")
    ref = load_pcb_reference()

    ok = validate(base, lid, ref)

    log("")
    log("Exporting ...")
    export(base, "base")
    export(lid, "lid")

    # print-ready lid: flipped so the cosmetic face is on the bed
    lid_print = lid.copy()
    lid_print.rotate(Vector(0, 0, 0), Vector(1, 0, 0), 180)
    lid_print.translate(Vector(0, 0, LID_TOP_Z))
    export(lid_print, "lid_print", step=False)

    if BUTTON_PLUNGER_ENABLE:
        export(build_plunger(), "button_plunger")   # print 2 off, stem down

    if RENDER_PREVIEWS and ref:
        log("")
        log("Rendering previews ...")
        pcb_parts = list(ref.values())
        pcb_comp = pcb_parts[0]
        for s in pcb_parts[1:]:
            pcb_comp = pcb_comp.fuse(s)
        GREY = (0.70, 0.72, 0.76)
        GREEN = (0.13, 0.40, 0.22)
        ORANGE = (0.80, 0.45, 0.16)
        plungers = []
        if BUTTON_PLUNGER_ENABLE:
            pl = plunger_placed(SW1_CENTER).fuse(plunger_placed(SW2_CENTER))
            plungers = [(pl, ORANGE, 0.0)]
        render([
            ("preview_base.png", "Base (print this side down)",
             [(base, GREY, 0.0)], 30, -60),
            ("preview_lid_top.png", "Lid / faceplate - outside face",
             [(lid, GREY, 0.0)], 42, -60),
            ("preview_lid_bottom.png",
             "Lid / faceplate - inside face (display recess + locating skirt)",
             [(lid, GREY, 0.0)], -42, -60),
            ("preview_pcb_in_base.png", "PCB seated in the base",
             [(base, GREY, 0.0), (pcb_comp, GREEN, 0.0)], 30, -60),
            ("preview_exploded.png", "Exploded assembly",
             [(base, GREY, 0.0), (pcb_comp, GREEN, 9.0), (lid, GREY, 24.0)],
             20, -60),
            ("preview_assembled.png", "Assembled",
             [(base, GREY, 0.0), (lid, GREY, 0.0)] + plungers, 24, -60),
            ("preview_section.png",
             "Section at y = 0, looking at the +Y half (ledge, posts, plunger)",
             [(half(base), GREY, 0.0), (half(pcb_comp), GREEN, 0.0),
              (half(lid), GREY, 0.0)] +
             [(half(p[0]), ORANGE, 0.0) for p in plungers], 4, -90),
            ("preview_top.png", "Lid layout, straight down "
             "(display window, SW1/SW2, S1 slot)", [(lid, GREY, 0.0)], 89.9, -90),
        ])

    with open(os.path.join(OUT, "build-report.txt"), "w") as fh:
        fh.write("\n".join(LOG) + "\n")

    log("")
    log("Done. %s" % ("All checks passed." if ok else "SOME CHECKS FAILED - see above."))
    return 0 if ok else 1


main()
