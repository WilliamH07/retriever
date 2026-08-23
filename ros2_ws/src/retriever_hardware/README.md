# retriever_hardware

The `ros2_control` SystemInterface. Talks to the microcontrollers over
SocketCAN using the frame layout generated from `firmware/protocol/protocol.yaml`.

Supports three modes: mock, loopback on a virtual CAN interface, and real
hardware. The first two let the entire stack be tested in continuous
integration without a robot.
