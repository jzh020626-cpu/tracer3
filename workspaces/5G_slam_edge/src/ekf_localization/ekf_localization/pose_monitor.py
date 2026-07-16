#!/usr/bin/env python3
import math
from functools import partial

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, QoSReliabilityPolicy, QoSHistoryPolicy, QoSDurabilityPolicy
from nav_msgs.msg import Odometry


class MultiPoseMonitor(Node):
    def __init__(self):
        super().__init__('pose_monitor_multi')

        # 可配置参数
        self.declare_parameter('robots', ['tracer1', 'tracer2', 'tracer3'])
        self.declare_parameter('topic_base', 'odometry/filtered')  # 不要以斜杠开头
        robots = self.get_parameter('robots').get_parameter_value().string_array_value
        topic_base = self.get_parameter('topic_base').get_parameter_value().string_value

        if not robots:
            robots = ['tracer1', 'tracer2', 'tracer3']

        # QoS：BEST_EFFORT + VOLATILE + Depth 10
        qos = QoSProfile(
            reliability=QoSReliabilityPolicy.BEST_EFFORT,
            durability=QoSDurabilityPolicy.VOLATILE,
            history=QoSHistoryPolicy.KEEP_LAST,
            depth=10
        )

        self.subs = []
        for ns in robots:
            topic = f'/{ns}/{topic_base.lstrip("/")}'
            sub = self.create_subscription(
                Odometry,
                topic,
                partial(self.odom_callback, ns),
                qos
            )
            self.subs.append(sub)
            self.get_logger().info(f'已订阅 {ns}: {topic}')

        self.get_logger().info('已启动多车位姿监听（输出位置与 yaw）')

    @staticmethod
    def quaternion_to_euler(x, y, z, w):
        t0 = +2.0 * (w * x + y * z)
        t1 = +1.0 - 2.0 * (x * x + y * y)
        roll = math.atan2(t0, t1)

        t2 = +2.0 * (w * y - z * x)
        t2 = +1.0 if t2 > +1.0 else t2
        t2 = -1.0 if t2 < -1.0 else t2
        pitch = math.asin(t2)

        t3 = +2.0 * (w * z + x * y)
        t4 = +1.0 - 2.0 * (y * y + z * z)
        yaw = math.atan2(t3, t4)
        return roll, pitch, yaw

    def odom_callback(self, robot_ns: str, msg: Odometry):
        p = msg.pose.pose.position
        q = msg.pose.pose.orientation
        roll, pitch, yaw = self.quaternion_to_euler(q.x, q.y, q.z, q.w)
        self.get_logger().info(
            f'\n[{robot_ns}] 位置: x={p.x:.3f}, y={p.y:.3f}, z={p.z:.3f}\n'
            f'[{robot_ns}] 姿态: roll={math.degrees(roll):.2f}°, '
            f'pitch={math.degrees(pitch):.2f}°, '
            f'yaw={math.degrees(yaw):.2f}° ({yaw:.3f} rad)'
        )


def main(args=None):
    rclpy.init(args=args)
    node = MultiPoseMonitor()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
