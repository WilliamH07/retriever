# Bench measurements

Results go here, one file per measurement, named `M<n>-<short-name>.md`.

Each of these blocks a design decision. Nothing that depends on a measurement
gets wired before the measurement exists.

| # | Measurement | Blocks | Status |
|---|-------------|--------|--------|
| M1 | Battery BMS current cut off threshold | Current budget, main fuse rating, number of driven wheels | to do |
| M2 | Pack voltage at rest and under load, internal resistance | Prospective short circuit current, fuse breaking capacity | to do |
| M3 | BMS connector logic level and pinout | Safety controller to BMS wiring | to do |
| M4 | Motor driver EL and STOP input polarity | The whole stop chain, levels 3 to 5 | to do |
| M5 | Motor driver J1 jumper state | PWM versus analogue command | to do |
| M6 | Motor pole pair count | Odometry constant | to do |
| M7 | Hall output voltage and impedance | Signal conditioning circuit | to do |
| M8 | No load and loaded current per motor | Current budget, cable sizing | to do |
| M9 | SBC USB 3.0 controller model | Whether the depth camera works at all | to do |
| M10 | Total bus input capacitance | Precharge resistor and duration | to do |
| M11 | GNSS supply voltage and protocol | Power rail, ROS 2 driver | to do |
| M12 | Motor driver input capacitor voltage rating | Whether a fully charged pack is safe | to do |

Template for a result file:

```markdown
# M<n> : <name>

**Date:**
**Method:**
**Instruments:**
**Protection in place:**

## Raw data

## Result

## Decision unblocked
```
