# tracer3 Orin 基线备份与 x86_64 迁移手册

本仓库是 **tracer3** 当前 NVIDIA Orin 运行环境的可迁移基线，采集时间为
2026-07-16。目标是将底盘、IMU、EKF、RoboSense 雷达、滑台控制和相关系统配置
迁移到 Ubuntu 22.04 x86_64 工控机。

> 这不是磁盘镜像，也不能把 Orin 的 `build/`、`install/` 或 ARM 二进制直接复制到
> x86。迁移必须在 x86_64 上重新安装依赖并执行 `colcon build`。

## 1. 固定基线

| 项目 | 当前值 |
|---|---|
| 机器人 | `tracer3` |
| 机器人编号 | `3` |
| 固定 IP | `192.168.50.53/24` |
| ROS 2 | Humble，Ubuntu 22.04 |
| 主 ROS Domain | `36`，四台车保持一致 |
| ICCP Domain | `82`，独立于主 ROS 图 |
| NTP 服务器 | `192.168.50.42` |
| 底盘 CAN | `can1`，500 kbit/s |
| 滑台 CAN | `can2`，500 kbit/s |
| 滑台状态/请求周期 | `50 ms` |
| 分阶段启动间隔 | `15 s` |
| 力传感器 | 默认不启动；只有 Nav2 停止且确认串口后才允许启用 |
| WiFi | 运行时关闭，数据链路仅走 5G/有线接口 |

必须保留以上 Domain 和机器人硬编码。不要为了“统一”而把 tracer3 的 namespace、
滑台话题、CAN 号或串口序列号替换为其他小车的值。

robot/legacy_helpers 中是历史雷达辅助脚本，不是默认入口。正式入口是 robot/startup/start_tracer3_lidar.sh。

## 2. 仓库内容

```text
robot/
  startup/              当前生效的整车与雷达启动脚本
  dds/                  Fast DDS 配置
  legacy_helpers/       历史辅助脚本，仅供排查
system/
  udev/                 IMU/力传感器稳定符号链接规则
  chrony/               对时配置
  systemd/              ICCP、监控和持久化日志配置
  iccp-monitor/         低频常驻 ROS 监控脚本
  iccp-agent.yaml.example  脱敏后的 ICCP 配置模板
workspaces/
  5G_slam_edge/src/     底盘、IMU、EKF 及相关消息/SDK 源码
  rslidar_slam/src/     RoboSense 雷达、点云转 LaserScan、建图源码
  ros_huatai3_pos_spe/src/  tracer3 滑台与力传感器源码
manifests/              软件、硬件、上游提交和文件校验清单
scripts/                x86 迁移辅助脚本
```

以下内容被有意排除：

- 所有 `build/`、`install/`、`log/` 和缓存目录；
- ROS bag、抓包、运行日志和回收站内容；
- 嵌套 `.git` 历史；上游 URL/commit 已写入 `manifests/source-upstreams.tsv`；
- `app-p9-*-linux-arm64`、`iccp-agent_*_arm64.deb` 和 `/opt/iccp-agent/agent`；
- SSH key、GitHub token、网络密码和 ICCP 凭据；
- 排障过程中生成的 `.bak`、`codex_backup` 临时副本。

## 3. x86 工控机要求

推荐保持与 Orin 相同的软件主版本：

- Ubuntu 22.04 LTS x86_64；
- ROS 2 Humble；
- 用户名和 home 路径使用 `skki`、`/home/skki`；
- 至少 4 核 CPU、8 GB RAM、100 GB SSD；
- 两路 SocketCAN 适配器、一个 CP210x IMU 串口和一个 FTDI 力传感器串口；
- 一张可配置为 `192.168.50.53/24` 的 5G/有线网卡。

使用同名用户是最稳妥的方式，因为启动脚本包含 `/home/skki/...` 绝对路径。如果必须使用
其他用户名，需要系统性修改启动脚本和 service 的路径，不能只改一处。

## 4. 安装 Ubuntu 与 ROS

1. 全新安装 Ubuntu 22.04 x86_64，RTC 使用 UTC。
2. 安装 ROS 2 Humble Desktop 和 rosdep。
3. 克隆本仓库到 `/home/skki/tracer3-backup`。
4. 执行依赖脚本：

```bash
cd /home/skki/tracer3-backup
bash scripts/install_dependencies.sh
```

5. 将源码复制为运行工作空间并构建：

```bash
mkdir -p /home/skki/5G_slam_edge /home/skki/rslidar_slam \
  /home/skki/ros_huatai3_pos_spe
cp -a workspaces/5G_slam_edge/src /home/skki/5G_slam_edge/
cp -a workspaces/rslidar_slam/src /home/skki/rslidar_slam/
cp -a workspaces/ros_huatai3_pos_spe/src \
  /home/skki/ros_huatai3_pos_spe/
bash scripts/build_workspaces.sh
```

不要复制仓库外的 Orin `install/`。若某个包意外链接到 `/usr/lib/aarch64-linux-gnu`，应删除
对应工作空间的 `build/ install/ log/` 后重新构建。

## 5. 串口与 udev

当前物理设备：

| 设备 | USB 芯片 | USB serial | 稳定路径 |
|---|---|---|---|
| IMU | CP210x `10c4:ea60` | `d47277e1c263ec11b2784debee680de7` | `/dev/imu` |
| 力传感器 | FTDI `0403:6001` | `A92X382Q` | `/dev/force_sensor` |

将传感器连接到 x86 后先核对 serial：

```bash
udevadm info --query=property --name=/dev/ttyUSB0 | grep -E 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL_SHORT'
udevadm info --query=property --name=/dev/ttyUSB1 | grep -E 'ID_VENDOR_ID|ID_MODEL_ID|ID_SERIAL_SHORT'
```

确认与表格一致后安装规则：

```bash
sudo install -m 0644 system/udev/99-tracer-serial.rules /etc/udev/rules.d/
sudo usermod -aG dialout skki
sudo udevadm control --reload-rules
sudo udevadm trigger
ls -l /dev/imu /dev/force_sensor
```

不要把 `/dev/ttyUSB0` 写回代码。USB 枚举顺序会在重启或换主板后变化。

## 6. CAN 适配器

当前约定严格如下：

- `can1`：Tracer 底盘；
- `can2`：tracer3 滑台；
- 两路均为 `500000` bit/s；
- 后台 `listener_motor` 不执行交互式 `sudo`，CAN 必须由前台启动阶段初始化；
- CAN 初始化失败必须退出，不能继续假设 `can_online=true`。

x86 上先识别两只适配器的稳定属性：

```bash
for dev in can0 can1 can2; do
  [ -e /sys/class/net/$dev ] || continue
  echo "== $dev =="
  udevadm info -q property -p /sys/class/net/$dev | grep -E 'ID_PATH|ID_SERIAL|ID_VENDOR|ID_MODEL'
done
```

根据适配器的 `ID_PATH` 或 serial 建立固定命名，使底盘永远为 `can1`、滑台永远为
`can2`。在物理对应关系未确认前禁止发送运动命令。

只读检查：

```bash
ip -details link show can1
ip -details link show can2
ip -statistics link show can1
ip -statistics link show can2
```

## 7. 网络与 Fast DDS

1. 给 5G/有线网卡配置 `192.168.50.53/24`。
2. 关闭 WiFi，避免 DDS 同时选择两张网卡。
3. 主 ROS 环境保持：

```bash
export ROS_DOMAIN_ID=36
export ROS_LOCALHOST_ONLY=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skki/.fastdds/fastdds_select5G.xml
export RMW_FASTRTPS_USE_QOS_FROM_XML=1
unset ROS_DISCOVERY_SERVER
```

4. 将 `robot/dds/fastdds_select5G.xml` 安装到 `/home/skki/.fastdds/`。
5. 配置中的 interface allowlist 当前绑定 `192.168.50.53` 和 `127.0.0.1`。如果工控机 IP
   变化，必须同步修改 XML；机器人编号和 ROS Domain 不变。

四台车都在 Domain 36。ICCP 继续使用 Domain 82，不要把 ICCP daemon 或监控进程改进
Domain 36。

## 8. 时间同步

安装并启用 chrony：

```bash
sudo install -m 0644 system/chrony/chrony.conf /etc/chrony/chrony.conf
sudo systemctl enable --now chrony
sudo systemctl restart chrony
date
timedatectl status
chronyc sources -v
chronyc tracking
```

验收要求：

- 当前源为 `192.168.50.42`；
- `Leap status: Normal`；
- 偏差稳定在 1 ms 内；
- `System clock synchronized: yes`；
- RTC 不再回到 1970 年。

`Ref time (UTC)` 是 chrony 最近一次从服务器采样的 UTC 时间，不是错误。

## 9. ICCP 迁移

仓库没有上传 ARM64 ICCP 可执行文件和凭据。迁移到 x86 前必须取得以下之一：

1. 官方 `amd64/x86_64` ICCP agent 安装包；
2. ICCP agent 源码并在 x86_64 上重新构建。

取得 x86 agent 后再参考 `system/systemd/` 安装服务。`iccp-agent.yaml.example` 已脱敏，
需要从安全渠道补回 endpoint 和认证信息。保持：

- ICCP ROS Domain 为 82；
- `ICCP_ROBOT_NAMESPACE=tracer3`；
- 常驻 `iccp_ros_monitor.py`，不要并发运行十几个 `ros2 topic hz`；
- CPU、内存、任务数限制沿用 systemd drop-in。

## 10. 启动顺序

安装运行文件：

```bash
install -m 0755 robot/startup/start_system.sh /home/skki/start_system.sh
mkdir -p /home/skki/bin /home/skki/.fastdds
install -m 0755 robot/startup/start_tracer3_lidar.sh /home/skki/bin/
install -m 0644 robot/dds/fastdds_select5G.xml /home/skki/.fastdds/
```

启动前确认车轮悬空或急停可用，然后执行：

```bash
bash /home/skki/start_system.sh
bash /home/skki/bin/start_tracer3_lidar.sh
```

`start_system.sh` 会依次启动底盘、IMU、EKF 和滑台，每阶段间隔 15 秒。它会清理旧进程、
初始化 can1/can2，并在 CAN 初始化失败时退出。默认不会启动力传感器。

只有在 Nav2 完全停止、`/dev/force_sensor` 已确认且串口未被 IMU 使用时，才允许：

```bash
START_FORCE_SENSOR=1 bash /home/skki/start_system.sh
```

## 11. 分层验收

### 11.1 系统层

```bash
uname -m                         # 必须是 x86_64
df -h /
free -h
chronyc tracking
ls -l /dev/imu /dev/force_sensor
ip -br link show can1
ip -br link show can2
```

### 11.2 ROS 图

```bash
export ROS_DOMAIN_ID=36
source /opt/ros/humble/setup.bash
source /home/skki/5G_slam_edge/install/setup.bash
ros2 topic info /tracer3/odom
ros2 topic info /tracer3/IMU_data
ros2 topic info /tracer3/odometry/filtered
ros2 topic info /tracer3/scan
ros2 topic info /tracer3/cmd_vel
```

要求：

- IMU 只有一个发布者；
- EKF 只有一套，`/tracer3/odometry/filtered` 连续发布；
- `tracer3/odom -> tracer3/base_link` 只由 EKF 发布；
- 雷达约 10 Hz，EKF 通常约 50 Hz；
- 静止时 odom yaw 与 IMU yaw 差值稳定，重新启用 orientation 融合前应小于 3 度；
- `/tracer3/cmd_vel` 的底盘订阅存在，但验证阶段不发送非零速度。

可以运行仓库中的只读检查：

```bash
bash scripts/verify_runtime.sh
```

### 11.3 低速运动

在系统层和 ROS 层全部通过后，先抬轮或置于空旷区域，以极低速度做单次短测试。检查：

- can1/can2 无 error-passive、bus-off；
- 底盘方向、里程计方向和 IMU yaw 方向一致；
- 滑台编号和话题均为 3，不能控制到其他滑台；
- 停止命令后速度立即归零。

### 11.4 Nav2

Nav2 和 RViz 位于控制电脑的 `navigation_ws`，不属于本仓库。机器人端通过 Domain 36 提供
`scan`、`odometry/filtered`、TF 和 `cmd_vel`。先确认这些接口稳定，再启动控制电脑上的：

```bash
ros2 launch tracer_navigation2 navigation2.launch.py namespace:=tracer3
```

AMCL 启动后需要在 RViz 使用 **2D Pose Estimate** 设置实际位置和朝向。

## 12. 回滚与故障定位

- 原 Orin 不要删除，直到 x86 连续完成至少 30 分钟底层运行和多次导航验收；
- 每次只替换一个层级：系统配置、底盘/IMU/EKF、雷达、滑台、ICCP、Nav2；
- 出现 SSH 卡顿时先停止 Nav2 和雷达高带宽链路，检查 CPU、内存、磁盘、NIC 丢包和 DDS
  参与者数量；
- 出现 1970 时间戳时先修 chrony/RTC，再启动 ROS；
- 出现多个 IMU/EKF 发布者时执行启动脚本的清理流程，不要直接叠加启动；
- 出现 CAN down 或 D 状态时停止整套 ROS，检查 USB CAN、内核日志和文件系统。

## 13. 安全与完整性

提交前已执行凭据扫描；上游 remote 中内嵌的 token 已从备份清单移除。若原 Orin 的 Git
remote 曾包含 PAT，应在 GitHub 撤销该 PAT。

迁移后可重新生成文件校验：

```bash
find . -type f -not -path './.git/*' -print0 | sort -z | xargs -0 sha256sum > manifests/SHA256SUMS
```

本仓库只记录 2026-07-16 的基线。后续修改硬件编号、EKF 参数、串口或滑台代码时，应同时
更新 README 和 manifests，并为每台车单独提交。
