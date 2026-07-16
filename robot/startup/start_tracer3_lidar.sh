#!/bin/bash
set -e

source /opt/ros/humble/setup.bash
source /home/skki/rslidar_slam/install/setup.bash

export ROS_DOMAIN_ID=36
export ROS_LOCALHOST_ONLY=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skki/.fastdds/fastdds_select5G.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
unset ROS_DISCOVERY_SERVER

pkill -INT -f 'rslidar_2dscan.launch.py.*robot_id:=tracer3' 2>/dev/null || true
pkill -INT -f 'rslidar_sdk_node.*__ns:=/tracer3' 2>/dev/null || true
pkill -INT -f '__node:=tracer3_base_to_rslidar.*__ns:=/tracer3' 2>/dev/null || true
pkill -INT -f '__node:=pointcloud_to_laserscan.*__ns:=/tracer3' 2>/dev/null || true
sleep 2
pkill -9 -f 'rslidar_2dscan.launch.py.*robot_id:=tracer3' 2>/dev/null || true
pkill -9 -f 'rslidar_sdk_node.*__ns:=/tracer3' 2>/dev/null || true
pkill -9 -f '__node:=tracer3_base_to_rslidar.*__ns:=/tracer3' 2>/dev/null || true
pkill -9 -f '__node:=pointcloud_to_laserscan.*__ns:=/tracer3' 2>/dev/null || true

log_dir=/home/skki/start_system_logs_tracer3/lidar_$(date +%F_%H%M%S)
mkdir -p "$log_dir"
nohup setsid stdbuf -oL -eL ros2 launch rslidar_sdk rslidar_2dscan.launch.py robot_id:=tracer3 \
    >"$log_dir/rslidar_2dscan.log" 2>&1 </dev/null &
launcher_pid=$!
sleep 5

if ! kill -0 "$launcher_pid" 2>/dev/null; then
    echo "lidar_start_failed log=$log_dir/rslidar_2dscan.log"
    tail -n 80 "$log_dir/rslidar_2dscan.log"
    exit 1
fi

echo "lidar_launch_pid=$launcher_pid"
echo "lidar_log=$log_dir/rslidar_2dscan.log"
