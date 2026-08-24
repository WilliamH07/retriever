# Safety bus distribution board

Carries the CAN bus and the `/SAFE` line to every node, and feeds each of them
5 V through its own protection. One trunk in, eight branches out.

**It has no microcontroller, and that is the point.** A board that sits on the
safety path and can crash, hang, or need reflashing is a liability. Everything
here is passive or analogue. There is nothing to debug at three in the morning.

It replaces the board previously called `can_distribution`. The rename is not
cosmetic: the `/SAFE` line runs to exactly the same four nodes as the CAN bus,
over exactly the same route. Pulling two looms side by side would have been
absurd.

## Why this board is built first

It is the only one of the three that no measurement blocks. `motor_interface`
waits on the Hall signal levels, `safety_power` waits on the BMS cut-off
threshold and the pack identity. This one waits on nothing.

Building it first puts three ESP32s on a working CAN bus on the bench within a
week, which is where the firmware, the frame protocol and `protocol.yaml` get
written — while the power measurements happen in parallel. That is the correct
use of the critical path.

## The trunk

Six conductors, one connector type, everywhere on the robot.

| Pin | Net | Wire |
|---|---|---|
| 1 | `5V_LOGIC` | orange |
| 2 | `CANH` | green |
| 3 | `CANL` | brown |
| 4 | `/SAFE` | violet |
| 5 | `GND_LOGIC` | black, white tracer |
| 6 | `SHIELD` | — |

Molex Micro-Fit 3.0, 6 circuits, 600 V, positive latch, 5.5 A per circuit at
20 AWG. Every node gets **two in parallel** so the bus passes through it without
a stub.

Violet is reserved for `/SAFE` itself. The e-stop loops and the brake command use
violet with a tracer — close enough to read as "safety" at a glance, distinct
enough that you cannot splice one into the other.

## Topology, stated plainly

A board with eight connectors *is* a star, and ISO 11898-2 asks for a line with
two terminations and short drops. At 500 kbit/s the bit time is 2 µs and a 30 cm
stub costs about 3 ns round trip, so a star works comfortably. That is not the
problem.

The problem is that if nobody writes it down, somebody eventually adds a three
metre drop. So the rule is on the silkscreen: **`STUB MAX 30 cm`**.

```
   Daisy chain — the normal case      Star — bench and fault-finding
   ┌───┐  ┌───┐  ┌───┐                     ┌──── trunk ────┐
   │IN │──│OUT│  │IN │── …                 │  │  │  │  │  │
   └───┘  └───┘  └───┘                    n1 n2 n3 n4 n5 n6
   120 Ω at the two physical ends      120 Ω on two branches only
```

## Termination you can verify

Split termination, always: 2 × 60 Ω with `CSPLIT` from the midpoint to ground.
It costs one capacitor and it cuts common-mode emission sharply. On a robot
where the CAN pair runs past four BLDC choppers, that is not a refinement.

Selectable by jumper at two positions, each with its own blue LED, so a glance
tells you how many are active.

And the acceptance test is printed on the board, because a termination jumper is
the classic thing nobody can account for six months later:

> **Bus unpowered, measure between `TP_CANH` and `TP_CANL`:**
> **60 Ω → correct.** 120 Ω → only one termination. 40 Ω → three.

Five seconds, no instrument beyond a multimeter.

## Per branch, and why each part is there

| Part | Against |
|---|---|
| `TCAN1042HVDR` at each node — not on this board | A power line touching `CANH`. See below |
| `NUP2105L` across CANH/CANL | ESD and **transients only** — see the caveat below. Clamps to 40 V at 5 A, where a `PESD1CAN` needs 70 V at 3 A |
| `ACT45B-510-2P-TL003` common mode choke | The 16–20 kHz chopping coupled from the phase wires. 200 mA rated, 1 Ω DCR |
| `1812L050/30` PPTC on the 5 V feed | One node shorting must not take down the other seven |
| **PPTC 500 mA in series with the branch ground** | The failure nobody plans for — see below |
| One green LED per branch | Knowing at a glance which node is actually powered |

### The transceiver choice belongs here even though the part sits elsewhere

The `SN65HVD230` in the original architecture is rated **−4 V to +16 V** on its
bus pins. This robot runs a 42 V rail in the same loom. A single contact between
a power conductor and `CANH` destroys **every transceiver on the bus at once** —
all four nodes, simultaneously.

The `TCAN1042HV` is rated **±70 V**. The same fault becomes survivable. It costs
a 5 V rail on each node instead of running natively at 3.3 V, and the `VIO` pin
handles the logic side. That is a good trade.

⚠️ **But do not oversell it.** The `NUP2105L` on every branch has a 24 V standoff
and breaks down between 26 and 32 V. A **sustained** 42 V contact makes it
conduct continuously and destroys it — on all eight branches — whatever the
transceiver is rated for. The TVS survives transients, not a permanent fault; it
fails short, which at least pulls the bus down visibly rather than letting the
fault propagate. The transceiver's ±70 V rating is what saves the *nodes*, and
the branch ground PPTC is what stops the fault current. Replacing eight TVS
diodes after a wiring mistake is the acceptable outcome here — not the same as
"nothing breaks".

Two things that will cost you an afternoon if you miss them: `STB` has an
internal pull-up, so **a floating `STB` pin silently puts the transceiver in
standby** — tie it to ground. And it is the `V` suffix, not the `H`, that brings
the `VIO` pin.

### The ground fuse

If a node's ground comes loose on the power side while the CAN cable stays
plugged in, that node's entire return current goes through a 0.5 mm² conductor
sized for a few tens of milliamps. It melts, or it heats and drags every ground
reference on the robot with it.

A 500 mA PPTC in series with each branch ground costs twenty cents and turns
that into a node that simply stops talking.

## `/SAFE`

Active low, open drain, wired-OR, referenced to **3.3 V** — not 5 V, so it lands
directly on an ESP32 pin with no level shifting.

**One** pull-up in the whole system: 1 kΩ to `3V3_HOTEL` on `safety_power`, **in
series with the mushroom's NC contact**. **One** pull-down, also on
`safety_power`: 10 kΩ. Every other board carries a 1 MΩ pull-down only, to define
its own state when it is unplugged from the trunk.

> A first draft of this board put a 10 kΩ pull-down on *every* board. With three
> boards that is 3.33 kΩ against a 4.7 kΩ pull-up: the high level lands at
> **2.07 V**, below the ESP32's 2.48 V VIH. The line would have read low
> permanently and the robot would never have released its brakes. One pull-up,
> one pull-down, and a level calculation written down — 3.3 × 10/(1+10) =
> **3.00 V**, 0.52 V of margin.

The mushroom's contact is **normally closed and in series with the pull-up**, not
across the line to ground. Released, the contact is closed, the pull-up is
connected, the line is high, the robot is permitted to run. Pressed — or the wire
cut, or the connector unplugged — the pull-up is disconnected and the pull-down
takes the line to ground. That is the only arrangement worth having, and it is
the opposite of what "NC contact pulls the line low" would suggest.

> ⚠️ **Read this before wiring `/SAFE` to the motor controllers.** Pulling
> `/SAFE` low does **not** brake the motors. On the ZS-X11H, `STOP` is active
> low and puts the drive into **coast**, and `BRAKE` is active **high**, so a
> released line releases the brake. The two-stage e-stop described in the
> architecture dossier assumes the opposite. The fix lives on
> `motor_interface` — a pull-up to 5 V sitting **on the loom, at the controller
> end**, which an open-drain MOSFET contradicts only while a hardware AND gate
> sees both `/SAFE` high and a live MCU. Lose any of them and `BRAKE` goes high.
> Putting the pull-up on the loom rather than the board is what makes unplugging
> the board brake rather than coast. See that board's specification.

## Node allocation

| Branch | Node |
|---|---|
| 1 | ESP32-SAFETY, on `safety_power` |
| 2 | ESP32-MOTION front |
| 3 | ESP32-MOTION rear |
| 4 | USB-CAN adapter to the X1 — `candleLight`/`gs_usb` firmware, not `slcan` |
| 5–7 | free — arm, turret, smart sensor, fifth wheel |
| 8 | bench |

Four nodes today, eight positions. The architecture brief asked for growth
without rewiring; this is what that looks like in copper.

## Bus load, for reference

675 frames per second at roughly 110 bits each, on 500 kbit/s, is about **15 %**
occupancy. There is a great deal of room.

## Bring up

Never plug a new board into the robot.

1. **Continuity.** Trunk pin *n* to branch pin *n*, eight branches, six pins.
   Then confirm no continuity between neighbours. Forty-eight measurements plus
   the shorts check.
2. **Terminations off, ohmmeter across `TP_CANH`/`TP_CANL`.** Expect open.
   Fit one jumper: 120 Ω. Fit the second: **60 Ω.**
3. **5 V alone** on the trunk, nothing else connected. Eight green LEDs.
   Current draw of a few milliamps. Above 50 mA, stop and find the short.
4. **`/SAFE` idle.** With the trunk pull-up present, every branch reads high.
   Short `/SAFE` to ground at any branch: every branch reads low. That is the
   wired-OR working.
5. **Two nodes, loopback.** Two ESP32s, 500 kbit/s, one sending, one receiving.
   Then unplug one mid-traffic and watch the other go bus-off and recover with
   `restart-ms 100`.
6. **Ground fuse.** Deliberately draw 1 A through a branch ground. The PPTC
   should trip. Better to learn this on the bench than in the field.

## Status

Specified. Nothing blocks it — no measurement, no pending decision. It is the
board to draw first.

Connector-by-connector pinout in [`hardware/icd.yaml`](../hardware/icd.yaml).
Component justifications, with datasheet URLs, in
[`09-validation-composants.md`](../architecture/09-validation-composants.md).
