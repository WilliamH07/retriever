# Motor interface board

Sits between the four hub motors and the motor controllers, on the Hall sensor
cables. It does two independent jobs at once.

**It changes the wire colour code.** The motor loom and the controller loom do
not agree on two of the five conductors. Rather than a taped adapter that
nobody can read six months later, the board carries the mapping in silkscreen
next to every pad. You follow the printed colour; you do not think.

**It taps the twelve Hall signals for an ESP32**, which counts transitions and
publishes wheel odometry. The tap is a spy, never an intermediary.

> **Hard requirement.** If the ESP32 is removed, unpowered or faulty, the motor
> to controller link must keep working. Nothing in the tap sits in series with
> the passthrough, which stays a purely metallic path.

## Colour mapping

| Signal | Motor side | Controller side |
|---|---|---|
| GND | black | black |
| +5 V | red | red |
| Hall A | yellow | yellow |
| Hall B | **blue** | **white** |
| Hall C | **green** | **orange** |

Only two conductors change. Those two are the whole reason the board exists,
and they are the two someone will get wrong, so they are boxed on the
silkscreen.

## Level shifting

Hall outputs are open collector and the pull up lives in the motor controller,
tied to +5 V. The line therefore swings to 5 V, and ESP32 GPIOs are not 5 V
tolerant. Each tap follows this chain and no other:

```
Hall net (0/5 V) ──[ 1 kR ]──┬──[ 10 nF ]── GND
                             └──> Schmitt buffer ──> GPIO (0/3.3 V)
```

- Buffer: 2 x `SN74LVC14AD`, powered from 3.3 V, inputs 5.5 V tolerant. It is
  an inverter, so the state read by software is the complement of the sensor.
- RC: 1 kR + 10 nF, 10 us, corner near 16 kHz. The useful signal peaks at
  **110 Hz**; the noise to reject is the 16 to 20 kHz power PWM coupled from
  the phase wires. Do not change one value without recomputing the other.
- Buffer input current is a few microamps, so the load seen by the Hall sensor
  is negligible and the passthrough survives the loss of 3.3 V.

## On board

| Block | Detail |
|---|---|
| Passthrough | 8 x 5 way connectors, pin *n* to pin *n*, straight traces |
| Tap | 12 x RC, 2 x SN74LVC14AD |
| ESP32 | DevKitC 38 pin on two 1x19 sockets, 22.86 mm row pitch |
| CAN | `SN65HVD230D`, 3.3 V native, 120 R termination behind a jumper, two parallel 3 pin connectors so the bus passes through without a stub |
| 5 V input | screw terminal, 500 mA PPTC, `SMAJ5.0A` TVS, `SS14` series Schottky, isolation jumper |
| Indicators | 12 activity LEDs, one per Hall channel, driven from the buffer outputs; 2 presence LEDs for 3.3 V and the motor 5 V rail |

## The USB and converter conflict

On a DevKitC the `5V` pin is wired straight to USB VBUS with no diode. Two 5 V
sources in parallel means the higher one back feeds the other, which would push
current into the host computer's USB port.

The series `SS14` drops about 0.35 V, so an external 5.0 V arrives at 4.65 V,
always below USB VBUS. USB wins, the Schottky is reverse biased, nothing flows
back. 4.65 V is comfortably above what the module regulator needs.

**This only holds if the converter is set to 5.0 V.** At 5.5 V it becomes
5.15 V after the diode, above USB, and it back feeds. Hence the `5,0 V MAX`
silkscreen next to the terminal. It is an instruction, not a suggestion.

## Bring up

Never plug a new board straight into the motors.

1. Continuity: `MOT_Jn` pad *k* to `CONTROL_Jn` pad *k*, four connectors, five
   pins, twenty measurements. Then confirm no continuity between neighbours.
2. Power alone, 5.0 V on the terminal, nothing else connected. The green LED
   lights. Measure at the jumper output: **4.65 V**. Reading 5.0 V means the
   Schottky is reversed or shorted, and the computer must not be connected.
3. Current draw of a few milliamps. Above 100 mA, stop and find the short.
4. Fit the ESP32. Nothing should get warm.
5. One motor only, robot off, turn the wheel by hand. Three LEDs blink in
   sequence. One dead LED is a sensor or a wire, not the board.

## Status

Schematic complete, ERC clean. Board is 120 x 79 mm, 2 layers, placement
frozen and routed. Silkscreen and fabrication output pending.

The French specification with the full component list is in
[`specification-fr.md`](specification-fr.md).
