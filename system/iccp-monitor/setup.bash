#!/bin/bash

# Keep this static: the agent starts many short probes and sourcing the full ROS
# setup for every probe creates expensive Python and argcomplete subprocesses.
export AMENT_PREFIX_PATH=/opt/ros/humble
export LD_LIBRARY_PATH=/opt/ros/humble/opt/rviz_ogre_vendor/lib:/opt/ros/humble/lib/aarch64-linux-gnu:/opt/ros/humble/lib
export PYTHONPATH=/opt/ros/humble/lib/python3.10/site-packages:/opt/ros/humble/local/lib/python3.10/dist-packages
export PATH="/usr/local/lib/iccp-agent/bin:/opt/ros/humble/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin"
export ROS_DISTRO=humble
export ROS_PYTHON_VERSION=3
export ROS_VERSION=2
export ROS_DOMAIN_ID=36
export ROS_LOCALHOST_ONLY=0

if [ -f /home/skki/.fastdds/fastdds_select5G.xml ]; then
    export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
    export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skki/.fastdds/fastdds_select5G.xml
    export RMW_FASTRTPS_USE_QOS_FROM_XML=1
fi
