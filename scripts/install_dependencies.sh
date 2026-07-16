#!/usr/bin/env bash
set -euo pipefail

if [ "$(uname -m)" != "x86_64" ]; then
    echo "ERROR: this migration helper is for x86_64, got $(uname -m)" >&2
    exit 1
fi
if [ "${VERSION_ID:-}" = "" ]; then
    . /etc/os-release
fi
if [ "${VERSION_ID}" != "22.04" ]; then
    echo "ERROR: expected Ubuntu 22.04, got ${VERSION_ID}" >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    python3-pip python3-rosdep python3-colcon-common-extensions \
    can-utils chrony network-manager \
    libasio-dev libusb-1.0-0-dev libyaml-cpp-dev libpcap-dev

if [ ! -f /opt/ros/humble/setup.bash ]; then
    echo "ERROR: ROS 2 Humble is not installed. Install ros-humble-desktop first." >&2
    exit 1
fi

sudo rosdep init 2>/dev/null || true
rosdep update
echo "Base dependencies installed. Run scripts/build_workspaces.sh next."
