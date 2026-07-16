from launch import LaunchDescription
from launch_ros.actions import Node, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from ament_index_python.packages import get_package_share_directory
import os

def generate_launch_description():

    rslidar_launch = os.path.join(
        get_package_share_directory('rslidar_sdk'),
        'launch',
        'start.py'
    )

    return LaunchDescription([

        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(rslidar_launch),
            launch_arguments={
                # ⚠️ 关键：在 launch 层做 remap
                'rslidar_points': '/tracer3/rslidar_points',
            }.items(),
        ),
    ])
