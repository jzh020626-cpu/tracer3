#!/usr/bin/env python3
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, LogInfo, GroupAction, ExecuteProcess, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration, TextSubstitution
from launch_ros.actions import Node, PushRosNamespace


DEFAULT_ROBOT_ID = "tracer3"
DEFAULT_CONFIG_FILE = "/home/skki/rslidar_slam/src/rslidar_sdk/config/tracer3_slam.yaml"


def generate_launch_description():
    robot_id = LaunchConfiguration('robot_id')
    config_file = LaunchConfiguration('config_file')

    cleanup_old_lidar = ExecuteProcess(
        cmd=[
            'bash',
            '-lc',
            """
set +e
robot="tracer3"
echo "[slam_lidar] checking old lidar processes for ${robot}"
pkill -INT  -f "[p]ointcloud_to_laserscan_node.*__ns:=/${robot}" || true
pkill -INT  -f "[r]slidar_sdk_node.*__ns:=/${robot}" || true
pkill -INT  -f "[s]tatic_transform_publisher.*${robot}/base_link.*${robot}/rslidar" || true
pkill -INT  -f "[r]os2 launch rslidar_sdk start.py.*robot_ns:=${robot}" || true
pkill -INT  -f "[r]os2 launch rslidar_sdk rslidar_2dscan.launch.py.*robot_id:=${robot}" || true
pkill -INT  -f "[p]ointcloud_to_laserscan_node" || true
pkill -INT  -f "[r]slidar_sdk_node" || true
sleep 1
pkill -TERM -f "[p]ointcloud_to_laserscan_node.*__ns:=/${robot}" || true
pkill -TERM -f "[r]slidar_sdk_node.*__ns:=/${robot}" || true
pkill -TERM -f "[s]tatic_transform_publisher.*${robot}/base_link.*${robot}/rslidar" || true
pkill -TERM -f "[p]ointcloud_to_laserscan_node" || true
pkill -TERM -f "[r]slidar_sdk_node" || true
echo "[slam_lidar] cleanup done for ${robot}"
"""
        ],
        output='screen'
    )

    nodes = GroupAction(
        actions=[
            PushRosNamespace(robot_id),

            # 1. 雷达驱动节点
            Node(
                package='rslidar_sdk',
                executable='rslidar_sdk_node',
                name='rslidar_sdk_node',
                output='screen',
                parameters=[{
                    # 'frame_id': 'rslidar',
                    # 'use_sim_time': False, # [增加]
                    'config_file': config_file
                }],
                remappings=[('/rslidar_points', 'rslidar_points')]
            ),

            # 2. 静态坐标变换 (修复动态命名)
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name='lidar_tf_publisher',
                arguments=[
                    '--x', '0.2', '--y', '0', '--z', '0.15',
                    '--qx', '0', '--qy', '0', '--qz', '0', '--qw', '1',
                    # [修复] 动态拼接 frame-id，例如 tracer4/base_link
                    '--frame-id', [robot_id, '/base_link'],
                    '--child-frame-id', [robot_id, '/rslidar']
                ],
                parameters=[{'use_sim_time': False}], # [增加]
                output='screen'
            ),

            # 3. 点云转激光
            Node(
                package='pointcloud_to_laserscan',
                executable='pointcloud_to_laserscan_node',
                name='pc2laser_converter',
                parameters=[{
                    'use_sim_time': False,
                    'queue_size': 1,
                    # 重要：将坐标系转换到 base_link，这样高度过滤就是相对于地面的
                    'target_frame': [robot_id, '/base_link'],
                    'transform_tolerance': 0.1,
                    # 重要：地面以上 0.1 米到 0.5 米。这样雷达既扫不到地面，也扫不到机器人车体
                    'min_height': 0.2,
                    'max_height': 0.5,
                    'angle_min': -3.1415,
                    'angle_max': 3.1415,
                    'angle_increment': 0.035, # 0.0349 稍微有点粗，可以改为 0.0174 (1度)
                    'scan_time': 0.1,
                    'range_min': 0.6,          # 重要：避开车体边缘，设为 0.6
                    'range_max': 8.0,
                    'use_inf': True,
                    'inf_epsilon': 1.0
                }],
                remappings=[
                    ('cloud_in', 'rslidar_points'),
                    ('scan', 'scan')
                ],
                output='screen'
            )
        ]
    )

    return LaunchDescription([
        DeclareLaunchArgument('robot_id', default_value=TextSubstitution(text=DEFAULT_ROBOT_ID)),
        DeclareLaunchArgument('config_file', default_value=TextSubstitution(text=DEFAULT_CONFIG_FILE)),
        LogInfo(msg=['Starting SLAM Lidar for: ', robot_id]),
        cleanup_old_lidar,
        RegisterEventHandler(
            OnProcessExit(
                target_action=cleanup_old_lidar,
                on_exit=[nodes]
            )
        )
    ])
