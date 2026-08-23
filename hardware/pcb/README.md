# Printed circuit boards

Three boards, currently being designed in EasyEDA.

| Board | Function | State |
|---|---|---|
| [`motor_interface/`](motor_interface/) | Hall passthrough with wire colour remapping, level shifting and signal conditioning to the pulse counters, CAN breakout | routed, pre production review |
| [`safety_power/`](safety_power/) | Power distribution, 37 V / 12 V / 5 V buses, fusing, overvoltage and regeneration protection, precharge, current sensing | specified |
| `can_distribution/` | CAN bus distribution with termination, node breakout and supply fan out | in design |

Each board folder should end up containing:

```
schematic.pdf        human readable schematic
gerbers/             fabrication output
bom.csv              bill of materials
pick_and_place.csv   if assembled
README.md            what it does, connectors, test points
```

Source files are exported from EasyEDA. Fabrication output is committed so that
anyone can order the boards without opening a design tool.
