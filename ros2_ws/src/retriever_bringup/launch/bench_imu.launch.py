"""Banc B1 — l'IMU de l'ESP32 jusqu'à Foxglove.

    ros2 launch retriever_bringup bench_imu.launch.py
    ros2 launch retriever_bringup bench_imu.launch.py device:=/dev/ttyUSB1
    ros2 launch retriever_bringup bench_imu.launch.py foxglove:=false

Ce que ça démarre :
  retriever_link_bridge  la liaison série et la publication de /imu/data
  foxglove_bridge        le pont WebSocket, port 8765

Dans Foxglove Studio, se connecter à ws://<adresse-du-pc>:8765 puis :
  - panneau 3D            repère fixe « imu_world », le pavé suit le capteur
  - panneau Plot          /imu/data.angular_velocity.z, .linear_acceleration.z
  - panneau Raw Messages  /retriever/imu_status et /retriever/link_status
  - panneau Diagnostics   les trois vérifications du pont

⚠️ Ce fichier est un banc, pas le lancement du robot. Il publie une
transformation imu_world → imu_link depuis l'IMU, ce qui est très pratique pour
voir quelque chose et absolument à proscrire une fois l'EKF en service.

Copyright (c) 2026 William Hanczyk — Apache License 2.0
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    device = LaunchConfiguration("device")
    baudrate = LaunchConfiguration("baudrate")
    foxglove = LaunchConfiguration("foxglove")
    port = LaunchConfiguration("foxglove_port")
    log_level = LaunchConfiguration("log_level")

    params = PathJoinSubstitution(
        [FindPackageShare("retriever_bringup"), "config", "link_bench.yaml"]
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "device",
                default_value="/dev/ttyUSB0",
                description="Port serie de l'ESP32. `ls -l /dev/serial/by-id/` donne "
                "un nom stable qui ne change pas d'un branchement a l'autre.",
            ),
            DeclareLaunchArgument("baudrate", default_value="921600"),
            DeclareLaunchArgument("foxglove", default_value="true"),
            DeclareLaunchArgument("foxglove_port", default_value="8765"),
            DeclareLaunchArgument("log_level", default_value="info"),
            Node(
                package="retriever_link",
                executable="link_bridge",
                name="retriever_link_bridge",
                output="screen",
                emulate_tty=True,
                parameters=[
                    params,
                    {"serial.device": device, "serial.baudrate": baudrate},
                ],
                arguments=["--ros-args", "--log-level", log_level],
            ),
            Node(
                package="foxglove_bridge",
                executable="foxglove_bridge",
                name="foxglove_bridge",
                output="screen",
                condition=IfCondition(foxglove),
                parameters=[
                    {
                        "port": port,
                        # 0.0.0.0 : Foxglove tourne souvent sur une autre machine
                        # que le calculateur du robot.
                        "address": "0.0.0.0",
                        # Les QoS des topics capteur sont BEST_EFFORT ; sans
                        # cela le pont s'abonnerait en RELIABLE et ne recevrait
                        # rien, ce qui ressemble beaucoup a une panne de liaison.
                        "use_compression": False,
                        "send_buffer_limit": 10000000,
                    }
                ],
            ),
        ]
    )
