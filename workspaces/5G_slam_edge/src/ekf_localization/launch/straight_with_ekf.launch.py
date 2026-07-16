from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # 启动 tracer1 的 PD 控制节点
        Node(
            package='ekf_localization',
            executable='pd_straight_controler',
            name='tracer1_pd_straight_controler',
            namespace='tracer1',
            output='screen',
            parameters=[{
                'target_distance': 2.0,
                'target_speed': 0.1,
                'Kp': 0.8,
                'Kd': 0.2,
                'odom_topic': '/tracer1/odometry/filtered',
                'cmd_vel_topic': '/tracer1/cmd_vel'
            }]
        ),

        # 启动 tracer2 的 PD 控制节点
        Node(
            package='ekf_localization',
            executable='pd_straight_controler',
            name='tracer2_pd_straight_controler',
            namespace='tracer2',
            output='screen',
            parameters=[{
                'target_distance': 2.0,
                'target_speed': 0.1,
                'Kp': 0.8,
                'Kd': 0.2,
                'odom_topic': '/tracer2/odometry/filtered',
                'cmd_vel_topic': '/tracer2/cmd_vel'
            }]
        ),

        # 启动 tracer3 的 PD 控制节点
        Node(
            package='ekf_localization',
            executable='pd_straight_controler',
            name='tracer3_pd_straight_controler',
            namespace='tracer3',
            output='screen',
            parameters=[{
                'target_distance': 2.0,
                'target_speed': 0.1,
                'Kp': 0.8,
                'Kd': 0.2,
                'odom_topic': '/tracer3/odometry/filtered',
                'cmd_vel_topic': '/tracer3/cmd_vel'
            }]
        ),
    ])
