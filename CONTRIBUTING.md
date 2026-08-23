# Contributing

Retriever is a personal project, but issues, questions and suggestions are
welcome, especially from people who have built something similar.

## Before opening a pull request

The architecture dossier in `docs/architecture/` is the reference. If a change
contradicts a decision recorded there, say so in the pull request and explain
why. Decisions are allowed to change; changing one silently is not.

## The rule that matters

Every factual claim in this repository carries one of four markers:

| Marker | Meaning |
|---|---|
| VERIFIED | Comes from a manufacturer datasheet or a standard |
| CONSENSUS | Cross checked community reverse engineering, reliable but not contractual |
| TO MEASURE | Unknown or contradictory. **Nothing gets wired before this is measured.** |
| ASSUMPTION | A value chosen in the absence of data, to be confirmed |

Please keep this discipline. A plausible number without a source is worse than
an admitted gap, because it stops anyone from looking further.

## Safety

This machine carries a lithium pack capable of delivering enough current to
start a fire. Contributions that weaken the stop chain described in
`docs/architecture/04-securite-watchdogs-capteurs.md`, or that move a safety
function from hardware into software, will not be merged.
