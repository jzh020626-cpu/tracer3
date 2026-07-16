#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus, KeyValue
from sensor_msgs.msg import Imu
from nav_msgs.msg import Odometry
import numpy as np

class SensorDelayMeasurer(Node):
    def __init__(self):
        super().__init__('sensor_delay_measurer')
        
        # 订阅传感器数据
        self.imu_sub = self.create_subscription(
            Imu, 
            '/tracer2/IMU_data', 
            self.imu_callback, 
            10
        )
        
        self.odom_sub = self.create_subscription(
            Odometry, 
            '/tracer2/odom', 
            self.odom_callback, 
            10
        )
        
        # 发布诊断信息
        self.diag_pub = self.create_publisher(
            DiagnosticArray, 
            '/diagnostics', 
            10
        )
        
        # 存储时间戳
        self.imu_timestamps = []
        self.odom_timestamps = []
        
        # 定时器处理延迟计算
        self.timer = self.create_timer(1.0, self.calculate_delays)

    def imu_callback(self, msg):
        # 记录IMU时间戳（纳秒）
        self.imu_timestamps.append(msg.header.stamp.nanosec)
        if len(self.imu_timestamps) > 100:
            self.imu_timestamps.pop(0)

    def odom_callback(self, msg):
        # 记录里程计时间戳（纳秒）
        self.odom_timestamps.append(msg.header.stamp.nanosec)
        if len(self.odom_timestamps) > 100:
            self.odom_timestamps.pop(0)

    def calculate_delays(self):
        diag_array = DiagnosticArray()
        diag_array.header.stamp = self.get_clock().now().to_msg()
        
        # 计算IMU延迟
        if self.imu_timestamps:
            current_time = self.get_clock().now().nanoseconds
            avg_delay = np.mean([current_time - ts for ts in self.imu_timestamps])
            
            imu_status = DiagnosticStatus()
            imu_status.name = "IMU Delay"
            imu_status.level = DiagnosticStatus.OK
            imu_status.message = f"Average delay: {avg_delay/1e6:.2f} ms"
            imu_status.values.append(KeyValue(key="delay_ms", value=f"{avg_delay/1e6:.2f}"))
            diag_array.status.append(imu_status)
        
        # 计算里程计延迟
        if self.odom_timestamps:
            current_time = self.get_clock().now().nanoseconds
            avg_delay = np.mean([current_time - ts for ts in self.odom_timestamps])
            
            odom_status = DiagnosticStatus()
            odom_status.name = "Odometry Delay"
            odom_status.level = DiagnosticStatus.OK
            odom_status.message = f"Average delay: {avg_delay/1e6:.2f} ms"
            odom_status.values.append(KeyValue(key="delay_ms", value=f"{avg_delay/1e6:.2f}"))
            diag_array.status.append(odom_status)
        
        self.diag_pub.publish(diag_array)

def main(args=None):
    rclpy.init(args=args)
    node = SensorDelayMeasurer()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
