# Printed circuit boards

Three boards, currently being designed in EasyEDA.

| Board | Function | State |
|---|---|---|
| `motor_interface/` | Level shifting and signal conditioning between the motion controller and the four BLDC drivers, Hall signal buffering to the pulse counters | in design |
| `safety_power/` | Safety state machine interface, current sensing, precharge sequencing, contactor drive, the hardware SAFE line | in design |
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
