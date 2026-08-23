# Images

CAD renders of the current assembly, captured from the Fusion 360 viewport by
script. All 1600 x 900, web sized, so the repository stays quick to clone.

## Overall

| File | Content |
|---|---|
| `01-hero-front-right.png` | Three quarter view, front right. README banner |
| `02-hero-rear-left.png` | Three quarter view, rear left |
| `03-low-angle.png` | Low angle, near ground level |
| `04-high-angle.png` | High angle |
| `00-preview.png` | Same as 01, lighter, for embeds |

## Orthographic, for dimensioning

| File | Content |
|---|---|
| `05-front.png` | Front elevation |
| `06-side-right.png` | Right side elevation |
| `07-top.png` | Plan view, front pointing up |
| `08-rear.png` | Rear elevation |

## Internal architecture

| File | Content |
|---|---|
| `09-no-top-plate.png` | Top plate hidden, both decks visible |
| `10-upper-deck.png` | The 3 mm upper deck and its six brackets |
| `11-bare-frame.png` | Aluminium extrusion frame alone |
| `15-electronics-bay.png` | Interior, low voltage side |
| `16-lower-deck-37v.png` | Lower floor, the 37 V compartment |

## Details

| File | Content |
|---|---|
| `12-wheel-mount.png` | Hub motor axle mount |
| `13-deck-bracket.png` | Printed deck bracket in the extrusion slot |
| `14-front-panel.png` | Front panel |

## Regenerating

The renders are produced by a Fusion 360 script that sets the camera, the
visual style and the visibility of each assembly group, then writes the PNGs.
Joints and sketches are hidden first, otherwise their cyan arcs appear over the
wheels. Framing is fixed, so a regenerated set is directly comparable to this
one.

Full resolution originals, 2560 x 1440 for the overall views, are kept outside
the repository.
