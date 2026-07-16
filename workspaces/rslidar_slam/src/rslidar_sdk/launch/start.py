import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction, RegisterEventHandler
from launch.event_handlers import OnProcessExit
from launch.substitutions import LaunchConfiguration, TextSubstitution
from launch_ros.actions import Node


DEFAULT_ROBOT_NS = "tracer3"


def launch_setup(context, *args, **kwargs):
    robot_ns = LaunchConfiguration("robot_ns").perform(context).strip("/")
    if not robot_ns:
        robot_ns = DEFAULT_ROBOT_NS

    config_path = LaunchConfiguration("config_path").perform(context)
    if not config_path:
        config_path = os.path.expanduser(
            f"~/rslidar_slam/src/rslidar_sdk/config/{robot_ns}.yaml"
        )

    points_topic = f"/{robot_ns}/rslidar_points"
    scan_topic = f"/{robot_ns}/scan"
    base_frame = f"{robot_ns}/base_link"
    lidar_frame = f"{robot_ns}/rslidar"

    rslidar_sdk_node = Node(
        namespace=robot_ns,
        package="rslidar_sdk",
        executable="rslidar_sdk_node",
        name="rslidar_sdk_node",
        output="screen",
        parameters=[{
            "config_path": config_path,
            "config_file": config_path,
        }],
        remappings=[
            ("rslidar_points", points_topic),
            ("/rslidar_points", points_topic),
        ],
    )

    static_tf = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name=f"{robot_ns}_base_to_rslidar",
        output="screen",
        arguments=[
            "0.2", "0.0", "0.15",
            "0.0", "0.0", "0.0", "1.0",
            base_frame,
            lidar_frame,
        ],
    )

    pointcloud_to_laserscan = Node(
        namespace=robot_ns,
        package="pointcloud_to_laserscan",
        executable="pointcloud_to_laserscan_node",
        name="pointcloud_to_laserscan",
        output="screen",
        remappings=[
            ("cloud_in", points_topic),
            ("/cloud_in", points_topic),
            ("scan", scan_topic),
            ("/scan", scan_topic),
        ],
        parameters=[{
            "target_frame": base_frame,
            "transform_tolerance": 0.1,
            "min_height": 0.2,
            "max_height": 0.5,
            "angle_min": -3.1415,
            "angle_max": 3.1415,
            "angle_increment": 0.035,
            "scan_time": 0.1,
            "range_min": 0.6,
            "range_max": 8.0,
            "use_inf": True,
            "inf_epsilon": 1.0,
            "queue_size": 1,
        }],
    )

    return [
        rslidar_sdk_node,
        static_tf,
        pointcloud_to_laserscan,
    ]


def generate_launch_description():
    cleanup_old_lidar = ExecuteProcess(
        cmd=[
            "bash",
            "-lc",
            """
set +e
robot="tracer3"
echo "[planning_lidar] checking old lidar processes for ${robot}"

# Stop old child processes first. This prevents two pointcloud_to_laserscan
# nodes from publishing /${robot}/scan at the same time.
pkill -INT  -f "[p]ointcloud_to_laserscan_node.*__ns:=/${robot}" || true
pkill -INT  -f "[r]slidar_sdk_node.*__ns:=/${robot}" || true
pkill -INT  -f "[s]tatic_transform_publisher.*${robot}/base_link.*${robot}/rslidar" || true

# Stop the SLAM lidar launch if it is still running. Do not match start.py
# here, because this file is also used by the planning launch being started.
pkill -INT  -f "[r]os2 launch rslidar_sdk rslidar_slam_2dscan.launch.py" || true

# A second pass handles launch variants that do not include the namespace in
# process arguments. Each vehicle is a separate host, so this is intentionally
# host-local.
pkill -INT  -f "[p]ointcloud_to_laserscan_node" || true
pkill -INT  -f "[r]slidar_sdk_node" || true
sleep 1
pkill -TERM -f "[p]ointcloud_to_laserscan_node.*__ns:=/${robot}" || true
pkill -TERM -f "[r]slidar_sdk_node.*__ns:=/${robot}" || true
pkill -TERM -f "[s]tatic_transform_publisher.*${robot}/base_link.*${robot}/rslidar" || true
pkill -TERM -f "[r]os2 launch rslidar_sdk rslidar_slam_2dscan.launch.py" || true
pkill -TERM -f "[p]ointcloud_to_laserscan_node" || true
pkill -TERM -f "[r]slidar_sdk_node" || true

echo "[planning_lidar] cleanup done for ${robot}"
"""
        ],
        output="screen",
    )

    return LaunchDescription([
        DeclareLaunchArgument(
            "robot_ns",
            default_value=TextSubstitution(text=DEFAULT_ROBOT_NS),
            description="Robot namespace, for example tracer1/tracer2/tracer3.",
        ),
        DeclareLaunchArgument(
            "config_path",
            default_value=TextSubstitution(text=""),
            description="Path to rslidar_sdk yaml. Empty means ~/rslidar_slam/src/rslidar_sdk/config/<robot_ns>.yaml.",
        ),
        cleanup_old_lidar,
        RegisterEventHandler(
            OnProcessExit(
                target_action=cleanup_old_lidar,
                on_exit=[OpaqueFunction(function=launch_setup)],
            )
        ),
    ])
