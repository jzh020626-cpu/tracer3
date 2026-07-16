#!/usr/bin/env python3
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch.actions import GroupAction
from launch_ros.actions import PushRosNamespace

def generate_launch_description():
    # 参数声明
    robot_id_arg = DeclareLaunchArgument(
        'robot_id',
        default_value='tracer3',
        description='Robot ID (tracer1/tracer2/tracer3/tracer4)'
    )
    
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation (Gazebo) clock if true'
    )

    return LaunchDescription([
        robot_id_arg,
        use_sim_time_arg,
        
        GroupAction([
            # 设置命名空间
            PushRosNamespace(LaunchConfiguration('robot_id')),
            
            # SLAM建图节点 (修正后的参数名)
            Node(
                package='slam_gmapping',
                executable='slam_gmapping',
                name='slam_gmapping',
                output='screen',
                parameters=[{
                    'use_sim_time': LaunchConfiguration('use_sim_time'),
                    'map_frame': 'map',
                    'base_frame': '/tracer3/base_link',
                    'odom_frame': '/tracer3/odom',
                    'map_update_interval': 0.5,
                    'maxUrange': 10.0,
                    'sigma': 0.05,
                    'kernelSize': 1,
                    'lstep': 0.05,
                    'astep': 0.05,
                    'iterations': 5,
                    'lsigma': 0.075,
                    'ogain': 3.0,
                    'minimumScore': 50,
                }],
                remappings=[
                    ('scan', 'scan'),
                    ('map', 'map'),
                    ('map_metadata', 'map_metadata')
                ]
            )
        ])
    ])