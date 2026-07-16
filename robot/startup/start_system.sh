#!/bin/bash

ROBOT_ID=3
ROBOT_NS="tracer3"
BASE_WS="/home/skki/5G_slam_edge"
SLIDER_WS="/home/skki/ros_huatai3_pos_spe"
FORCE_WS="/home/skki/ForceSensorC++"
LOG_ROOT="$HOME/start_system_logs_${ROBOT_NS}"
START_STAGGER_SEC=15
TIME_SERVER_IP="192.168.50.42"
TIME_SYNC_TIMEOUT_SEC=120
LOG_RETENTION_DAYS=14
DISK_WARN_PERCENT=80
START_FORCE_SENSOR="${START_FORCE_SENSOR:-0}"

stop_tracer_processes() {
    local signal="$1"

    pkill -"${signal}" -f "tracer_base" 2>/dev/null || true
    pkill -"${signal}" -f "listener_motor" 2>/dev/null || true
    pkill -"${signal}" -f "motor_controller_node${ROBOT_ID}" 2>/dev/null || true
    pkill -"${signal}" -f "force_sensor_node" 2>/dev/null || true
    pkill -"${signal}" -f "ekf_localization" 2>/dev/null || true
    pkill -"${signal}" -f "ekf_${ROBOT_NS}.launch.py" 2>/dev/null || true
    pkill -"${signal}" -f "ekf_node.*${ROBOT_NS}_ekf.yaml" 2>/dev/null || true
    pkill -"${signal}" -f "robot_localization.*/ekf_node.*${ROBOT_NS}_ekf.yaml" 2>/dev/null || true
    pkill -"${signal}" -x ekf_node 2>/dev/null || true
    pkill -"${signal}" -f "hipnuc_imu" 2>/dev/null || true
    pkill -"${signal}" -f "${ROBOT_NS}_IMU_publisher" 2>/dev/null || true
    pkill -"${signal}" -f "imu_topic:=/${ROBOT_NS}/IMU_data" 2>/dev/null || true
}

chrony_uses_expected_source() {
    chronyc -n sources 2>/dev/null | awk -v expected="$TIME_SERVER_IP" '
        $1 == "^*" && $2 == expected { found = 1 }
        END { exit(found ? 0 : 1) }
    '
}

wait_for_time_sync() {
    local waited=0
    while [ "$waited" -lt "$TIME_SYNC_TIMEOUT_SEC" ]; do
        if chrony_uses_expected_source && chronyc waitsync 1 0.05 >/dev/null 2>&1; then
            echo "[System Script] chrony synchronized with ${TIME_SERVER_IP}: $(date '+%F %T %Z')"
            return 0
        fi
        sleep 1
        waited=$((waited + 1))
    done
    echo "[System Script] ERROR: chrony did not synchronize with ${TIME_SERVER_IP} within ${TIME_SYNC_TIMEOUT_SEC}s." >&2
    return 1
}

cleanup_old_logs() {
    local disk_used

    mkdir -p "$LOG_ROOT"
    find "$LOG_ROOT" -mindepth 1 -maxdepth 1 -type d \
        -mtime +"$LOG_RETENTION_DAYS" -exec rm -rf -- {} +

    if [ -d "$HOME/.ros/log" ]; then
        find "$HOME/.ros/log" -mindepth 1 -maxdepth 1 -type d \
            -mtime +"$LOG_RETENTION_DAYS" -exec rm -rf -- {} +
        find "$HOME/.ros/log" -mindepth 1 -maxdepth 1 -type f \
            -mtime +"$LOG_RETENTION_DAYS" -delete
    fi

    disk_used=$(df -P / | awk 'NR == 2 { gsub("%", "", $5); print $5 }')
    if [ -n "$disk_used" ] && [ "$disk_used" -ge "$DISK_WARN_PERCENT" ]; then
        echo "[System Script] WARNING: root filesystem usage is ${disk_used}% (limit ${DISK_WARN_PERCENT}%)." >&2
    else
        echo "[System Script] root filesystem usage after log cleanup: ${disk_used}%."
    fi
}

init_can_interface() {
    local iface="$1"
    local flags

    if [ ! -d "/sys/class/net/${iface}" ]; then
        echo "[System Script] ERROR: ${iface} does not exist after loading gs_usb." >&2
        return 1
    fi

    sudo -n ip link set dev "$iface" down 2>/dev/null || true
    sudo -n ip link set dev "$iface" type can bitrate 500000 || return 1
    sudo -n ip link set dev "$iface" up || return 1

    flags=$(cat "/sys/class/net/${iface}/flags" 2>/dev/null) || return 1
    if (( (flags & 0x1) == 0 )); then
        echo "[System Script] ERROR: ${iface} did not enter UP state." >&2
        return 1
    fi

    echo "[System Script] ${iface} initialized at 500000 bit/s."
}

cleanup() {
    echo
    echo "[System Script] stop signal received, cleaning ${ROBOT_NS} nodes..."
    stop_tracer_processes SIGINT
    sleep 2
    stop_tracer_processes 9
    sudo ip link set can1 down 2>/dev/null || true
    sudo ip link set can2 down 2>/dev/null || true
    echo "[System Script] cleanup done."
    exit 0
}

trap cleanup SIGINT SIGTERM

export ROS_DOMAIN_ID=36
export ROS_LOCALHOST_ONLY=0
export RCUTILS_LOGGING_BUFFERED_STREAM=1

if [ -f /home/skki/.fastdds/fastdds_select5G.xml ]; then
    export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
    export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skki/.fastdds/fastdds_select5G.xml
    export RMW_FASTRTPS_USE_QOS_FROM_XML=1
fi

wait_for_time_sync || exit 1
cleanup_old_logs

echo "[System Script] requesting sudo authorization for foreground hardware setup..."
sudo -v || exit 1

echo "[System Script] pre-cleaning old ${ROBOT_NS} nodes..."
stop_tracer_processes SIGINT
sleep 2
stop_tracer_processes 9
sleep 1

if [ ! -e /dev/imu ]; then
    echo "[System Script] ERROR: /dev/imu is missing. Check the IMU USB connection and udev rules." >&2
    exit 1
fi
echo "[System Script] IMU device: /dev/imu -> $(readlink -f /dev/imu)"
if [ "$START_FORCE_SENSOR" = "1" ] && [ -e /dev/force_sensor ]; then
    echo "[System Script] force device: /dev/force_sensor -> $(readlink -f /dev/force_sensor)"
elif [ -e /dev/force_sensor ]; then
    echo "[System Script] force device detected; automatic start is disabled for navigation stability."
else
    echo "[System Script] force sensor is not connected; its node will remain disabled."
fi

MODULE_NAME="gs_usb"
MODULE_FILE="${BASE_WS}/autostart/gs_usb6.2.ko"

if ! lsmod | grep -q "^${MODULE_NAME}[[:space:]]"; then
    sudo insmod "$MODULE_FILE" || exit 1
    sleep 0.5
fi

init_can_interface can1 || exit 1
init_can_interface can2 || exit 1

source /opt/ros/humble/setup.bash
source "${BASE_WS}/install/setup.bash"
source "${SLIDER_WS}/install/setup.bash"
source "${FORCE_WS}/install/setup.bash"

LOG_DIR="${LOG_ROOT}/$(date +%F_%H%M%S)"
mkdir -p "$LOG_DIR"
echo "[System Script] log dir: $LOG_DIR"

nohup stdbuf -oL -eL ros2 launch tracer_base tracer_base.launch.py port_name:=can1 robot_id:=${ROBOT_ID} \
    >"$LOG_DIR/tracer_base.log" 2>&1 </dev/null &
echo "[System Script] waiting ${START_STAGGER_SEC}s before starting IMU..."
sleep "$START_STAGGER_SEC"

nohup stdbuf -oL -eL ros2 launch hipnuc_imu imu_spec_msg.launch.py \
    >"$LOG_DIR/hipnuc_imu.log" 2>&1 </dev/null &
echo "[System Script] waiting ${START_STAGGER_SEC}s before starting EKF..."
sleep "$START_STAGGER_SEC"

nohup stdbuf -oL -eL ros2 launch ekf_localization ekf_tracer3.launch.py \
    >"$LOG_DIR/ekf_localization.log" 2>&1 </dev/null &
echo "[System Script] waiting ${START_STAGGER_SEC}s before starting listener_motor..."
sleep "$START_STAGGER_SEC"

nohup stdbuf -oL -eL ros2 run motor_httx_pos_spe listener_motor --ros-args \
    -r __node:=motor_controller_node3 \
    -p tracer_id:=3 \
    -p can_interface:=can2 \
    -p command_topic:=/huatai3_pos_spe_pd \
    -p status_topic:=/huatai3_pos_spe_p \
    -p compensation_topic:=/huatai3_compensation_ref \
    -p status_period_ms:=50 \
    -p speed_request_period_ms:=50 \
    >"$LOG_DIR/listener_motor.log" 2>&1 </dev/null &
LISTENER_PID=$!

sleep 3
if ! kill -0 "$LISTENER_PID" 2>/dev/null; then
    echo "[System Script] ERROR: listener_motor exited during CAN initialization." >&2
    stop_tracer_processes SIGINT
    sleep 2
    stop_tracer_processes 9
    exit 1
fi

# Keep the normal navigation profile at seven DDS participants. The force node
# can still be enabled explicitly for force-only work when Nav2 is stopped.
if [ "$START_FORCE_SENSOR" = "1" ] && [ -e /dev/force_sensor ]; then
    echo "[System Script] waiting ${START_STAGGER_SEC}s before starting force sensor..."
    sleep "$START_STAGGER_SEC"
    nohup stdbuf -oL -eL ros2 run force_sensor_pkg force_sensor_node \
        >"$LOG_DIR/force_sensor_node.log" 2>&1 </dev/null &
elif [ "$START_FORCE_SENSOR" = "1" ]; then
    echo "[System Script] force_sensor_node disabled: /dev/force_sensor is missing." \
        >"$LOG_DIR/force_sensor_node.log"
else
    echo "[System Script] force_sensor_node disabled for navigation stability. Set START_FORCE_SENSOR=1 only when Nav2 is stopped." \
        >"$LOG_DIR/force_sensor_node.log"
fi

sleep 1
echo "[System Script] tracer_base PID:       $(pgrep -f 'tracer_base' | tr '\n' ' ')"
echo "[System Script] hipnuc_imu PID:       $(pgrep -f 'hipnuc_imu.*talker|tracer3_IMU_publisher' | tr '\n' ' ')"
echo "[System Script] ekf_localization PID: $(pgrep -f 'ekf_localization.*ekf_tracer3.launch.py|ekf_node.*tracer3_ekf.yaml' | tr '\n' ' ')"
echo "[System Script] listener_motor PID:   $(pgrep -f 'listener_motor|motor_controller_node3' | tr '\n' ' ')"
echo "[System Script] force_sensor PID:     $(pgrep -f 'force_sensor_node' | tr '\n' ' ')"
echo "[System Script] nodes started in background; script exits now."
echo "[System Script] logs: $LOG_DIR"
exit 0
