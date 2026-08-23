# Power distribution board

Takes the battery rail and fans it out to three buses, each independently
fused, with the protections the traction chain needs.

## The battery, decoded

The pack is marked `10INR19/66-4`, an IEC 61960 designation:

| | |
|---|---|
| `10` | ten cells in series |
| `INR` | lithium NMC, cylindrical |
| `19/66` | 18650 format |
| `-4` | four in parallel |

So 10S4P, 12800 mAh, and 12800 / 4 = **3200 mAh per cell**. Check: 37 x 12.8 =
473.6 Wh against the 474 Wh on the label.

**The rail is not 37 V, it is 30 to 42 V.** 37 V is only the nominal figure.
Full charge at 4.2 V per cell is 42.0 V, BMS cutoff at 3.0 V per cell is 30.0 V.

Every part on this rail must therefore be rated for 42 V with margin, which in
practice means **choosing 60 V parts**. A converter sold as "8 to 40 V", the
most common category on the market, is destroyed by the first full charge. This
is the single most common way this kind of board dies.

## Current, and why the 100 A breaker was wrong

3200 mAh 18650s are energy cells, not power cells, typically 2C to 3C
continuous. Four in parallel gives 26 to 40 A at cell level, and the BMS on a
pack like this usually limits first, around 20 to 30 A continuous.

A 100 A breaker will never trip before the BMS cuts or the cabling heats.
Protection that never acts before the thing it protects is decorative, and
worse, it leaves the BMS absorbing the entire fault. **The main fuse is 40 A.**

| Element | Protects |
|---|---|
| BMS | the cells: over discharge, overcharge, temperature |
| 40 A main fuse | the cabling, against a hard short |
| Branch fuses | each load, independently of the others |

## Architecture

```
Battery 10S4P, 30 to 42 V
  -> BMS  ->  manual isolator  ->  40 A fuse  ->  distribution board
       bus 37 V   30 A motor controllers | 5 A to 12 V | 3 A to 5 V | 5 A spare
       bus 12 V   SBC | fans | LIDAR | spare
       bus  5 V   Hall board and ESP32 | sensors | spare
```

The onboard computer is a **youyeetoo X1**, which takes **12 V** and a 3 A
supply. There is therefore no 19 V rail: the computer shares the 12 V bus with
the fans and sensors, and the board needs two converters, not three.

Converters are specified by requirement rather than part number, because
availability moves. The eliminating criterion is the input range: **20 to 60 V
minimum**, 12 V at 8 A and 5 V at 5 A out.

They are carried as **replaceable modules on a carrier board**. Designing a
60 V, 8 A switcher from scratch is a project of its own, and a module that
fails is swapped in two minutes instead of condemning the board.

## Protections

| Against | Choice |
|---|---|
| Reverse polarity | keyed connectors, XT90 or Anderson. A diode at 30 A would burn 20 W; a connector that cannot be plugged backwards costs nothing and never heats |
| Bus overvoltage | `SMCJ48A` TVS, 48 V standoff, above the 42 V maximum |
| Regeneration spikes | 2 x 470 uF, 63 V, low ESR |
| Hard short | 40 A main fuse |
| Branch fault | automotive blade fuses in PCB holders |

### Regeneration is the failure nobody plans for

Four hub motors become generators on any downhill or braking. Current flows
back toward the battery and the bus voltage rises. If the BMS disconnects at
that moment, on a full pack, the current has nowhere to go and the bus can
climb well above 42 V within milliseconds. This is what kills power boards on
hub motor robots. The bulk capacitance and the TVS are not optional.

## Copper

IPC-2221, external layer, 30 C rise:

| Current | 1 oz | 2 oz | 3 oz |
|---|---|---|---|
| 40 A | 24.6 mm | 12.3 mm | 8.2 mm |
| 30 A | 16.5 mm | 8.3 mm | 5.5 mm |
| 10 A | 3.6 mm | 1.8 mm | 1.2 mm |

**2 oz copper**, 37 V bus poured 10 mm wide on both faces and stitched with
vias every 5 mm. An ordinary two layer board carries 30 A that way; no bolted
busbar is needed.

## Precharge

The build uses a manual isolator, no contactor. The motor controllers have
large input capacitors which, at the instant the isolator closes, behave as a
short: several hundred amps for a few milliseconds, an arc, and contacts that
pit a little more each time.

A **100 R, 10 W resistor** permanently across the isolator, or in series with a
momentary button, fixes it. Press for two seconds, the capacitors charge
gently, then close the isolator onto an already equalised voltage. It lives on
the loom, not on the board.

## Status

Specified, not yet drawn. Blocking item: the BMS continuous rating from the
pack label, which sets every figure downstream. The full French specification
is in [`specification-fr.md`](specification-fr.md).
