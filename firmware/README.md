# Firmware

Three ESP32 nodes on the CAN bus.

| Node | Role |
|---|---|
| `esp32_motion/` | Two units, front and rear. Speed control loop at 200 Hz, Hall decoding on the hardware pulse counters, per wheel current limiting. Sets the wheel setpoint to zero if no CAN frame arrives for 150 ms. |
| `esp32_safety/` | One unit. Safety state machine, current budget enforcement, heartbeat watchdogs, precharge sequence, contactor control, BMS telemetry over UART. Owns the hardware SAFE line. |

`protocol/` holds `protocol.yaml`, the single source of truth for the CAN frame
layout, and the generator that produces the C and C++ headers from it. The
computer side and the firmware side are generated from the same file so they
cannot drift apart.

The safety node is deliberately the simplest program in the project. It does
not talk to the network, does not parse anything complex, and does not depend
on the main computer being alive.
