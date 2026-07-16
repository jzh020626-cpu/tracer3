from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            namespace='tracer1',
            output='screen',
            parameters=[{
                'frequency': 50.0,
                'sensor_timeout': 0.05,
                'two_d_mode': True,
                'publish_tf': True,
                'print_diagnostics': True,
                'diagnostic_updater_rate': 1.0,
                'predict_to_current_time': True,
                'transform_time_offset': 0.05,

                # 与你当前消息完全一致的帧名
                'map_frame': 'tracer1/map',
                'odom_frame': 'tracer1/odom',
                'base_link_frame': 'tracer1/base_link',
                'world_frame': 'tracer1/odom',

                # 输入话题（绝对话题）
                'odom0': '/tracer1/odom',
                'odom0_config': [True, True, False,
                                 False, False, True,
                                 True,  True,  False,
                                 False, False, False,
                                 False, False, False],
                'odom0_differential': False,
                'odom0_relative': False,
                'odom0_queue_size': 20,

                'imu0': '/tracer1/IMU_data',
                'imu0_config': [False, False, False,
                                True,  True,  True,
                                False, False, False,
                                True,  True,  True,
                                True,  True,  True],
                'imu0_differential': False,
                'imu0_relative': False,
                'imu0_queue_size': 30,
                'imu0_remove_gravitational_acceleration': True,

                'use_control': False,

                # 过程噪声矩阵（与你 YAML 一致，略长，若需要我可再精简）
            }]
        )
    ])
