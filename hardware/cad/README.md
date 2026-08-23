# Mechanical design

Aluminium extrusion frame, laser cut panels, 3D printed mounts.

```
source/     native CAD files
step/       STEP exports, for anyone using a different tool
stl/        printable parts, ready to slice
dxf/        laser cutting profiles
```

Print settings, material and post processing notes for each printed part
belong in this README as the parts are finalised.

## Parts released so far

| File | Part | Process | Notes |
|---|---|---|---|
| `dxf/upper-deck-550x276.dxf` | Upper deck | Laser cut | 550 x 276 mm, M4 grid at 25 mm pitch, 181 holes, two cable slots. See the thickness note below |
| `stl/deck-bracket-standard.stl` | Deck bracket, standard | 3D print | 4 off. 100 x 30 x 69 mm, ~55 g in PETG |
| `stl/deck-bracket-rear.stl` | Deck bracket, rear station | 3D print | 2 off. Shelf offset 35 mm forward to clear the fan mounts |

### Upper deck, thickness

The deck sits on six supports, the largest span being 270 x 276 mm. Under 8 kg
distributed:

| Material | Immediate deflection | With creep |
|---|---|---|
| PMMA 3 mm | 8.8 mm | ~18 mm |
| PMMA 6 mm | 1.1 mm | ~2.2 mm |
| Aluminium 3 mm | 0.4 mm | 0.4 mm |

3 mm PMMA is not usable here: the deflection becomes permanent, which is a
property of the material and not of the cut. The DXF is unchanged whichever
thickness is ordered, only the M4 screw length changes.

PMMA is also Euroclass E, it ignites easily and drips while burning, directly
above the 37 V compartment. Polycarbonate is self extinguishing and cuts on the
same machine; aluminium settles both the stiffness and the fire question.

### Deck brackets

Print flat on the back face, the one that meets the extrusion. Build height
30 mm, no supports, and the layers then work the right way at the shelf root.
PETG or ASA, 4 perimeters, 40 % infill. Two M8 hammer nuts per bracket: one
alone lets the bracket rotate, and it is the couple between the two screws that
carries the shelf moment.
