# Credits and attribution

## Clearpath Husky

Retriever is inspired by the [Clearpath
Husky](https://clearpathrobotics.com/husky-a300-unmanned-ground-vehicle-robot/),
an unmanned ground vehicle widely used as a research platform. The influence is
real and is stated openly here rather than hidden.

What was taken:

* The general configuration of the platform: a compact four wheel drive skid
  steer chassis with a flat payload deck. This is a functional layout, not a
  protected design.
* Some ROS 2 package naming conventions, which follow the structure used by the
  open source [husky/husky](https://github.com/husky/husky) repository. That
  repository is published under the BSD 3 Clause licence, which permits reuse
  and redistribution provided the copyright notice is retained.

What was not taken:

* No mechanical CAD file. The Retriever chassis was modelled from scratch, and
  is built from aluminium extrusion, laser cut panels and 3D printed mounts,
  which is a different construction method.
* No electrical design. The power architecture, the CAN bus topology, the
  safety chain and the microcontroller split were designed independently and
  are documented in `docs/architecture/`.
* No firmware and no application code.

**Clearpath Robotics is not affiliated with this project, does not sponsor it,
and does not endorse it.** "Husky" is a product name belonging to Clearpath
Robotics. Retriever is an independent project and carries its own name for
exactly that reason.

## Upstream open source

| Project | Licence | Used for |
|---|---|---|
| [ROS 2 Jazzy](https://docs.ros.org/en/jazzy/) | Apache 2.0 | Middleware and tooling |
| [Nav2](https://github.com/ros-navigation/navigation2) | Apache 2.0 | Navigation stack |
| [ros2_control](https://github.com/ros-controls/ros2_control) | Apache 2.0 | Hardware abstraction |
| [robot_localization](https://github.com/cra-ros-pkg/robot_localization) | BSD 3 Clause | State estimation |
| [ros2_socketcan](https://github.com/autowarefoundation/ros2_socketcan) | Apache 2.0 | CAN interface |
| [husky/husky](https://github.com/husky/husky) | BSD 3 Clause | Package naming conventions |
| [ceva-dsp/sh2](https://github.com/ceva-dsp/sh2) | Apache 2.0 | BNO085 sensor hub stack, vendored as a submodule at v1.4.0 and unmodified. The ESP32 SPI port is this project's own. |
| [foxglove_bridge](https://github.com/foxglove/ros-foxglove-bridge) | MIT | WebSocket bridge for visualisation |
| [diagnostic_updater](https://github.com/ros/diagnostics) | BSD 3 Clause | Diagnostics |

## Prior work by the author

Retriever follows a full rebuild of the [Niryo
One](https://github.com/NiryoRobotics/niryo_one), an open source six axis
robotic arm. That project is where most of the practical knowledge behind this
one came from.

## Algorithms

The serial framing uses Consistent Overhead Byte Stuffing, described in
Cheshire & Baker, *Consistent Overhead Byte Stuffing*, IEEE/ACM Transactions on
Networking, 1999. The implementation is this project's own; the idea is theirs.
