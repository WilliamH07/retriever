<!-- GÉNÉRÉ depuis firmware/protocol/protocol.yaml — ne pas modifier -->

# Table des trames — protocole Retriever

Version **0.1.0** · hash `0xC1214F10` · charge utile ≤ 8 octets · entiers petit-boutiste.

| ID | Trame | Émetteur | DLC | Hz | État | Contenu |
|---|---|---|:-:|:-:|:-:|---|
| `0x010` | `SAFETY_STATE` | SAFETY | 8 | 100 | planned | state:u8, cause:u8, arm_blockers:u16, flags:u8, counter:u8, uptime_s:u16 |
| `0x020` | `ESTOP_REQUEST` | HOST | 1 | évt | planned | magic:u8 |
| `0x100` | `CMD_WHEELS_FRONT` | HOST | 6 | 50 | planned | left:i16×0.001, right:i16×0.001, seq:u8, crc8:u8 |
| `0x101` | `CMD_WHEELS_REAR` | HOST | 6 | 50 | planned | left:i16×0.001, right:i16×0.001, seq:u8, crc8:u8 |
| `0x180` | `FB_WHEELS_FRONT` | MOTION_FRONT | 8 | 50 | planned | vel_left:i16×0.001, vel_right:i16×0.001, dpos_left:i16, dpos_right:i16 |
| `0x181` | `FB_WHEELS_REAR` | MOTION_REAR | 8 | 50 | planned | vel_left:i16×0.001, vel_right:i16×0.001, dpos_left:i16, dpos_right:i16 |
| `0x190` | `MOT_STATUS_FRONT` | MOTION_FRONT | 4 | 10 | planned | flags_left:u8, flags_right:u8, temp_c:i8, seq:u8 |
| `0x191` | `MOT_STATUS_REAR` | MOTION_REAR | 4 | 10 | planned | flags_left:u8, flags_right:u8, temp_c:i8, seq:u8 |
| `0x200` | `POWER` | SAFETY | 8 | 20 | planned | v_bus:u16×0.001, v_pack:u16×0.001, i_bus:i16×0.001, flags:u8, seq:u8 |
| `0x201` | `BATTERY` | SAFETY | 8 | 2 | planned | soc_pct:u8, temp_c:i8, current_ma:i32, cycles:u16 |
| `0x202` | `CELLS_A` | SAFETY | 8 | 0.2 | planned | cell1:u16, cell2:u16, cell3:u16, cell4:u16 |
| `0x203` | `CELLS_B` | SAFETY | 8 | 0.2 | planned | cell5:u16, cell6:u16, cell7:u16, cell8:u16 |
| `0x204` | `CELLS_C` | SAFETY | 8 | 0.2 | planned | cell9:u16, cell10:u16, cell_min:u16, delta_mv:u16 |
| `0x210` | `IMU_QUAT` | SAFETY | 8 | 100 | bench | w:i16×6.103515625e-05, x:i16×6.103515625e-05, y:i16×6.103515625e-05, z:i16×6.103515625e-05 |
| `0x211` | `IMU_GYRO` | SAFETY | 8 | 100 | bench | gx:i16×0.0005, gy:i16×0.0005, gz:i16×0.0005, seq:u8, flags:u8 |
| `0x212` | `IMU_ACCEL` | SAFETY | 8 | 100 | bench | ax:i16×0.002, ay:i16×0.002, az:i16×0.002, seq:u8, flags:u8 |
| `0x214` | `IMU_MAG` | SAFETY | 8 | 10 | bench | mx:i16×0.01, my:i16×0.01, mz:i16×0.01, seq:u8, flags:u8 |
| `0x213` | `IMU_STATUS` | SAFETY | 8 | 10 | bench | quat_accuracy:u16×0.0001, status_rot:u8, status_gyro:u8, status_accel:u8, reset_count:u8, dropped:u16 |
| `0x220` | `THERMAL` | SAFETY | 8 | 1 | planned | temp_a_c:i8, temp_b_c:i8, fan_a_pct:u8, fan_b_pct:u8, rpm_a:u16, rpm_b:u16 |
| `0x300` | `TIME_SYNC` | HOST | 8 | 1 | bench | t_host_us:u64 |
| `0x310` | `ARM_REQUEST` | HOST | 2 | évt | planned | arm:u8, magic:u8 |
| `0x320` | `CONFIG` | HOST | 8 | évt | planned | key:u16, value:i32, seq:u8, magic:u8 |
| `0x330` | `LINK_PING` | HOST | 7 | évt | bench | target:u8, seq:u16, t_tx_us:u32 |
| `0x331` | `LINK_PONG` | SAFETY | 7 | évt | bench | source:u8, seq:u16, t_tx_us:u32 |
| `0x7F0` | `LOG` | SAFETY | 8 | évt | bench | header:u8, c0:u8, c1:u8, c2:u8, c3:u8, c4:u8, c5:u8, c6:u8 |
| `0x701` | `HEARTBEAT_SAFETY` | SAFETY | 8 | 10 | bench | state:u8, uptime_s:u16, err_count:u8, protocol_hash:u32 |
| `0x702` | `HEARTBEAT_MOTION_FRONT` | MOTION_FRONT | 8 | 10 | planned | state:u8, uptime_s:u16, err_count:u8, protocol_hash:u32 |
| `0x703` | `HEARTBEAT_MOTION_REAR` | MOTION_REAR | 8 | 10 | planned | state:u8, uptime_s:u16, err_count:u8, protocol_hash:u32 |

## Charge du lien

Calculé sur les cadences déclarées ci-dessus, hors trames événementielles.

- **694.6 trames/s** au total.
- **CAN 500 kbit/s** : ≈ 76.9 kbit/s, soit **15.4 %** du bus (bourrage de bits non compté, majorer d'environ 15 %).
- **Série** : ≈ 9.4 ko/s de charge utile encadrée. À 921 600 bauds 8N1 (92 160 o/s) → **10.2 %**. À 115 200 bauds → **82.0 %**.

Le second chiffre est la raison pour laquelle le banc tourne à 921 600 et non à 115 200 : voir §AC.3.
