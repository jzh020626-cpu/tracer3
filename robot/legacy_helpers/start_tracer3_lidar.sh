#!/usr/bin/env bash
set -e

# ================= 用户配置 =================
export ROS_DOMAIN_ID=36
WS=~/rslidar_slam            # ⚠️ 工作空间根目录（包含 src/ install/ build/）
RELAY=~/ros_utils/tracer3_points_relay.py
# ===========================================

echo "[1/4] Source ROS2..."
source /opt/ros/$ROS_DISTRO/setup.bash

echo "[2/4] Check workspace build..."
cd "$WS"
if [ ! -f "$WS/install/setup.bash" ]; then
  echo "  install/ not found, running colcon build..."
  colcon build
else
  echo "  install/ exists, skip build."
fi

echo "[3/4] Source workspace overlay..."
source "$WS/install/setup.bash"

echo "[4/4] Start rslidar + relay..."
ros2 launch rslidar_sdk start.py &

# 等雷达节点真正起来
sleep 2

python3 "$RELAY"

