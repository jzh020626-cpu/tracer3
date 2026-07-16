#!/usr/bin/env bash
set -e

export ROS_DOMAIN_ID=36
source /opt/ros/humble/setup.bash
source ~/rslidar_slam/install/setup.bash

# 1) 启动雷达（如果你已经用别的方法开着，可注释掉这一行）
ros2 launch rslidar_sdk start.py &

sleep 2

# 2) 点云 -> 2D scan
ros2 run pointcloud_to_laserscan pointcloud_to_laserscan_node --ros-args \
  -r cloud_in:=/tracer3/rslidar_points \
  -r scan:=/tracer3/scan \
  -p target_frame:=tracer3/rslidar \
  -p transform_tolerance:=0.2 \
  -p min_height:=-0.10 \
  -p max_height:=0.10 \
  -p angle_min:=-3.14159 \
  -p angle_max:=3.14159 \
  -p angle_increment:=0.0174533 \
  -p scan_time:=0.1 \
  -p range_min:=0.20 \
  -p range_max:=20.0 \
  -p use_inf:=true \
  -p inf_epsilon:=1.0
