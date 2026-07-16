import os
import launch
import launch_ros

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node

def generate_launch_description():
    ##############################
    # 1. 参数声明（所有必需参数）
    ##############################

    # 机器人标识参数
    robot_id_arg = DeclareLaunchArgument(
        'robot_id',
        default_value='1',
        description='Unique ID of the robot (1, 2, 3, or 4)'
  )

    # 时间参数
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation (Gazebo) clock if true'
    )

    # 硬件接口参数
    port_name_arg = DeclareLaunchArgument(
        'port_name',
        default_value='can0',
        description='Physical CAN bus port name (e.g. can0, can1)'
    )
    
    # 坐标系参数
    odom_frame_arg = DeclareLaunchArgument(
        'odom_frame',
        default_value='odom',
        description='Odometry frame ID'
    )

    base_link_frame_arg = DeclareLaunchArgument(
        'base_frame',
        default_value='base_link',
        description='Base link frame ID'
    )

    # 车型配置参数
    is_tracer_mini_arg = DeclareLaunchArgument(
        'is_tracer_mini',
        default_value='false',
        description='Set to true if using Tracer Mini model'
    )

    simulated_robot_arg = DeclareLaunchArgument(
        'simulated_robot',
        default_value='false',
        description='Set to true when running in simulation'
    )

    sim_control_rate_arg = DeclareLaunchArgument(
        'control_rate',
        default_value='50',
        description='Control loop frequency (Hz) for simulation'
    )
    
    ##############################
    # 2. 节点配置
    ##############################

    # 获取机器人ID
    robot_id = LaunchConfiguration('robot_id')

    # 节点配置（关键修改：无命名空间，手动添加话题前缀）
    tracer_base_node = Node(
        package='tracer_base',
        executable='tracer_base_node',
        name=['tracer_base_node'],  # 节点名称变为 tracer1, tracer2等
        namespace=['tracer',robot_id],  # 不设置命名空间
        #name=['tracer',robot_id],
        #namespace='',
        output='screen',
        emulate_tty=True,
        additional_env={'ROS_DOMAIN_ID': '36'},
        parameters=[{
            # 基础配置
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'robot_id': robot_id,

            # 硬件接口
            'port_name': LaunchConfiguration('port_name'),

            # 坐标系配置（手动添加前缀）
            'odom_frame': ['tracer', robot_id, '/odom'],
            'base_frame': ['tracer', robot_id, '/base_link'],
	    # 话题配置（手动添加前缀）
            'odom_topic_name': 'odom',
            'cmd_vel_topic_name': 'cmd_vel',
            'tracer_status_topic_name':'tracer_status',
            'light_control_topic_name': 'light_control',

            # 车型配置
            'is_tracer_mini': LaunchConfiguration('is_tracer_mini'),
            'simulated_robot': LaunchConfiguration('simulated_robot'),
            'control_rate': LaunchConfiguration('control_rate'),
        }],
        
        # 显式重映射所有话题
        remappings=[
            # Keep raw wheel odometry TF available for diagnostics without
            # competing with the EKF on the global /tf topic.
            ('/tf', 'raw_tf'),
           # ('/tf_static', 'tf_static'),
            ('/odom', 'odom'),
            ('/cmd_vel', 'cmd_vel'),
            ('/tracer_status', 'tracer_status'),
            ('/light_control', 'light_control'),
        ]
    )

    ##############################
    # 3. 启动描述
    ##############################

    return LaunchDescription([
        # 参数声明
        robot_id_arg,
        use_sim_time_arg,
        port_name_arg,
        odom_frame_arg,
        base_link_frame_arg,
        is_tracer_mini_arg,  # 之前缺失的参数
        simulated_robot_arg,
        sim_control_rate_arg,

        # 节点启动
        tracer_base_node
    ])
