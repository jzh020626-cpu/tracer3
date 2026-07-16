#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import math
import time
from nav_msgs.msg import Odometry


class PoseMonitor(Node):
    """负责接收 /odometry/filtered 并实时保存位姿信息"""
    def __init__(self, robot_name):
        super().__init__(f'{robot_name}_pose_monitor')
        self.robot_name = robot_name
        self.subscription = self.create_subscription(
            Odometry,
            f'/{robot_name}/odometry/filtered',
            self.odom_callback,
            10
        )

        self.x = 0.0
        self.y = 0.0
        self.yaw = 0.0
        self.orientation = None
        self.has_pose = False

    def odom_callback(self, msg):
        pos = msg.pose.pose.position
        ori = msg.pose.pose.orientation

        # 四元数转 yaw
        t3 = +2.0 * (ori.w * ori.z + ori.x * ori.y)
        t4 = +1.0 - 2.0 * (ori.y * ori.y + ori.z * ori.z)
        yaw = math.atan2(t3, t4)

        self.x = pos.x
        self.y = pos.y
        self.yaw = yaw
        self.has_pose = True


class PDControlNode(Node):
    """使用 PoseMonitor 中的位姿信息进行直线PD控制"""
    def __init__(self, robot_name, pose_monitor: PoseMonitor):
        super().__init__(f'{robot_name}_pd_straight_controller')
        self.pose_monitor = pose_monitor
        self.robot_name = robot_name

        # ====== 参数 ======
        self.declare_parameter('target_distance', 2.0)
        self.declare_parameter('target_speed', 0.1)
        self.declare_parameter('Kp', 0.8)
        self.declare_parameter('Kd', 0.2)
        self.declare_parameter('cmd_vel_topic', f'/{robot_name}/cmd_vel')

        # 读取参数
        self.target_distance = self.get_parameter('target_distance').value
        self.target_speed = self.get_parameter('target_speed').value
        self.Kp = self.get_parameter('Kp').value
        self.Kd = self.get_parameter('Kd').value
        self.cmd_vel_topic = self.get_parameter('cmd_vel_topic').value

        # ====== 发布话题 ======
        self.cmd_pub = self.create_publisher(Twist, self.cmd_vel_topic, 10)

        # ====== 状态变量 ======
        self.start_x = None
        self.start_y = None
        self.last_error = 0.0
        self.finished = False

        # 控制循环 20Hz
        self.timer = self.create_timer(0.05, self.control_loop)
        self.get_logger().info(f"🚗 {robot_name} PD直线控制节点已启动，等待位姿数据...")

    def control_loop(self):
        if not self.pose_monitor.has_pose or self.finished:
            return

        # 初始化起点
        if self.start_x is None:
            self.start_x = self.pose_monitor.x
            self.start_y = self.pose_monitor.y
            self.get_logger().info(
                f"✅ {self.robot_name} 起点记录: x={self.start_x:.3f}, y={self.start_y:.3f}"
            )
            return

        # 当前距离
        dx = self.pose_monitor.x - self.start_x
        dy = self.pose_monitor.y - self.start_y
        distance = math.sqrt(dx ** 2 + dy ** 2)

        # 误差计算
        error = self.target_distance - distance
        d_error = error - self.last_error
        self.last_error = error

        # PD 控制输出
        v = self.Kp * error + self.Kd * d_error
        v = max(min(v, self.target_speed), -self.target_speed)

        # 判断是否到达目标
        if distance >= self.target_distance:
            v = 0.0
            self.finished = True
            self.get_logger().info(f"🎯 {self.robot_name} 到达目标: {distance:.2f} m, 停止小车")

        # 发布速度指令
        twist = Twist()
        twist.linear.x = v
        twist.angular.z = 0.0
        self.cmd_pub.publish(twist)

        # 输出调试信息
        self.get_logger().info(
            f"[{self.robot_name} PD控制] x={self.pose_monitor.x:.2f}, y={self.pose_monitor.y:.2f}, yaw={math.degrees(self.pose_monitor.yaw):.1f}°, "
            f"距离={distance:.2f}m, v={v:.3f}m/s"
        )

        if self.finished:
            self.cmd_pub.publish(Twist())  # 停止
            time.sleep(0.5)
            rclpy.shutdown()


def main(args=None):
    rclpy.init(args=args)

    # 启动三个小车的控制节点
    robots = ['tracer1', 'tracer2', 'tracer3']
    executor = rclpy.executors.MultiThreadedExecutor()

    # 为每个小车启动 PoseMonitor 和 PDControlNode
    for robot in robots:
        pose_monitor = PoseMonitor(robot)
        controller = PDControlNode(robot, pose_monitor)
        executor.add_node(pose_monitor)
        executor.add_node(controller)

    try:
        executor.spin()
    finally:
        rclpy.shutdown()


if __name__ == '__main__':
    main()
