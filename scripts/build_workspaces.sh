#!/usr/bin/env bash
set -euo pipefail

source /opt/ros/humble/setup.bash

build_workspace() {
    local workspace="$1"
    echo "== Building ${workspace} =="
    cd "$workspace"
    rosdep install --from-paths src --ignore-src -r -y --rosdistro humble
    rm -rf build install log
    colcon build --symlink-install
}

build_workspace /home/skki/5G_slam_edge
build_workspace /home/skki/rslidar_slam
build_workspace /home/skki/ros_huatai3_pos_spe

echo "All tracer3 workspaces built for $(uname -m)."
