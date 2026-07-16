import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    # EKF节点配置
    config_file = os.path.join(
        get_package_share_directory('ekf_localization'),
        'config',
        'ekf.yaml'
    )
    
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_node',
        output='screen',
        parameters=[config_file]
    )
    
    # 速度加速度监控节点
    monitor_node = Node(
        package='ekf_localization',
        executable='velocity_acceleration_monitor.py',
        name='velocity_acceleration_monitor',
        output='screen'
    )
    
    return LaunchDescription([
        ekf_node,
        monitor_node
    ])