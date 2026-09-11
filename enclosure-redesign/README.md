# Turn Taker — parametric enclosure (revision 1)

A code-driven replacement for the simple open-topped cup in
`../turn-taker/turn-taker/turn-taker/turn-taker.FCStd`. Nothing in the original
KiCad / FreeCAD / STEP files was modified — they are only read.

```
enclosure-redesign/
    enclosure.py        the whole model, parameters at the top
    README.md
    output/
        base.step  base.stl
        lid.step   lid.stl
        lid_print.stl           lid flipped + dropped to z=0, ready to slice
        button_plunger.step/.stl  optional, print 2 off
        build-report.txt        full validation log from the last run
        preview_*.png
```

## Building

```bash
cd enclosure-redesign
freecadcmd enclosure.py          # ~70 s, regenerates everything in output/
```

Requires FreeCAD ≥ 1.0 (1.1.1 here) with `numpy` + `matplotlib` for the
previews. No GUI, no CadQuery, no virtualenv — CadQuery was not installed and
FreeCAD headless already reads the KiCad STEP directly, so there was no reason
to add a dependency. Editing the model means editing the parameter block at the
top of `enclosure.py` and re-running; the script re-validates and re-exports
every time.

## Coordinate system

| | |
|---|---|
| X, Y | centred on the round PCB axis, Y up when looking at the component side |
| Z | 0 at the outside bottom of the base, +Z toward the lid |

Mapping from the KiCad STEP export:

```
x_local = x_step - 181.533565
y_local = y_step +  86.500000      (the STEP already has y = -kicad_y)
z_local = z_step + PCB_BOTTOM_Z    (the STEP has the PCB underside at z = 0)
```

## What the CAD told us

Everything below marked **derived** came out of `turn-taker.kicad_pcb`,
`turn-taker-PTH.drl` or `turn-taker.step`. Nothing here was guessed.

| Item | Value | Source |
|---|---|---|
| **PCB diameter** | **65.067 mm** (R 32.533565) | `Edge.Cuts` circle, centre (181.533565, 86.5) |
| PCB thickness | 1.51 mm in the STEP, **1.60 mm used** | STEP solid / KiCad nominal |
| Longest pin protrusion below the board | **1.91 mm** (SW1/SW2 legs) | STEP `ZMin = -1.905` |
| Plated through-holes | 62 | `turn-taker-PTH.drl` |
| Mounting holes usable for the case | **none** | the only NPTH are 4 holes inside the Pico footprint |
| Bottom-side components | **none** | `B_Paste.gbr` is empty — everything is THT |
| SW1 actuator | R 1.75 mm at (−19.884, +0.250), top 4.385 mm above the board | R1.75 cylinder in the STEP |
| SW2 actuator | R 1.75 mm at (−19.884, −14.250), same height | ditto |
| Tallest component | **J2, 8.54 mm** above the PCB top face | STEP bounding boxes |
| Pico USB micro-B | 7.50 mm wide at local X −0.259, 1.085–3.535 mm above the board, face 2.2 mm inside the board edge | STEP section at the Pico's USB end |
| S1 (power slide switch) | pads at local (18.466, +5.0 / +0.3 / −4.4); courtyard 7.3 × 13.2 mm | `kicad_pcb` pads + `F.CrtYd` |
| Bat_In1 | horizontal JST-PH, entirely inside R 29.7 | STEP |

Net check from the schematic: **SW1 → GPIO14** (back / undo),
**SW2 → GPIO15** (next person), **J1 → the OLED** (GND, +5V, GP4/SDA, GP5/SCL),
**J2 + S1 → battery switching**, **Bat_In1 → battery**.

Component heights above the PCB top face:

| | mm |
|---|---|
| J2 (power header) | 8.54 |
| J1 (display connector) | 6.00 |
| Bat_In1 | 4.85 |
| SW1 / SW2 | 4.30 |
| A1 Pico (incl. USB) | 3.70 |

## The enclosure

Overall **Ø70.067 × 26.80 mm**. Base 24.60 mm tall, lid 2.20 mm on top of it.

### Key dimensions

| Parameter | Value |
|---|---|
| PCB radial clearance (`PCB_RADIAL_CLEAR`) | 0.50 mm → cavity R 33.034 |
| Wall thickness (`WALL_T`) | 2.00 mm → outer R 35.034 |
| Floor thickness (`FLOOR_T`) | 2.00 mm |
| Lid thickness (`LID_T`) | 2.20 mm |
| PCB elevation above the floor (`PCB_ELEVATION`) | 5.00 mm → PCB underside at z = 7.00, top face at z = 8.60 |
| PCB support height | 5.00 mm (3.09 mm spare over the 1.91 mm pin protrusion) |
| Lid internal clearance (`LID_INTERIOR_CLEAR`) | 16.00 mm above the PCB top face → lid underside at z = 24.60, **7.46 mm of headroom over J2** |
| Lid fit clearance (`LID_FIT_CLEAR`) | 0.30 mm radial / 0.60 mm diametral |
| Lid skirt | Ø65.47 outer, 2.00 mm thick, 4.00 mm deep, 0.80 mm lead-in chamfer |

### PCB support

Two things hold the board, and neither touches a solder joint:

1. **A continuous annular ledge** at z = 7.00, inner radius 31.43 mm
   (`LEDGE_W = 1.6`), giving **1.10 mm of bearing** all the way round. The
   outermost through-hole in the whole board is at R 29.11, so the ledge is
   clear of every pin by construction. Because the cavity *widens* going up,
   the ledge is a step, not an overhang — it needs no print support.
2. **Nine Ø5 mm posts** from the floor to z = 7.00, listed in `SUPPORT_POSTS`.
   The script checks every post against all 62 plated holes; the worst case is
   **4.73 mm** centre-to-centre, against a 4.10 mm requirement (post radius +
   a 1.6 mm assumed solder-joint radius). One post sits deliberately at
   (−19.9, −7.0), between SW1 and SW2, so button presses don't flex the board.

The board drops in from above, sits level, and cannot rock. There is no
retention clip yet — that was explicitly deferred.

### Lid openings

| Opening | Size | Centre (local X, Y) |
|---|---|---|
| **SW1 button** | Ø6.00 mm | (−19.884, +0.250) |
| **SW2 button** | Ø6.00 mm | (−19.884, −14.250) |
| **Display window** | 24.0 × 15.0 mm, R1.2 corners | (−0.70, **−7.00**) |
| Display module recess (in the lid underside) | 28.3 × 28.8 × 1.40 mm deep | (−0.70, −3.40) |
| **S1 power switch slot** | 7.0 × 13.0 mm, R1.5 corners | (18.466, +0.300) |
| **USB micro-B** (in the base wall) | 13.0 × 8.0 mm | X −6.76…+6.24, Z 7.20…15.20 |
| Lid pry notch (base rim) | R6.0 × 1.6 mm deep scallop | at 270°, i.e. (0, −35.03) |

Button openings are 6.00 mm over a 3.50 mm actuator — **1.25 mm of annular
clearance**, so print tolerance and lid misalignment can't jam them.

The USB slot is sized for a plug overmould, not just the receptacle: the port
face sits 2.2 mm inside the board edge and there is another 2.0 mm of wall, so
the plug has to enter the case by about 5 mm before it engages.

### Edge access

- **Pico USB micro-B** — reaches the perimeter, so it gets a side slot. This is
  both the charging/serial port and the BOOTSEL flashing path, so it is not
  optional.
- **S1 power switch** — sits ~10 mm inside the board edge, so a side opening is
  geometrically impossible. It gets a slot in the lid instead (see assumptions).
- **Bat_In1 / J1 / J2** — entirely internal (max radius 29.7 mm); no openings,
  per the brief.

## Printing

0.4 mm nozzle, ordinary PLA/PETG settings. **Neither part needs supports.**

| Part | Orientation | Notes |
|---|---|---|
| `base.stl` | as exported, floor on the bed | Longest bridge is the 13 mm USB slot roof. The PCB ledge is a step that narrows going up, so it self-supports. |
| `lid_print.stl` | as exported (already flipped, outer face on the bed) | Gives the best finish on the visible face. `lid.stl` is the same part in assembly position if you'd rather orient it yourself. |
| `button_plunger.stl` | as exported, stem down — **print 2** | The flange overhang is picked up by a 45° cone. |

Minimum wall anywhere is 2.00 mm, except the 0.80 mm bezel that remains over
the display recess (4 layers at 0.2 mm).

## Assembly

1. Drop the two plungers into the lid's button holes **from underneath** (the
   8.5 mm flange won't pass through the 6 mm hole).
2. Seat the OLED module face-down into the lid recess and tape/glue it; route
   its 4 wires to J1.
3. Lower the PCB into the base — it lands on the ledge and the nine posts.
   Check the USB port lines up with the wall slot.
4. Press the lid on. The skirt locates it; 0.30 mm radial clearance should give
   a light friction fit. To open, push up through the pry notch in the rim.

## Assumptions (things the CAD could not tell us)

These are the only guesses in the model. Each is a parameter near the top of
`enclosure.py`.

1. **Display position and size.** There is *no display footprint on the PCB* —
   the SSD1315 is a separate module on flying leads to J1, so nothing in the
   CAD says where it physically sits. Dimensions assume the ubiquitous 0.96"
   4-pin I²C OLED: **27.3 × 27.8 mm module PCB, 21.74 × 10.86 mm active area**.
   The module is mounted **to the lid** (recess + window), which also keeps it
   clear of everything on the board. Position was chosen so the window centres
   on the midpoint between SW1 and SW2. Adjust `DISPLAY_MODULE_W/H`,
   `DISPLAY_CENTER`, `DISPLAY_WINDOW_W/H` and `DISPLAY_WINDOW_OFFSET`.
   *Measure your actual module before printing the lid.*
2. **S1 actuator.** Its footprint position is derived, but there is no 3D model
   for it, so its body and actuator height are unknown. The 7 × 13 mm lid slot
   covers the courtyard with clearance. With the lid 16 mm above the board this
   is a fingernail/stylus reach, not a comfortable thumb slide — a printed
   slider is a good rev-2 addition.
3. **Button reach.** The brief asked for plain holes so buttons could be pressed
   directly, but J2's 8.5 mm header sets the internal height, which leaves the
   actuators **13.82 mm below the lid's outer face** — far too deep for a
   fingertip through a 6 mm hole. The holes are still plain (a pen works), and
   the optional `button_plunger` bridges the gap properly. Set
   `BUTTON_PLUNGER_ENABLE = False` if you don't want them.
4. **Wiring volume.** `LID_INTERIOR_CLEAR = 16.00` is deliberately generous —
   8.54 mm of it is J2, the remaining 7.46 mm is for the display's flying
   leads and for a Dupont shell on J2 if you use one. Reduce it to ~12 mm if
   you solder wires directly and want a slimmer puck.
5. **Battery.** Bat_In1 implies a LiPo, but nothing in the CAD says where it
   lives. The model assumes it goes in the volume above the board. If you want
   it *under* the board instead, raise `PCB_ELEVATION` — there is currently
   only 5.00 mm there, 1.91 mm of which is pin protrusion.
6. **Solder joint size.** Posts are kept 1.6 mm (`POST_KEEPOUT`) away from any
   hole centre, on top of the post radius.
7. **PCB thickness.** The STEP solid is 1.51 mm; 1.60 mm is used so the pocket
   is never tight.

## Validation

`enclosure.py` runs 35 checks on every build and writes them to
`output/build-report.txt`. They cover: PCB fit and ledge bearing, pin
clearance, every support post against all 62 drilled holes, boolean
intersection of both enclosure parts against the real PCB assembly imported
from the STEP, lid headroom vs. the tallest component, button holes vs. the
actual actuator axes, plunger fit and travel, display window/recess layout and
its margins to the other lid features, skirt clearance, USB slot alignment
against the real port bounding box, minimum wall thickness, bridge length, and
a watertight/manifold/self-intersection check on each exported mesh.

Last run: **35/35 passed**, all three meshes watertight, manifold and free of
self-intersections.

## Previews

| File | |
|---|---|
| `preview_base.png` | base alone |
| `preview_lid_top.png` / `preview_lid_bottom.png` | lid outside / inside |
| `preview_top.png` | lid layout looking straight down |
| `preview_pcb_in_base.png` | PCB seated in the base |
| `preview_exploded.png` | exploded assembly |
| `preview_assembled.png` | assembled |
| `preview_section.png` | vertical section — ledge, posts, plunger, stack-up |
