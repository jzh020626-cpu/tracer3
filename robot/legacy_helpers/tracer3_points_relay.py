#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from sensor_msgs.msg import PointCloud2

class Relay(Node):
    def __init__(self):
        super().__init__('tracer3_points_relay')
        self.sub = self.create_subscription(
            PointCloud2,
            '/rslidar_points',
            self.cb,
            qos_profile_sensor_data
        )
        self.pub = self.create_publisher(
            PointCloud2,
            '/tracer3/rslidar_points',
            qos_profile_sensor_data
        )
        self.get_logger().info('Relay: /rslidar_points  ->  /tracer3/rslidar_points')

    def cb(self, msg: PointCloud2):
        self.pub.publish(msg)

def main():
    rclpy.init()
    node = Relay()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
