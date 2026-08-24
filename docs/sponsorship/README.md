# Sponsorship

Retriever is self funded. This folder holds the dossier sent to companies whose
hardware the project needs.

## Files

| File | What it is |
|---|---|
| `dossier.pdf` | The document to send. Six pages, English |
| `dossier.typ` | Its source, written in [Typst](https://typst.app) |

The PDF is the deliverable, but the source is what matters over time: the
dossier is rebuilt from it every time the project moves on, and a PDF with no
source becomes uneditable the moment its author forgets what was in it.

## What the dossier says

The project, its goal, its architecture and where the build stands, then four
things it needs to reach autonomous outdoor operation: the three custom boards,
an embedded AI compute module, an outdoor 3D LiDAR and a depth camera that
works in daylight. Each is described by what it has to do rather than by a
product reference, so that a sponsor can propose whatever suits them.

In exchange: an integration tutorial for the sponsored part, a field test video
with honest performance data, attribution on the chassis and in the repository,
and private feedback on how the part behaves on a real robot.

## Rebuilding it

```bash
typst compile --root . docs/sponsorship/dossier.typ docs/sponsorship/dossier.pdf
```

The `--root` matters: the dossier pulls its images from `docs/images` and
`docs/images/build`, which sit outside its own folder.

Images with a keyed out background live in `docs/images/cover`. They are the
renders with the flat background turned into transparency, so that the robot
and its shadow sit on the gradient of the cover page rather than in a grey box.

## Sponsors

None yet. This section will list them as they come.
