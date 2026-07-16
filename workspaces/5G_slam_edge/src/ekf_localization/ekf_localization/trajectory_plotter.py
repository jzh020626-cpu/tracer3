#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
import matplotlib.pyplot as plt
import os
from datetime import datetime
import math
from nav_msgs.msg import Odometry


class PoseMonitor(Node):
    """负责订阅 /odometry/filtered 并保存位姿"""
    def __init__(self, robot_name):
        super().__init__(f'{robot_name}_pose_monitor')
        self.robot_name = robot_name
        self.subscription = self.create_subscription(
            Odometry, f'/{robot_name}/odometry/filtered', self.odom_callback, 10
        )
        self.x = 0.0
        self.y = 0.0
        self.yaw = 0.0
        self.has_pose = False

    def odom_callback(self, msg):
        pos = msg.pose.pose.position
        ori = msg.pose.pose.orientation
        yaw = math.atan2(2 * (ori.w * ori.z + ori.x * ori.y),
                         1 - 2 * (ori.y * ori.y + ori.z * ori.z))
        self.x = pos.x
        self.y = pos.y
        self.yaw = yaw
        self.has_pose = True


class TrajectoryPlotter(Node):
    """轨迹绘制节点，读取 PoseMonitor 的实时数据"""
    def __init__(self, robot_name, pose_monitor: PoseMonitor):
        super().__init__(f'{robot_name}_trajectory_plotter')
        self.pose_monitor = pose_monitor
        self.robot_name = robot_name

        # 数据记录
        self.x_data = []
        self.y_data = []
        self.yaw_data = []
        self.start_pose = None

        # 输出目录
        self.output_dir = '/home/li/hjz/trajectory_plots'  # 将路径改为绝对路径
        os.makedirs(self.output_dir, exist_ok=True)

        # 10Hz 采样
        self.timer = self.create_timer(0.1, self.record_pose)
        self.get_logger().info(f"📈 正在通过 {self.robot_name} 记录轨迹... 按 Ctrl+C 结束并保存图像")

    def record_pose(self):
        """从 PoseMonitor 获取实时位置"""
        if not self.pose_monitor.has_pose:
            return

        x = self.pose_monitor.x
        y = self.pose_monitor.y
        yaw = self.pose_monitor.yaw

        self.x_data.append(x)
        self.y_data.append(y)
        self.yaw_data.append(yaw)

        # 第一次记录起点
        if self.start_pose is None:
            self.start_pose = (x, y, yaw)
            self.get_logger().info(f"✅ {self.robot_name} 起点记录: x={x:.3f}, y={y:.3f}, yaw={yaw:.3f} rad")

    def save_plot(self):
        """生成轨迹图"""
        if not self.x_data:
            self.get_logger().warn("⚠️ 没有轨迹数据，无法生成图像。")
            return

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_path = os.path.join(self.output_dir, f"{self.robot_name}_trajectory_{timestamp}.png")
        
        self.get_logger().info(f"Saving plot to {output_path}")

        try:
            plt.figure(figsize=(6, 6))
            plt.plot(self.x_data, self.y_data, 'b-', label=f'{self.robot_name} EKF轨迹')
            plt.scatter(self.x_data[0], self.y_data[0], c='green', label='起点')
            plt.scatter(self.x_data[-1], self.y_data[-1], c='red', label='终点')

            # 绘制理想参考线（以起点方向为准，4m 长）
            if self.start_pose is not None:
                x0, y0, yaw0 = self.start_pose
                x_ref_end = x0 + 4.0 * math.cos(yaw0)
                y_ref_end = y0 + 4.0 * math.sin(yaw0)
                plt.plot([x0, x_ref_end], [y0, y_ref_end],
                         'r--', label='目标直线 (4m, 初始方向)')

            plt.title(f'{self.robot_name} 小车运行轨迹')
            plt.xlabel('X 位置 (m)')
            plt.ylabel('Y 位置 (m)')
            plt.legend()
            plt.grid(True)
            plt.axis('equal')
            plt.tight_layout()
            plt.savefig(output_path)
            plt.close()
            
            self.get_logger().info(f"✅ {self.robot_name} 轨迹图已保存: {output_path}")
        except Exception as e:
            self.get_logger().error(f"⚠️ 保存轨迹图时出错: {e}")

    def destroy_node(self):
        """重写关闭函数，自动保存轨迹图"""
        self.save_plot()
        super().destroy_node()


def main(args=None):
    rclpy.init(args=args)

    robots = ['tracer1', 'tracer2', 'tracer3']
    executor = rclpy.executors.MultiThreadedExecutor()

    # 为每个小车启动 PoseMonitor 和 TrajectoryPlotter 节点
    for robot in robots:
        pose_monitor = PoseMonitor(robot)
        plotter = TrajectoryPlotter(robot, pose_monitor)

        executor.add_node(pose_monitor)
        executor.add_node(plotter)

    try:
        executor.spin()
    except KeyboardInterrupt:
        for robot in robots:
            print(f"🛑 {robot} 结束记录，保存轨迹图...")
    finally:
        # 确保在程序退出时只调用一次 shutdown
        rclpy.shutdown()


if __name__ == '__main__':
    main()
