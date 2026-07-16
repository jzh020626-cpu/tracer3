#!/usr/bin/env python3
import math
import os
import socket
import statistics
import time
from collections import defaultdict, deque

import rclpy
from rclpy.executors import ExternalShutdownException
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data
from rosidl_runtime_py.utilities import get_message


SOCKET_PATH = os.environ.get(
    'ICCP_ROS_MONITOR_SOCKET', '/run/iccp-ros-monitor/monitor.sock'
)
WINDOW_SECONDS = 10.0
ROBOT_NAMESPACE = os.environ.get('ICCP_ROBOT_NAMESPACE', 'tracer4').strip('/')
ROBOT_SUFFIX = ROBOT_NAMESPACE.removeprefix('tracer')
GLOBAL_TOPICS = {'/tf', '/tf_static'}


class TopicRateMonitor(Node):
    def __init__(self):
        super().__init__('iccp_topic_rate_monitor')
        self._monitored_subscriptions = {}
        self._samples = defaultdict(lambda: deque(maxlen=2000))
        self._errors = {}

    def _topic_types(self):
        return dict(self.get_topic_names_and_types())

    @staticmethod
    def _topic_allowed(topic):
        robot_prefix = f'/{ROBOT_NAMESPACE}/'
        slider_prefix = f'/huatai{ROBOT_SUFFIX}'
        object_prefix = f'/object_position{ROBOT_SUFFIX}'
        return (
            topic.startswith(robot_prefix)
            or topic.startswith(slider_prefix)
            or topic.startswith(object_prefix)
            or topic in GLOBAL_TOPICS
        )

    def ensure_subscription(self, topic):
        if topic in self._monitored_subscriptions:
            return True

        if not self._topic_allowed(topic):
            self._errors[topic] = (
                f'topic [{topic}] is outside the {ROBOT_NAMESPACE} monitor scope'
            )
            return False

        topic_types = self._topic_types().get(topic, [])
        if not topic_types:
            self._errors[topic] = f'topic [{topic}] does not appear to be published yet'
            return False

        try:
            message_type = get_message(topic_types[0])
        except (AttributeError, ModuleNotFoundError, ValueError) as exc:
            self._errors[topic] = f'cannot load type {topic_types[0]}: {exc}'
            return False

        def callback(message, monitored_topic=topic):
            self._samples[monitored_topic].append(
                (time.monotonic(), len(message))
            )

        self._monitored_subscriptions[topic] = self.create_subscription(
            message_type,
            topic,
            callback,
            qos_profile_sensor_data,
            raw=True,
        )
        self._errors.pop(topic, None)
        self.get_logger().info(f'monitoring topic rate: {topic} ({topic_types[0]})')
        return True

    def _active_samples(self, topic):
        now = time.monotonic()
        samples = self._samples[topic]
        while samples and now - samples[0][0] > WINDOW_SECONDS:
            samples.popleft()
        return now, list(samples)

    def report_rate(self, topic):
        if not self.ensure_subscription(topic):
            return f'WARNING: {self._errors[topic]}\n'

        _, samples = self._active_samples(topic)

        if len(samples) < 2:
            return f'WARNING: topic [{topic}] does not appear to be published yet\n'

        times = [sample[0] for sample in samples]
        intervals = [right - left for left, right in zip(times, times[1:])]
        intervals = [value for value in intervals if value > 0.0]
        if not intervals:
            return f'WARNING: no valid timing samples for topic [{topic}]\n'

        average_period = sum(intervals) / len(intervals)
        average_rate = 1.0 / average_period
        minimum = min(intervals)
        maximum = max(intervals)
        deviation = statistics.pstdev(intervals) if len(intervals) > 1 else 0.0
        if not math.isfinite(average_rate):
            return f'WARNING: no valid timing samples for topic [{topic}]\n'

        return (
            f'average rate: {average_rate:.3f}\n'
            f'\tmin: {minimum:.3f}s max: {maximum:.3f}s '
            f'std dev: {deviation:.5f}s window: {len(samples)}\n'
        )

    @staticmethod
    def _format_bytes(value, unit):
        if unit == 'B':
            return f'{value:.0f} B'
        if unit == 'KB':
            return f'{value / 1000:.2f} KB'
        return f'{value / 1000 / 1000:.2f} MB'

    def report_bandwidth(self, topic):
        prefix = f'Subscribed to [{topic}]\n'
        if not self.ensure_subscription(topic):
            return prefix + f'WARNING: {self._errors[topic]}\n'

        now, samples = self._active_samples(topic)
        if len(samples) < 2:
            return prefix + f'WARNING: topic [{topic}] does not appear to be published yet\n'

        elapsed = now - samples[0][0]
        if elapsed <= 0.0:
            return prefix + 'WARNING: no valid bandwidth samples\n'

        sizes = [sample[1] for sample in samples]
        total = sum(sizes)
        bytes_per_second = total / elapsed
        unit = (
            'B' if bytes_per_second < 1000
            else 'KB' if bytes_per_second < 1000000
            else 'MB'
        )
        bandwidth = self._format_bytes(bytes_per_second, unit) + '/s'
        mean = self._format_bytes(total / len(sizes), unit)
        minimum = self._format_bytes(min(sizes), unit)
        maximum = self._format_bytes(max(sizes), unit)
        return (
            prefix
            + f'{bandwidth} from {len(sizes)} messages\n'
            + f'\tMessage size mean: {mean} min: {minimum} max: {maximum}\n'
        )

    def report(self, topic, mode):
        if mode == 'bw':
            return self.report_bandwidth(topic)
        return self.report_rate(topic)


def serve():
    os.makedirs(os.path.dirname(SOCKET_PATH), mode=0o755, exist_ok=True)
    try:
        os.unlink(SOCKET_PATH)
    except FileNotFoundError:
        pass

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCKET_PATH)
    os.chmod(SOCKET_PATH, 0o660)
    server.listen(64)
    server.setblocking(False)

    rclpy.init()
    node = TopicRateMonitor()
    node.get_logger().info(f'ICCP ROS rate monitor listening on {SOCKET_PATH}')
    try:
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.05)
            while True:
                try:
                    connection, _ = server.accept()
                except BlockingIOError:
                    break

                with connection:
                    connection.settimeout(0.5)
                    try:
                        request = connection.recv(4096).decode('utf-8').strip()
                        if '\t' in request:
                            mode, topic = request.split('\t', 1)
                        else:
                            mode, topic = 'hz', request
                        if (
                            mode not in ('hz', 'bw')
                            or not topic
                            or '\x00' in topic
                            or len(topic) > 1024
                        ):
                            response = 'WARNING: invalid topic name\n'
                        else:
                            response = node.report(topic, mode)
                    except (OSError, UnicodeDecodeError) as exc:
                        response = f'WARNING: monitor request failed: {exc}\n'
                    try:
                        connection.sendall(response.encode('utf-8'))
                    except OSError:
                        pass
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()
        server.close()
        try:
            os.unlink(SOCKET_PATH)
        except FileNotFoundError:
            pass


if __name__ == '__main__':
    serve()
