#!/usr/bin/env bash
set -u

export ROS_DOMAIN_ID=36
export ROS_LOCALHOST_ONLY=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skki/.fastdds/fastdds_select5G.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
unset ROS_DISCOVERY_SERVER

source /opt/ros/humble/setup.bash
source /home/skki/5G_slam_edge/install/setup.bash

echo '=== system ==='
uname -m
date -Is
chronyc tracking 2>/dev/null | grep -E 'Reference ID|System time|Leap status' || true
df -h /
free -h

echo '=== devices ==='
ls -l /dev/imu /dev/force_sensor 2>/dev/null || true
ip -details link show can1 2>/dev/null | head -n 8 || true
ip -details link show can2 2>/dev/null | head -n 8 || true

echo '=== ROS endpoints ==='
for topic in odom IMU_data odometry/filtered scan cmd_vel; do
    echo "--- /tracer3/${topic}"
    ros2 topic info "/tracer3/${topic}" 2>/dev/null | head -n 4 || true
done

echo '=== rates ==='
timeout 6 ros2 topic hz /tracer3/IMU_data 2>/dev/null | tail -n 4 || true
timeout 6 ros2 topic hz /tracer3/odometry/filtered 2>/dev/null | tail -n 4 || true
timeout 8 ros2 topic hz /tracer3/scan 2>/dev/null | tail -n 4 || true

echo '=== TF ==='
timeout 6 ros2 run tf2_ros tf2_echo tracer3/odom tracer3/base_link 2>/dev/null | head -n 12 || true

echo '=== duplicate process check ==='
pgrep -af 'tracer_base|hipnuc_imu|ekf_node|listener_motor|rslidar_sdk_node|force_sensor_node' || true
