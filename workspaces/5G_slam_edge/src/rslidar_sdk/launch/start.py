from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration


# 直接定义常量
pi = 3.141592653589793  # 定义圆周率

def generate_launch_description():
    
    rsildar_sdk_node = Node(
        namespace="rslidar_sdk",
        package="rslidar_sdk",
        executable="rslidar_sdk_node",
        output="screen",
        parameters=[{"config_path": ""}],
    )
    rviz2_node = Node(
        namespace="rviz2",
        package="rviz2",
        executable="rviz2",
        arguments=["-d", get_package_share_directory("rslidar_sdk") + "/rviz/rviz2.rviz"],
    )

    pointcloud_to_laserscan_node = Node(
        package="pointcloud_to_laserscan",
        executable="pointcloud_to_laserscan_node",
        remappings=[("cloud_in", "/rslidar_points"), ("scan", "/rslidar_scan")],
        parameters=[
            {
                "target_frame": "laser_link",      # 目标坐标系
                "transform_tolerance": 0.01,       # 坐标变换容差
                "min_height": 0.2,                # 点云转换为激光扫描的最小高度
                "max_height": 2.0,                # 点云转换为激光扫描的最大高度
                "angle_min": -pi,                 # 扫描角度最小值
                "angle_max": pi,                  # 扫描角度最大值
                "angle_increment": 0.2/180*pi,    # 角度增量
                "scan_time": 0.1,                 # 扫描时间
                "range_min": 0.2,                 # 最小测量距离
                "range_max": 200.0,               # 最大测量距离
                "use_inf": False,                 # 是否使用无穷大值
                "inf_epsilon": 0.5,               # 无穷大值的阈值，`use_inf=False`时实际上不起作用
            }
        ],
        name="pointcloud_to_laserscan",
    )


    static_transform_publisher_laser_node = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="base_to_laser",
        arguments=[
            "--x", "0", "--y", "0", "--z", "0.3",       # 平移：base_link 到 laser_link 的 Z 轴偏移 0.3 米
            "--qx", "0", "--qy", "0", "--qz", "0", "--qw", "1",  # 旋转（四元数）：无旋转 (w=1)
            "--frame-id", "base_link", "--child-frame-id", "laser_link"
        ],
    )

    static_transform_publisher_imu_node = Node(
        package="tf2_ros",
        executable="static_transform_publisher",
        name="base_to_imu",
        arguments=[
            "--x", "0", "--y", "0", "--z", "0",         # 平移：base_link 到 imu_link 无偏移
            "--qx", "0", "--qy", "0", "--qz", "0", "--qw", "1",  # 旋转（四元数）：无旋转 (w=1)
            "--frame-id", "base_link", "--child-frame-id", "imu_link"
        ],
    )

    # 启动 Cartographer 生成地图的节点
    occupancy_grid_node = Node(
        package="cartographer_ros",
        executable="cartographer_occupancy_grid_node",
        name="cartographer_occupancy_grid_node",
        arguments=["-resolution", "0.05"],
    )


    use_sim_time = LaunchConfiguration("use_sim_time", default="false")
    cartographer_node = Node(
        package="cartographer_ros",
        executable="cartographer_node",
        name="cartographer_node",
        output="screen",
        parameters=[{"use_sim_time": use_sim_time}],
        arguments=[
            "-configuration_directory",
            get_package_share_directory("rslidar_sdk"),
            "-configuration_basename",
            "rs16_lidar_rslidar.lua",
        ],
        remappings=[
            ("/scan", "/rslidar_scan"),
            ("/imu", "/IMU_data"),
        ],
    )

    return LaunchDescription(
        [
            rsildar_sdk_node,            
            pointcloud_to_laserscan_node,
            static_transform_publisher_laser_node,
            static_transform_publisher_imu_node,
            rviz2_node,

            cartographer_node,
            occupancy_grid_node,
        ]
    )
