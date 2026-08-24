# Mechanical design

Aluminium extrusion frame, laser cut panels, 3D printed mounts. The design is
complete and the parts are made; this folder holds the files they were made
from.

```
dxf/        laser cutting profiles, one file per flat part
stl/        printable parts, ready to slice
step/       STEP exports, for anyone using a different CAD tool
source/     native CAD files
```

## Laser cut panels

Six flat parts make up the body and the decks. All are cut from sheet, and all
are in `dxf/`.

| File | Part | Material |
|---|---|---|
| `ETG_Plateau_superieur_PMMA_3mm.dxf` | Upper deck | PMMA, 3 mm |
| `Plancher.dxf` | Floor of the payload bay | |
| `Panneau_AVANT.dxf` | Front panel | |
| `Panneau_ARRIERE.dxf` | Rear panel | |
| `Panneau_lateral_DROIT.dxf` | Right side panel and wheel arch | |
| `Panneau_lateral_GAUCHE.dxf` | Left side panel and wheel arch | |

The side panels carry the wheel arches, so their outline is what sets the
wheelbase and the track. Change one and the URDF in
[`ros2_ws/src/retriever_description`](../../ros2_ws/src/retriever_description)
has to follow.

Material and thickness are recorded in the file name where they matter. Fill in
the blanks in the table above as each part is confirmed.

## Printed parts

The corner brackets, the deck brackets, the wheel mounts, the fan housing and
the front section are printed. STL files go in `stl/`.

For each part, note here the material, the layer height, the wall count and
the infill actually used, not the ones intended. A printed bracket that holds a
35 kg machine is a structural part, and the print settings are part of the
specification.

## Frame

The frame is aluminium extrusion, cut and drilled by hand. Lengths and the
drilling pattern belong in `source/`, with the extrusion profile and the corner
connector reference recorded here once confirmed.

## Renders

The renders in [`docs/images`](../../docs/images) are produced from this model
by a Fusion 360 script that fixes the camera, the visual style and the
visibility of each assembly group. A regenerated set is directly comparable to
the current one.
