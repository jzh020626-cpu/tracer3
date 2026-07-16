#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>
#include <string>
#include <cstdint>
#include <algorithm>
#include <stdexcept>

#include "rclcpp/rclcpp.hpp"
#include "base_interfaces_demo/msg/motor_command.hpp"
#include "base_interfaces_demo/msg/motor_status.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "motor_httx_pos_spe/MotorController.h"

using namespace std::chrono_literals;

// 默认电机参数
static const MotorInfo X_MOTOR = {17835, 9109020, 2.0, 181.05228};
static const MotorInfo Y_MOTOR = {23183, 9095830, 2.0, 181.05228};
static const MotorInfo Z_MOTOR = {54635, 2677570, 5.0, 181.05228};

enum class ControlMode
{
    IDLE = 0,
    POSITION = 1,
    SPEED = 2,
    COMPENSATION = 3
};

enum class DriveModeState
{
    UNKNOWN = 0,
    POSITION = 1,
    SPEED = 2
};

struct TargetPose
{
    double x{0.0};
    double y{0.0};
    double z{0.0};
    bool valid{false};
};

class MotorControllerNode : public rclcpp::Node
{
public:
    MotorControllerNode()
    : Node("motor_controller_node")
    {
        // ===== 基础参数 =====
        this->declare_parameter<int>("tracer_id", 1);
        this->declare_parameter<std::string>("can_interface", "can2");

        // 允许外部显式传入；若为空，则根据 tracer_id 自动生成
        this->declare_parameter<std::string>("command_topic", "");
        this->declare_parameter<std::string>("status_topic", "");
        this->declare_parameter<std::string>("compensation_topic", "");

        // ===== 行程限制 =====
        this->declare_parameter<double>("x_min", 1e-6);
        this->declare_parameter<double>("x_max", 278.0);
        this->declare_parameter<double>("y_min", 1e-6);
        this->declare_parameter<double>("y_max", 277.0);
        this->declare_parameter<double>("z_min", 1e-6);
        this->declare_parameter<double>("z_max", 200.0);

        // ===== 判稳与状态发布 =====
        this->declare_parameter<double>("position_tolerance_mm", 0.5);
        this->declare_parameter<double>("velocity_tolerance_mmps", 1.0);
        this->declare_parameter<int>("settle_cycles_required", 5);

        this->declare_parameter<int>("status_period_ms", 50);
        this->declare_parameter<int>("speed_request_period_ms", 50);

        // ===== 补偿控制参数 =====
        this->declare_parameter<double>("compensation_vx_limit_mmps", 30.0);
        this->declare_parameter<double>("compensation_vy_limit_mmps", 30.0);
        this->declare_parameter<double>("compensation_vz_limit_mmps", 15.0);

        this->declare_parameter<double>("compensation_vx_deadband_mmps", 0.1);
        this->declare_parameter<double>("compensation_vy_deadband_mmps", 0.1);
        this->declare_parameter<double>("compensation_vz_deadband_mmps", 0.1);

        // 调试阶段适度放宽 watchdog
        this->declare_parameter<int>("compensation_watchdog_ms", 500);
        this->declare_parameter<bool>("compensation_allow_position", true);
        this->declare_parameter<bool>("compensation_position_is_relative_default", true);

        // ===== 读取参数 =====
        tracer_id_ = static_cast<int>(this->get_parameter("tracer_id").as_int());
        if (tracer_id_ <= 0) {
            RCLCPP_WARN(this->get_logger(),
                        "非法 tracer_id=%d，已回退到 1", tracer_id_);
            tracer_id_ = 1;
        }

        can_interface_ = this->get_parameter("can_interface").as_string();
        command_topic_ = this->get_parameter("command_topic").as_string();
        status_topic_ = this->get_parameter("status_topic").as_string();
        comp_topic_ = this->get_parameter("compensation_topic").as_string();

        if (command_topic_.empty()) {
            command_topic_ = "/huatai" + std::to_string(tracer_id_) + "_pos_spe_pd";
        }
        if (status_topic_.empty()) {
            status_topic_ = "/huatai" + std::to_string(tracer_id_) + "_pos_spe_p";
        }
        if (comp_topic_.empty()) {
            comp_topic_ = "/huatai" + std::to_string(tracer_id_) + "_compensation_ref";
        }

        x_min_ = this->get_parameter("x_min").as_double();
        x_max_ = this->get_parameter("x_max").as_double();
        y_min_ = this->get_parameter("y_min").as_double();
        y_max_ = this->get_parameter("y_max").as_double();
        z_min_ = this->get_parameter("z_min").as_double();
        z_max_ = this->get_parameter("z_max").as_double();

        position_tolerance_mm_ = this->get_parameter("position_tolerance_mm").as_double();
        velocity_tolerance_mmps_ = this->get_parameter("velocity_tolerance_mmps").as_double();
        settle_cycles_required_ = static_cast<int>(this->get_parameter("settle_cycles_required").as_int());
        status_period_ms_ = static_cast<int>(this->get_parameter("status_period_ms").as_int());
        speed_request_period_ms_ = static_cast<int>(this->get_parameter("speed_request_period_ms").as_int());

        compensation_vx_limit_mmps_ = this->get_parameter("compensation_vx_limit_mmps").as_double();
        compensation_vy_limit_mmps_ = this->get_parameter("compensation_vy_limit_mmps").as_double();
        compensation_vz_limit_mmps_ = this->get_parameter("compensation_vz_limit_mmps").as_double();

        compensation_vx_deadband_mmps_ = this->get_parameter("compensation_vx_deadband_mmps").as_double();
        compensation_vy_deadband_mmps_ = this->get_parameter("compensation_vy_deadband_mmps").as_double();
        compensation_vz_deadband_mmps_ = this->get_parameter("compensation_vz_deadband_mmps").as_double();

        compensation_watchdog_ms_ = static_cast<int>(this->get_parameter("compensation_watchdog_ms").as_int());
        compensation_allow_position_ = this->get_parameter("compensation_allow_position").as_bool();
        compensation_position_is_relative_default_ =
            this->get_parameter("compensation_position_is_relative_default").as_bool();

        // ===== 初始化底层控制器 =====
        controller_ = std::make_shared<MotorController>(
            can_interface_, X_MOTOR, Y_MOTOR, Z_MOTOR);

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            if (!controller_->setupCanInterface(can_interface_)) {
                RCLCPP_FATAL(this->get_logger(),
                             "CAN 初始化失败，listener_motor 退出且不会发布在线状态");
                throw std::runtime_error("CAN initialization failed");
            }
            controller_->enableMotors();
            controller_->startReadingPosition();
            controller_->requestMotorSpeed();

            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
        }

        last_speed_request_tp_ = std::chrono::steady_clock::now();
        can_online_.store(true);

        // ===== ROS pub/sub =====
        status_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorStatus>(
            status_topic_, 10);

        status_publisher_std_ = this->create_publisher<std_msgs::msg::Float64MultiArray>(
            status_topic_ + "_std", 10);

        command_subscriber_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
            command_topic_, 10,
            std::bind(&MotorControllerNode::command_callback, this, std::placeholders::_1));

        compensation_subscriber_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
            comp_topic_, 10,
            std::bind(&MotorControllerNode::compensation_callback, this, std::placeholders::_1));

        status_timer_ = this->create_wall_timer(
            std::chrono::milliseconds(status_period_ms_),
            std::bind(&MotorControllerNode::publish_status, this));

        // ===== 初始位置 =====
        Position init_pos;
        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            init_pos = controller_->getCurrentPosition();
        }

        last_valid_pos_ = init_pos;

        RCLCPP_INFO(this->get_logger(), "车载滑台底座已启动");
        RCLCPP_INFO(this->get_logger(), "tracer_id: %d", tracer_id_);
        RCLCPP_INFO(this->get_logger(), "节点名: %s", this->get_name());
        RCLCPP_INFO(this->get_logger(), "CAN接口: %s", can_interface_.c_str());
        RCLCPP_INFO(this->get_logger(), "命令话题: %s", command_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "状态话题: %s", status_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "标准数组话题: %s_std", status_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "补偿参考话题: %s", comp_topic_.c_str());
        RCLCPP_INFO(this->get_logger(), "初始位置: X=%.2f, Y=%.2f, Z=%.2f",
                    init_pos.x, init_pos.y, init_pos.z);

        RCLCPP_INFO(this->get_logger(),
                    "状态发布周期=%d ms, 速度请求周期=%d ms",
                    status_period_ms_, speed_request_period_ms_);

        RCLCPP_INFO(this->get_logger(),
                    "补偿参数: vmax=(%.1f, %.1f, %.1f) mm/s, deadband=(%.2f, %.2f, %.2f) mm/s, watchdog=%d ms",
                    compensation_vx_limit_mmps_, compensation_vy_limit_mmps_, compensation_vz_limit_mmps_,
                    compensation_vx_deadband_mmps_, compensation_vy_deadband_mmps_, compensation_vz_deadband_mmps_,
                    compensation_watchdog_ms_);
    }

    ~MotorControllerNode() override
    {
        safeShutdown();
    }

private:
    void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!msg) return;

        RCLCPP_INFO(this->get_logger(), "[Command] received type=%s", msg->command_type.c_str());

        if (msg->command_type == "position") {
            handlePositionCommand(msg);
        } else if (msg->command_type == "speed") {
            handleSpeedCommand(msg);
        } else if (msg->command_type == "stop") {
            handleStopCommand();
        } else if (msg->command_type == "exit") {
            handleExitCommand();
        } else if (msg->command_type == "powerDown") {
            handlePowerDownCommand();
        } else if (msg->command_type == "powerUp") {
            handlePowerUpCommand();
        } else if (msg->command_type == "canDown") {
            handleCanDownCommand();
        } else if (msg->command_type == "canUp") {
            handleCanUpCommand();
        } else {
            RCLCPP_WARN(this->get_logger(), "[Command] 未知命令: %s", msg->command_type.c_str());
        }
    }

    void compensation_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!msg) return;

        if (msg->command_type == "speed") {
            handleCompensationSpeedCommand(msg);
        } else if (msg->command_type == "position") {
            if (!compensation_allow_position_) {
                RCLCPP_WARN(this->get_logger(),
                            "[Compensation] 当前配置禁止 position 型补偿命令，已忽略");
                return;
            }
            handleCompensationPositionCommand(msg);
        } else if (msg->command_type == "stop") {
            handleCompensationStopCommand();
        } else {
            RCLCPP_WARN(this->get_logger(),
                        "[Compensation] 未知补偿命令: %s", msg->command_type.c_str());
        }
    }

    void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!can_online_.load()) {
            RCLCPP_WARN(this->get_logger(), "[Position] CAN总线离线，拒绝执行位置指令");
            return;
        }

        if (!std::isfinite(msg->x) || !std::isfinite(msg->y) ||
            !std::isfinite(msg->z) || !std::isfinite(msg->time)) {
            RCLCPP_ERROR(this->get_logger(), "[Position] 拒绝执行：输入包含 NaN 或 Inf");
            return;
        }

        if (msg->time <= 0.0) {
            RCLCPP_ERROR(this->get_logger(), "[Position] 拒绝执行：time 必须大于 0");
            return;
        }

        double x = msg->x;
        double y = msg->y;
        double z = msg->z;

        if (msg->is_relative) {
            Position current;
            {
                std::lock_guard<std::mutex> lock(controller_mutex_);
                current = controller_->getCurrentPosition();
            }
            x += current.x;
            y += current.y;
            z += current.z;
        }

        if (!checkPositionInRange(x, y, z)) {
            RCLCPP_ERROR(this->get_logger(), "[Position] 目标超出行程限制: x=%.2f y=%.2f z=%.2f",
                         x, y, z);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            ensurePositionModeLocked("normal position");
            controller_->runMotors(x, y, z, msg->time);
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_pose_.x = x;
            target_pose_.y = y;
            target_pose_.z = z;
            target_pose_.valid = true;
            target_reached_ = false;
            current_mode_ = ControlMode::POSITION;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }
    }

    void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!can_online_.load()) {
            RCLCPP_WARN(this->get_logger(), "[Speed] CAN总线离线，拒绝执行速度指令");
            return;
        }

        if (!std::isfinite(msg->vx) || !std::isfinite(msg->vy) || !std::isfinite(msg->vz)) {
            RCLCPP_ERROR(this->get_logger(), "[Speed] 拒绝执行：速度输入包含 NaN 或 Inf");
            return;
        }

        const int32_t vx_cmd = static_cast<int32_t>(std::lround(msg->vx));
        const int32_t vy_cmd = static_cast<int32_t>(std::lround(msg->vy));
        const int32_t vz_cmd = static_cast<int32_t>(std::lround(msg->vz));

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            ensureSpeedModeLocked("normal speed");
            controller_->runMotorsSpeed(vx_cmd, vy_cmd, vz_cmd);
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_pose_.valid = false;
            target_reached_ = (vx_cmd == 0 && vy_cmd == 0 && vz_cmd == 0);
            current_mode_ = (vx_cmd == 0 && vy_cmd == 0 && vz_cmd == 0)
                                ? ControlMode::IDLE
                                : ControlMode::SPEED;
            last_speed_cmd_x_ = vx_cmd;
            last_speed_cmd_y_ = vy_cmd;
            last_speed_cmd_z_ = vz_cmd;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }
    }

    void handleCompensationSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!can_online_.load()) {
            RCLCPP_WARN(this->get_logger(), "[Compensation-Speed] CAN总线离线，拒绝执行补偿指令");
            return;
        }

        if (!std::isfinite(msg->vx) || !std::isfinite(msg->vy) || !std::isfinite(msg->vz)) {
            RCLCPP_ERROR(this->get_logger(), "[Compensation-Speed] 拒绝执行：速度输入包含 NaN 或 Inf");
            return;
        }

        const int32_t vx_cmd = sanitizeCompSpeed(msg->vx, compensation_vx_deadband_mmps_, compensation_vx_limit_mmps_);
        const int32_t vy_cmd = sanitizeCompSpeed(msg->vy, compensation_vy_deadband_mmps_, compensation_vy_limit_mmps_);
        const int32_t vz_cmd = sanitizeCompSpeed(msg->vz, compensation_vz_deadband_mmps_, compensation_vz_limit_mmps_);

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            ensureSpeedModeLocked("compensation speed");
            controller_->runMotorsSpeed(vx_cmd, vy_cmd, vz_cmd);
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_pose_.valid = false;
            target_reached_ = (vx_cmd == 0 && vy_cmd == 0 && vz_cmd == 0);
            current_mode_ = (vx_cmd == 0 && vy_cmd == 0 && vz_cmd == 0)
                                ? ControlMode::IDLE
                                : ControlMode::COMPENSATION;
            last_speed_cmd_x_ = vx_cmd;
            last_speed_cmd_y_ = vy_cmd;
            last_speed_cmd_z_ = vz_cmd;
            compensation_velocity_active_ = !(vx_cmd == 0 && vy_cmd == 0 && vz_cmd == 0);
            last_compensation_cmd_tp_ = std::chrono::steady_clock::now();
            compensation_watchdog_latched_ = false;
            position_settle_count_ = 0;
        }
    }

    void handleCompensationPositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        if (!can_online_.load()) {
            RCLCPP_WARN(this->get_logger(), "[Compensation-Position] CAN总线离线，拒绝执行补偿位置指令");
            return;
        }

        if (!std::isfinite(msg->x) || !std::isfinite(msg->y) ||
            !std::isfinite(msg->z) || !std::isfinite(msg->time)) {
            RCLCPP_ERROR(this->get_logger(), "[Compensation-Position] 拒绝执行：输入包含 NaN 或 Inf");
            return;
        }

        if (msg->time <= 0.0) {
            RCLCPP_ERROR(this->get_logger(), "[Compensation-Position] 拒绝执行：time 必须大于 0");
            return;
        }

        double x = msg->x;
        double y = msg->y;
        double z = msg->z;

        // 严格尊重上游 is_relative
        const bool is_relative = msg->is_relative;

        if (is_relative) {
            Position current;
            {
                std::lock_guard<std::mutex> lock(controller_mutex_);
                current = controller_->getCurrentPosition();
            }
            x += current.x;
            y += current.y;
            z += current.z;
        }

        if (!checkPositionInRange(x, y, z)) {
            RCLCPP_ERROR(this->get_logger(),
                         "[Compensation-Position] 目标超出行程限制: x=%.2f y=%.2f z=%.2f",
                         x, y, z);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            ensurePositionModeLocked("compensation position");
            controller_->runMotors(x, y, z, msg->time);
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_pose_.x = x;
            target_pose_.y = y;
            target_pose_.z = z;
            target_pose_.valid = true;
            target_reached_ = false;
            current_mode_ = ControlMode::COMPENSATION;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }
    }

    void handleCompensationStopCommand()
    {
        if (can_online_.load()) {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_reached_ = true;
            target_pose_.valid = false;
            current_mode_ = ControlMode::IDLE;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }

        publish_status();
    }

    void handleStopCommand()
    {
        if (can_online_.load()) {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_reached_ = true;
            target_pose_.valid = false;
            current_mode_ = ControlMode::IDLE;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }

        publish_status();
    }

    void handleExitCommand()
    {
        rclcpp::shutdown();
    }

    void handlePowerDownCommand()
    {
        RCLCPP_INFO(this->get_logger(), "[Command] explicit powerDown");
        if (can_online_.load()) {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
            controller_->powerOffMotors();
            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_reached_ = true;
            target_pose_.valid = false;
            current_mode_ = ControlMode::IDLE;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }
    }

    void handlePowerUpCommand()
    {
        RCLCPP_INFO(this->get_logger(), "[Command] explicit powerUp");
        if (can_online_.load()) {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            controller_->recoverAllMotors();
            controller_->enableMotors();
            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
        }
    }

    void handleCanDownCommand()
    {
        RCLCPP_INFO(this->get_logger(), "[Command] explicit canDown");
        if (!can_online_.load()) return;

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            controller_->stopReadingPosition();
            controller_->shutdownCanInterface(can_interface_);
            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
        }

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            target_reached_ = false;
            target_pose_.valid = false;
            current_mode_ = ControlMode::IDLE;
            position_settle_count_ = 0;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
        }

        can_online_.store(false);
        RCLCPP_INFO(this->get_logger(), "[CanDown] CAN 通信已安全关闭");
    }

    void handleCanUpCommand()
    {
        RCLCPP_INFO(this->get_logger(), "[Command] explicit canUp");
        if (can_online_.load()) return;

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            if (!controller_->setupCanInterface(can_interface_)) {
                can_online_.store(false);
                RCLCPP_FATAL(this->get_logger(),
                             "CanUp 失败，listener_motor 退出以避免伪在线状态");
                throw std::runtime_error("CAN reinitialization failed");
            }
            controller_->enableMotors();
            controller_->startReadingPosition();
            controller_->requestMotorSpeed();
            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
        }

        last_speed_request_tp_ = std::chrono::steady_clock::now();
        can_online_.store(true);
        RCLCPP_INFO(this->get_logger(), "[CanUp] CAN 通信已恢复");
    }

    void update_position_reach_state(double x, double y, double z, double vx, double vy, double vz)
    {
        std::lock_guard<std::mutex> state_lock(state_mutex_);

        if ((current_mode_ != ControlMode::POSITION && current_mode_ != ControlMode::COMPENSATION) ||
            !target_pose_.valid) {
            return;
        }

        const double ex = std::abs(x - target_pose_.x);
        const double ey = std::abs(y - target_pose_.y);
        const double ez = std::abs(z - target_pose_.z);

        const bool pos_ok = (ex < position_tolerance_mm_) &&
                            (ey < position_tolerance_mm_) &&
                            (ez < position_tolerance_mm_);

        const bool vel_ok = (std::abs(vx) < velocity_tolerance_mmps_) &&
                            (std::abs(vy) < velocity_tolerance_mmps_) &&
                            (std::abs(vz) < velocity_tolerance_mmps_);

        if (pos_ok && vel_ok) {
            ++position_settle_count_;
        } else {
            position_settle_count_ = 0;
        }

        if (position_settle_count_ >= settle_cycles_required_) {
            target_reached_ = true;
            target_pose_.valid = false;
            current_mode_ = ControlMode::IDLE;
            position_settle_count_ = 0;
            compensation_velocity_active_ = false;
            compensation_watchdog_latched_ = false;
            last_speed_cmd_x_ = 0;
            last_speed_cmd_y_ = 0;
            last_speed_cmd_z_ = 0;
        }
    }

    void update_speed_safety_state(const Position& pos)
    {
        int32_t vx_cmd = 0, vy_cmd = 0, vz_cmd = 0;
        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            vx_cmd = last_speed_cmd_x_;
            vy_cmd = last_speed_cmd_y_;
            vz_cmd = last_speed_cmd_z_;
        }

        const bool hit_limit =
            ((vx_cmd > 0 && pos.x >= x_max_) || (vx_cmd < 0 && pos.x <= x_min_) ||
             (vy_cmd > 0 && pos.y >= y_max_) || (vy_cmd < 0 && pos.y <= y_min_) ||
             (vz_cmd > 0 && pos.z >= z_max_) || (vz_cmd < 0 && pos.z <= z_min_));

        if (hit_limit) {
            {
                std::lock_guard<std::mutex> lock(controller_mutex_);
                stopMotionLocked();
            }

            {
                std::lock_guard<std::mutex> state_lock(state_mutex_);
                last_speed_cmd_x_ = 0;
                last_speed_cmd_y_ = 0;
                last_speed_cmd_z_ = 0;
                target_reached_ = true;
                target_pose_.valid = false;
                current_mode_ = ControlMode::IDLE;
                compensation_velocity_active_ = false;
                compensation_watchdog_latched_ = false;
            }

            RCLCPP_WARN(this->get_logger(), "[Speed/Compensation] 触发行程软限位，已自动停下");
        }
    }

    void check_compensation_watchdog()
    {
        if (!can_online_.load()) return;
        if (compensation_watchdog_ms_ <= 0) return;

        bool need_stop = false;
        bool need_warn = false;

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            if (!compensation_velocity_active_) {
                return;
            }

            if (last_compensation_cmd_tp_.time_since_epoch().count() == 0) {
                return;
            }

            const auto now_tp = std::chrono::steady_clock::now();
            const auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_compensation_cmd_tp_).count();

            if (dt >= compensation_watchdog_ms_) {
                compensation_velocity_active_ = false;
                target_reached_ = true;
                target_pose_.valid = false;
                current_mode_ = ControlMode::IDLE;
                last_speed_cmd_x_ = 0;
                last_speed_cmd_y_ = 0;
                last_speed_cmd_z_ = 0;
                need_stop = true;

                if (!compensation_watchdog_latched_) {
                    compensation_watchdog_latched_ = true;
                    need_warn = true;
                }
            }
        }

        if (need_stop) {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
        }

        if (need_warn) {
            RCLCPP_WARN(this->get_logger(),
                        "[Compensation] 补偿命令超时（>%d ms），已自动停滑台以防止持续吃旧命令",
                        compensation_watchdog_ms_);
        }
    }

    void publish_status()
    {
        if (!controller_) return;

        check_compensation_watchdog();

        base_interfaces_demo::msg::MotorStatus msg;
        std_msgs::msg::Float64MultiArray msg_std;
        msg_std.data.resize(9, 0.0);

        if (!can_online_.load()) {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            msg.x = last_valid_pos_.x;
            msg.y = last_valid_pos_.y;
            msg.z = last_valid_pos_.z;
            msg.vx = 0.0;
            msg.vy = 0.0;
            msg.vz = 0.0;
            msg.reached_target = false;

            msg_std.data[0] = msg.x;
            msg_std.data[1] = msg.y;
            msg_std.data[2] = msg.z;
            msg_std.data[3] = 0.0;
            msg_std.data[4] = 0.0;
            msg_std.data[5] = 0.0;
            msg_std.data[6] = 0.0;
            msg_std.data[7] = 0.0;
            msg_std.data[8] = static_cast<double>(current_mode_);

            status_publisher_->publish(msg);
            status_publisher_std_->publish(msg_std);
            return;
        }

        Position pos;
        double vx = 0.0, vy = 0.0, vz = 0.0;
        const auto now_tp = std::chrono::steady_clock::now();

        {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            pos = controller_->getCurrentPosition();
            vx = controller_->getCurrentSpeedX();
            vy = controller_->getCurrentSpeedY();
            vz = controller_->getCurrentSpeedZ();

            if (last_speed_request_tp_.time_since_epoch().count() == 0 ||
                now_tp - last_speed_request_tp_ >= std::chrono::milliseconds(speed_request_period_ms_)) {
                controller_->requestMotorSpeed();
                last_speed_request_tp_ = now_tp;
            }
        }

        update_position_reach_state(pos.x, pos.y, pos.z, vx, vy, vz);
        update_speed_safety_state(pos);

        {
            std::lock_guard<std::mutex> state_lock(state_mutex_);
            last_valid_pos_ = pos;
            msg.reached_target = target_reached_;
            msg_std.data[6] = target_reached_ ? 1.0 : 0.0;
            msg_std.data[7] = can_online_.load() ? 1.0 : 0.0;
            msg_std.data[8] = static_cast<double>(current_mode_);
        }

        msg.x = pos.x;
        msg.y = pos.y;
        msg.z = pos.z;
        msg.vx = vx;
        msg.vy = vy;
        msg.vz = vz;

        msg_std.data[0] = pos.x;
        msg_std.data[1] = pos.y;
        msg_std.data[2] = pos.z;
        msg_std.data[3] = vx;
        msg_std.data[4] = vy;
        msg_std.data[5] = vz;

        status_publisher_->publish(msg);
        status_publisher_std_->publish(msg_std);
    }

    bool checkPositionInRange(double x, double y, double z) const
    {
        return (x >= x_min_ && x <= x_max_ &&
                y >= y_min_ && y <= y_max_ &&
                z >= z_min_ && z <= z_max_);
    }

    int32_t sanitizeCompSpeed(double v_mmps, double deadband_mmps, double limit_mmps) const
    {
        if (!std::isfinite(v_mmps)) return 0;

        double v = v_mmps;
        if (std::abs(v) < deadband_mmps) {
            v = 0.0;
        }

        v = std::clamp(v, -std::abs(limit_mmps), std::abs(limit_mmps));
        return static_cast<int32_t>(std::lround(v));
    }

    void ensurePositionModeLocked(const char* reason)
    {
        if (drive_mode_state_ != DriveModeState::POSITION) {
            controller_->setMotorToPositionMode();
            drive_mode_state_ = DriveModeState::POSITION;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
            RCLCPP_INFO(this->get_logger(), "[DriveMode] -> POSITION (%s)",
                        reason ? reason : "unknown");
        } else {
            speed_mode_active_ = false;
            if (motion_halt_active_) {
                controller_->clearMotorHalt();
                motion_halt_active_ = false;
                RCLCPP_INFO(this->get_logger(), "[DriveStop] clear halt in POSITION (%s)",
                            reason ? reason : "unknown");
            }
        }
    }

    void ensureSpeedModeLocked(const char* reason)
    {
        if (drive_mode_state_ != DriveModeState::SPEED || !speed_mode_active_) {
            controller_->setMotorToSpeedMode();
            drive_mode_state_ = DriveModeState::SPEED;
            speed_mode_active_ = true;
            motion_halt_active_ = false;
            RCLCPP_INFO(this->get_logger(), "[DriveMode] -> SPEED (%s)",
                        reason ? reason : "unknown");
        } else {
            speed_mode_active_ = true;
            if (motion_halt_active_) {
                controller_->clearMotorHalt();
                motion_halt_active_ = false;
                RCLCPP_INFO(this->get_logger(), "[DriveStop] clear halt in SPEED (%s)",
                            reason ? reason : "unknown");
            }
        }
    }

    void stopMotionLocked()
    {
        if (drive_mode_state_ == DriveModeState::SPEED) {
            controller_->runMotorsSpeed(0, 0, 0);
            speed_mode_active_ = true;
            motion_halt_active_ = false;
        } else {
            controller_->haltMotors();
            motion_halt_active_ = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    void safeShutdown()
    {
        can_online_.store(false);

        if (!controller_) return;

        try {
            std::lock_guard<std::mutex> lock(controller_mutex_);
            stopMotionLocked();
            controller_->stopReadingPosition();
            controller_->powerOffMotors();
            controller_->shutdownCanInterface(can_interface_);
            drive_mode_state_ = DriveModeState::UNKNOWN;
            speed_mode_active_ = false;
            motion_halt_active_ = false;
        } catch (...) {
        }
    }

private:
    int tracer_id_{1};

    std::shared_ptr<MotorController> controller_;
    std::atomic<bool> can_online_{false};

    std::mutex controller_mutex_;
    std::mutex state_mutex_;

    bool speed_mode_active_{false};
    bool motion_halt_active_{false};
    DriveModeState drive_mode_state_{DriveModeState::UNKNOWN};

    bool target_reached_{false};
    ControlMode current_mode_{ControlMode::IDLE};
    TargetPose target_pose_;
    Position last_valid_pos_{};

    int position_settle_count_{0};

    int32_t last_speed_cmd_x_{0};
    int32_t last_speed_cmd_y_{0};
    int32_t last_speed_cmd_z_{0};

    bool compensation_velocity_active_{false};
    bool compensation_watchdog_latched_{false};
    std::chrono::steady_clock::time_point last_compensation_cmd_tp_{};

    std::string can_interface_;
    std::string command_topic_;
    std::string status_topic_;
    std::string comp_topic_;

    double x_min_{1e-6}, x_max_{278.0};
    double y_min_{1e-6}, y_max_{277.0};
    double z_min_{1e-6}, z_max_{200.0};

    double position_tolerance_mm_{0.5};
    double velocity_tolerance_mmps_{1.0};
    int settle_cycles_required_{5};
    int status_period_ms_{50};
    int speed_request_period_ms_{50};

    double compensation_vx_limit_mmps_{30.0};
    double compensation_vy_limit_mmps_{30.0};
    double compensation_vz_limit_mmps_{15.0};

    double compensation_vx_deadband_mmps_{0.1};
    double compensation_vy_deadband_mmps_{0.1};
    double compensation_vz_deadband_mmps_{0.1};

    int compensation_watchdog_ms_{500};
    bool compensation_allow_position_{true};
    bool compensation_position_is_relative_default_{true};

    std::chrono::steady_clock::time_point last_speed_request_tp_{};

    rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_subscriber_;
    rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr compensation_subscriber_;
    rclcpp::Publisher<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_publisher_;
    rclcpp::Publisher<std_msgs::msg::Float64MultiArray>::SharedPtr status_publisher_std_;
    rclcpp::TimerBase::SharedPtr status_timer_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);
    try {
        auto node = std::make_shared<MotorControllerNode>();
        rclcpp::spin(node);
        rclcpp::shutdown();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "listener_motor fatal error: " << ex.what() << std::endl;
        rclcpp::shutdown();
        return 1;
    }
}
