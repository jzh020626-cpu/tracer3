import os
from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    # 获取配置文件的路径
    config_file = os.path.join(
        get_package_share_directory('ekf_localization'),
        'config',
        'ekf-time-compensation.yaml'
    )
    
    # 创建EKF节点
    ekf_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_node',
        output='screen',
        parameters=[config_file],
        remappings=[
            ('odometry/filtered', '/tracer2/filtered_odom'),
        ]
    )
    
    # 创建静态TF发布器（新式参数格式）
    static_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_transform_publisher',
        output='screen',
        arguments=['0.0', '0.0', '0.0', '0.0', '0.0', '0.0', 'tracer2/base_link', 'imu_link']
    )
    
    # 创建时间同步诊断节点
    time_sync_node = Node(
        package='diagnostic_aggregator',
        executable='aggregator_node',
        name='diagnostic_aggregator',
        output='screen'
    )
    
    return LaunchDescription([
        ekf_node,
        static_tf,
        time_sync_node
    ])
