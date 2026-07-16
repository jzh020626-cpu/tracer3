from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():

    # 定义一个节点，将点云转换为激光扫描数据
    pointcloud2laser = Node(
            package='pointcloud_to_laserscan', executable='pointcloud_to_laserscan_node',
            remappings=[('cloud_in', '/rslidar_points'),
                        ('scan', '/scan')],
            parameters=[{
                'target_frame': '',
                'transform_tolerance': 0.1,
                'min_height': -0.2, # -20cm
                'max_height': 0.2,  #  20cm
                'angle_min': -3.1415,  # - M_PI
                'angle_max': 3.1415,  # M_PI
                'angle_increment': 0.0349,  # M_PI * 2 / 360.0 = 1 degree
                'scan_time': 0.1,
                'range_min': 0.1, # 10cm
                'range_max': 10.0, # 70m
                'use_inf': True,
                'inf_epsilon': 1.0
            }],
        name='pointcloud_to_laserscan'
    )

    # 返回一个启动描述，包括上述定义的两个节点
    return LaunchDescription([
        pointcloud2laser
    ])

