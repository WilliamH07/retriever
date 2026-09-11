# retriever_link

The link layer, computer side: two interchangeable transports behind one
interface, and the node that turns frames into standard ROS 2 messages.

| | |
|---|---|
| `SerialTransport` | COBS framing over a USB serial port. The bench transport. |
| `SocketCanTransport` | CAN 2.0A over SocketCAN. Written, never run — the board does not exist yet. |
| `link_bridge` | Publishes `/imu/data`, `/imu/mag`, link and node status, diagnostics, and firmware logs into `/rosout`. |

The framing is not reimplemented here. This package compiles the same C files
as the firmware, from `firmware/components/retriever_link/portable/`. One
implementation, so the two ends of the cable cannot disagree.

`imu_conversion.cpp` holds everything about units, frames and covariances, in a
translation unit with no ROS node and no hardware, tested on its own. Silent
conversions are what make EKFs diverge, and they read better alone.

See `docs/architecture/13-architecture-logicielle-liaison.md`, sections AB and AF.
