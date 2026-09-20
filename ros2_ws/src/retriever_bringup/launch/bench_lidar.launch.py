"""Banc — le YDLIDAR X4 jusqu'à Foxglove.

    ros2 launch retriever_bringup bench_lidar.launch.py
    ros2 launch retriever_bringup bench_lidar.launch.py device:=/dev/ttyUSB1
    ros2 launch retriever_bringup bench_lidar.launch.py foxglove:=false

Ce que ça démarre :
  ydlidar_ros2_driver_node   /dev/ydlidar → /scan, 7 Hz
  static_transform_publisher base_link → laser   ⚠️ aide de banc, voir plus bas
  foxglove_bridge            port 8765

⚠️ `foxglove:=false` si le banc IMU tourne déjà : deux ponts ne peuvent pas
écouter le même port, et le second échoue avec une erreur peu parlante.

⚠️ LA TRANSFORMATION base_link → laser EST UNE AIDE DE BANC. Sur le robot,
c'est l'URDF de `retriever_description` qui décrit où le lidar est monté — un
seul endroit, et c'est lui qui fait foi. Publier la position du capteur depuis
un fichier de lancement est exactement la faute du §I.3 : deux sources pour la
même arête de l'arbre TF, qui finissent par diverger. Les arguments x/y/z/yaw
existent pour pouvoir mesurer avant d'avoir l'URDF, pas pour y rester.

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
    frame = LaunchConfiguration("frame_id")
    parent = LaunchConfiguration("parent_frame")
    foxglove = LaunchConfiguration("foxglove")
    port = LaunchConfiguration("foxglove_port")

    params = PathJoinSubstitution(
        [FindPackageShare("retriever_bringup"), "config", "lidar_bench.yaml"]
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "device",
                default_value="/dev/ydlidar",
                description="Port serie du lidar. /dev/ydlidar est l'alias cree par "
                "la regle udev du pilote ; sans elle, /dev/ttyUSB0 ou ttyUSB1 — et "
                "le numero change des qu'un autre peripherique serie apparait, "
                "l'ESP32 par exemple.",
            ),
            DeclareLaunchArgument("frame_id", default_value="laser"),
            DeclareLaunchArgument("parent_frame", default_value="base_link"),
            DeclareLaunchArgument("x", default_value="0.0"),
            DeclareLaunchArgument("y", default_value="0.0"),
            DeclareLaunchArgument("z", default_value="0.0"),
            DeclareLaunchArgument("yaw", default_value="0.0"),
            DeclareLaunchArgument("foxglove", default_value="true"),
            DeclareLaunchArgument("foxglove_port", default_value="8765"),
            DeclareLaunchArgument("publish_tf", default_value="true"),

            Node(
                package="ydlidar_ros2_driver",
                executable="ydlidar_ros2_driver_node",
                # ⚠️ Ce nom doit rester celui de la cle de premier niveau du
                # fichier de parametres. Un noeud renomme demarre quand meme,
                # avec les valeurs par defaut d'un autre modele de lidar.
                name="ydlidar_ros2_driver_node",
                output="screen",
                emulate_tty=True,
                parameters=[params, {"port": device, "frame_id": frame}],
            ),

            Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                name="bench_laser_tf",
                output="log",
                condition=IfCondition(LaunchConfiguration("publish_tf")),
                arguments=[
                    "--x", LaunchConfiguration("x"),
                    "--y", LaunchConfiguration("y"),
                    "--z", LaunchConfiguration("z"),
                    "--yaw", LaunchConfiguration("yaw"),
                    "--frame-id", parent,
                    "--child-frame-id", frame,
                ],
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
                        "address": "0.0.0.0",
                        "use_compression": False,
                        "send_buffer_limit": 10000000,
                    }
                ],
            ),
        ]
    )
