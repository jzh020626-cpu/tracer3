from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, TextSubstitution
from ament_index_python.packages import get_package_share_directory
import os


DEFAULT_ROBOT_NS = "tracer3"


def generate_launch_description():
    start_launch = os.path.join(
        get_package_share_directory("rslidar_sdk"),
        "launch",
        "start.py",
    )

    robot_id = LaunchConfiguration("robot_id")

    return LaunchDescription([
        DeclareLaunchArgument(
            "robot_id",
            default_value=TextSubstitution(text=DEFAULT_ROBOT_NS),
            description="Robot ID, for example tracer1/tracer2/tracer3.",
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(start_launch),
            launch_arguments={"robot_ns": robot_id}.items(),
        ),
    ])
