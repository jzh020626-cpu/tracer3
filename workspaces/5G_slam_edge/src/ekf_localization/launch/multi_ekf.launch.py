from launch import LaunchDescription
from launch_ros.actions import Node

def ekf(ns):
    return Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        namespace=ns,
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

            'map_frame': f'{ns}/map',
            'odom_frame': f'{ns}/odom',
            'base_link_frame': f'{ns}/base_link',
            'world_frame': f'{ns}/odom',

            'odom0': f'/{ns}/odom',
            'odom0_config': [True, True, False,
                             False, False, True,
                             True,  True,  False,
                             False, False, False,
                             False, False, False],
            'odom0_differential': False,
            'odom0_relative': False,
            'odom0_queue_size': 20,

            'imu0': f'/{ns}/IMU_data',
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
        }]
    )               

def generate_launch_description():
    return LaunchDescription([
        ekf('tracer1'),
        ekf('tracer2'),
        ekf('tracer3'),
    ])
