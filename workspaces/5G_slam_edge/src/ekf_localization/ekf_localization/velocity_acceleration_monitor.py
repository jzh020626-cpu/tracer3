#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
import numpy as np

class VelocityAccelerationMonitor(Node):
    def __init__(self):
        super().__init__('velocity_acceleration_monitor')
        
        # 订阅EKF融合后的里程计数据
        self.subscription = self.create_subscription(
            Odometry,
            '/odometry/filtered',
            self.odom_callback,
            10)
        
        # 存储历史数据用于计算加速度
        self.last_velocity = None
        self.last_time = None
        
        self.get_logger().info("Velocity and Acceleration Monitor Started")

    def odom_callback(self, msg):
        # 提取线速度和角速度
        linear_vel = msg.twist.twist.linear
        angular_vel = msg.twist.twist.angular
        
        # 如果有加速度数据直接使用
        if hasattr(msg, 'acceleration'):
            linear_acc = msg.acceleration.linear
            angular_acc = msg.acceleration.angular
        else:
            # 计算加速度（如果EKF没有直接提供）
            linear_acc, angular_acc = self.calculate_acceleration(msg)
        
        # 打印或处理速度和加速度数据
        self.get_logger().info(
            f"Linear Velocity: ({linear_vel.x:.2f}, {linear_vel.y:.2f}, {linear_vel.z:.2f}) m/s\n"
            f"Angular Velocity: ({angular_vel.x:.2f}, {angular_vel.y:.2f}, {angular_vel.z:.2f}) rad/s\n"
            f"Linear Acceleration: ({linear_acc.x:.2f}, {linear_acc.y:.2f}, {linear_acc.z:.2f}) m/s²\n"
            f"Angular Acceleration: ({angular_acc.x:.2f}, {angular_acc.y:.2f}, {angular_acc.z:.2f}) rad/s²"
        )
        
        # 更新历史数据
        self.last_velocity = (linear_vel, angular_vel)
        self.last_time = msg.header.stamp

    def calculate_acceleration(self, current_msg):
        if self.last_velocity is None or self.last_time is None:
            return (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
        
        # 计算时间差（秒）
        dt = (current_msg.header.stamp.sec - self.last_time.sec) + \
             (current_msg.header.stamp.nanosec - self.last_time.nanosec) * 1e-9
        
        if dt <= 0:
            return (0.0, 0.0, 0.0), (0.0, 0.0, 0.0)
        
        # 计算线性加速度
        current_linear = current_msg.twist.twist.linear
        last_linear, last_angular = self.last_velocity
        
        linear_acc = (
            (current_linear.x - last_linear.x) / dt,
            (current_linear.y - last_linear.y) / dt,
            (current_linear.z - last_linear.z) / dt
        )
        
        # 计算角加速度
        current_angular = current_msg.twist.twist.angular
        angular_acc = (
            (current_angular.x - last_angular.x) / dt,
            (current_angular.y - last_angular.y) / dt,
            (current_angular.z - last_angular.z) / dt
        )
        
        return linear_acc, angular_acc

def main(args=None):
    rclpy.init(args=args)
    monitor = VelocityAccelerationMonitor()
    rclpy.spin(monitor)
    monitor.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()