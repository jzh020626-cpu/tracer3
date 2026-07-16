// #include <sstream>
// #include "motor_httx_pos_spe/MotorController.h"
// #include <iomanip>
// #include <unistd.h>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <atomic>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <cmath>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// // 电机参数配置
// const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
// const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
// const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};

// // 全局控制变量
// std::atomic<bool> keepPrinting(false);
// std::atomic<bool> exitProgram(false);
// std::atomic<bool> targetReached(false);
// std::mutex cout_mutex;
// std::mutex pos_mutex;
// std::mutex motor_mutex;

// // 行程范围配置
// constexpr double x_min = 1e-6, x_max = 278.0;
// constexpr double y_min = 1e-6, y_max = 277.0;
// constexpr double z_min = 1e-6, z_max = 200.0;

// // 运行模式枚举
// enum class ControlMode {
//     POSITION,
//     SPEED
// };

// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node"), controller_("can2", X_MOTOR, Y_MOTOR, Z_MOTOR) {
//         // 初始化电机控制器
//         controller_.setupCanInterface("can2");
//         controller_.enableMotors();
//         controller_.startReadingPosition();
        
//         // 显示初始位置
//         Position init_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm", 
//                    init_pos.x, init_pos.y, init_pos.z);
        
//         // 创建订阅者
//         subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//             "motor_commands", 10, std::bind(&MotorControlNode::command_callback, this, std::placeholders::_1));
//     }
    
//     ~MotorControlNode() {
//         safeShutdown();
//     }

// private:
//     MotorController controller_;
//     std::thread monitorThread_;
//     ControlMode currentMode_ = ControlMode::POSITION;
//     rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr subscription_;
    
//     void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         // if (msg->command_type == "stop") {
//         //     resetMotionParameters();
//         //     return;
//         // }
//         if (msg->command_type == "stop") {
//             // 停止所有电机运动
//             controller_.runMotorsSpeed(0, 0, 0);
//             // 停止位置监控线程
//             if (monitorThread_.joinable()) {
//                 keepPrinting = false;
//                 monitorThread_.join();
//             }
//             // 重置状态标志
//             targetReached = true;
            
//             RCLCPP_INFO(this->get_logger(), "已完全停止所有电机运动");
//             return;
//         }
        
//         if (msg->command_type == "position") {
//             handlePositionCommand(msg);
//         } 
//         else if (msg->command_type == "speed") {
//             handleSpeedCommand(msg);
//         }
//     }
    
//     void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         double x = msg->x;
//         double y = msg->y;
//         double z = msg->z;
//         double time = msg->time;
//         bool relative = msg->is_relative;
        
//         if (time <= 0) {
//             RCLCPP_ERROR(this->get_logger(), "错误: 时间必须为正数！");
//             return;
//         }
        
//         if (relative) {
//             Position current = controller_.getCurrentPosition();
//             x += current.x;
//             y += current.y;
//             z += current.z;
            
//             if(fabs(x) < 1e-4) x = (x >= 0) ? 1e-3 : -1e-3;
//             if(fabs(y) < 1e-4) y = (y >= 0) ? 1e-3 : -1e-3;
//             if(fabs(z) < 1e-4) z = (z >= 0) ? 1e-3 : -1e-3;
//         }
        
//         if (!checkPositionInRange(x, y, z) ){
//             RCLCPP_ERROR(this->get_logger(), 
//                         "错误: 目标位置超出允许范围！X[%.2f,%.2f] Y[%.2f,%.2f] Z[%.2f,%.2f]",
//                         x_min, x_max, y_min, y_max, z_min, z_max);
//             return;
//         }
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::POSITION;
//         controller_.setMotorToPositionMode();
        
//         RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs %s",
//                    x, y, z, time, relative ? "(相对运动)" : "");
        
//         controller_.runMotors(x, y, z, time);
        
//         keepPrinting = true;
//         monitorThread_ = std::thread([this, x, y, z]() {
//             positionMonitoringThread(x, y, z);
//         });
//     }
    
//     void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         int32_t vx = msg->vx;
//         int32_t vy = msg->vy;
//         int32_t vz = msg->vz;
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::SPEED;
//         controller_.setMotorToSpeedMode();
        
//         RCLCPP_INFO(this->get_logger(), "设置速度: X=%dmm/s, Y=%dmm/s, Z=%dmm/s", vx, vy, vz);
        
//         if (vx != 0 || vy != 0 || vz != 0) {
//             controller_.runMotorsSpeed(vx, vy, vz);
            
//             keepPrinting = true;
//             monitorThread_ = std::thread([this, vx, vy, vz]() {
//                 speedMonitoringThread(vx, vy, vz);
//             });
//         } else {
//             controller_.runMotorsSpeed(0, 0, 0);
//             RCLCPP_INFO(this->get_logger(), "速度为零，电机已停止");
//         }
//     }
    
//     void positionMonitoringThread(double target_x, double target_y, double target_z) {
//         const double threshold = 0.01;
//         bool position_stable = false;
        
//         Position last_pos = controller_.getCurrentPosition();
//         auto start_time = std::chrono::steady_clock::now();
        
//         while (keepPrinting && !exitProgram && !position_stable) {
//             auto now = std::chrono::steady_clock::now();
//             auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
//             if (elapsed >= 100) {
//                 Position current_pos = controller_.getCurrentPosition();
                
//                 RCLCPP_INFO(this->get_logger(), 
//                            "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 目标位置: X=%.2f Y=%.2f Z=%.2f",
//                            current_pos.x, current_pos.y, current_pos.z, target_x, target_y, target_z);

//                 double dx = std::abs(current_pos.x - last_pos.x);
//                 double dy = std::abs(current_pos.y - last_pos.y);
//                 double dz = std::abs(current_pos.z - last_pos.z);
                
//                 if (dx < threshold && dy < threshold && dz < threshold) {
//                     position_stable = true;
//                     targetReached = true;
//                     RCLCPP_INFO(this->get_logger(), "已到达目标位置");
//                     RCLCPP_INFO(this->get_logger(), "最终坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                                current_pos.x, current_pos.y, current_pos.z);
//                 }
                
//                 last_pos = current_pos;
//                 start_time = now;
//             }
            
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }
//     }
    
//     void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
//         while (keepPrinting && !exitProgram) {
//             Position current_pos = controller_.getCurrentPosition();
            
//             RCLCPP_INFO(this->get_logger(), 
//                        "实时坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 运动方向: X=%s%d Y=%s%d Z=%s%d",
//                        current_pos.x, current_pos.y, current_pos.z,
//                        (vx > 0 ? "+" : "-"), std::abs(vx),
//                        (vy > 0 ? "+" : "-"), std::abs(vy),
//                        (vz > 0 ? "+" : "-"), std::abs(vz));

//             if (isAtLimitPosition(current_pos, vx, vy, vz)) {
//                 controller_.runMotorsSpeed(0, 0, 0);
//                 targetReached = true;
//                 RCLCPP_INFO(this->get_logger(), "已到达极限位置，电机已停止");
//                 break;
//             }

//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//     }
    
//     bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
//         return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
//                ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
//                ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
//     }
    
//     void resetMotionParameters() {
//         if (monitorThread_.joinable()) {
//             keepPrinting = false;
//             monitorThread_.join();
//         }
        
//         for (int i = 0; i < 3; ++i) {
//             controller_.runMotorsSpeed(0, 0, 0);
//             usleep(5000);
//         }
//         targetReached = true;
        
//         Position current_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "已停止当前运动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                    current_pos.x, current_pos.y, current_pos.z);
//     }
    
//     void safeShutdown() {
//         exitProgram = true;
//         keepPrinting = false;
        
//         RCLCPP_INFO(this->get_logger(), "正在停止监控线程...");
//         controller_.stopReadingPosition();
//         controller_.runMotorsSpeed(0, 0, 0);
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭电机电源...");
//         controller_.powerOffMotors();
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭CAN接口...");
//         controller_.shutdownCanInterface("can2");
        
//         RCLCPP_INFO(this->get_logger(), "系统已安全关闭");
//     }
    
//     bool checkPositionInRange(double x, double y, double z) {
//         return (x >= x_min && x <= x_max && 
//                 y >= y_min && y <= y_max && 
//                 z >= z_min && z <= z_max);
//     }
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<MotorControlNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }



// #include <sstream>
// #include <iomanip>
// #include <unistd.h>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <atomic>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <cmath>
// #include <csignal>
// #include "motor_httx_pos_spe/MotorController.h"
// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// // 电机参数配置
// const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
// const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
// const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};

// // 全局控制变量
// std::atomic<bool> keepPrinting(false);
// std::atomic<bool> exitProgram(false);
// std::atomic<bool> targetReached(false);
// std::mutex cout_mutex;
// std::mutex pos_mutex;
// std::mutex motor_mutex;

// // 行程范围配置
// constexpr double x_min = 1e-6, x_max = 278.0;
// constexpr double y_min = 1e-6, y_max = 277.0;
// constexpr double z_min = 1e-6, z_max = 200.0;

// // 运行模式枚举
// enum class ControlMode {
//     POSITION,
//     SPEED
// };

// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node"), controller_("can2", X_MOTOR, Y_MOTOR, Z_MOTOR) {
//         // 初始化电机控制器（不检查返回值）
//         controller_.setupCanInterface("can2");
//         controller_.enableMotors();
//         controller_.startReadingPosition();
        
//         // 显示初始位置
//         Position init_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm", 
//                    init_pos.x, init_pos.y, init_pos.z);
        
//         // 创建订阅者
//         subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//             "motor_commands", 10, std::bind(&MotorControlNode::command_callback, this, std::placeholders::_1));
//     }
    
//     ~MotorControlNode() {
//         safeShutdown();
//     }

// private:
//     MotorController controller_;
//     std::thread monitorThread_;
//     ControlMode currentMode_ = ControlMode::POSITION;
//     rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr subscription_;
    
//     void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {

//         RCLCPP_INFO(this->get_logger(), "command_type的值为: %s",msg->command_type.c_str());

//         if (msg->command_type == "stop") {

//             // 停止所有电机运动（不检查返回值）
//             controller_.runMotorsSpeed(0, 0, 0);
//             // safeShutdown(controller_);
//             // 停止位置监控线程
//             if (monitorThread_.joinable()) {
//                 keepPrinting = false;
//                 monitorThread_.join();
//             }
//             // 重置状态标志
//             targetReached = true;
            
//             RCLCPP_INFO(this->get_logger(), "已完全停止所有电机运动");
//             return;
//         }
        

//         if (msg->command_type == "position") {
//             handlePositionCommand(msg);
//         } 
//         else if (msg->command_type == "speed") {
//             handleSpeedCommand(msg);
//         }
//     }
    
//     void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         double x = msg->x;
//         double y = msg->y;
//         double z = msg->z;
//         double time = msg->time;
//         bool relative = msg->is_relative;
        
//         if (time <= 0) {
//             RCLCPP_ERROR(this->get_logger(), "错误: 时间必须为正数！");
//             return;
//         }
        
//         if (relative) {
//             Position current = controller_.getCurrentPosition();
//             x += current.x;
//             y += current.y;
//             z += current.z;
            
//             if(fabs(x) < 1e-4) x = (x >= 0) ? 1e-3 : -1e-3;
//             if(fabs(y) < 1e-4) y = (y >= 0) ? 1e-3 : -1e-3;
//             if(fabs(z) < 1e-4) z = (z >= 0) ? 1e-3 : -1e-3;
//         }
        
//         if (!checkPositionInRange(x, y, z)) {
//             RCLCPP_ERROR(this->get_logger(), 
//                         "错误: 目标位置超出允许范围！X[%.2f,%.2f] Y[%.2f,%.2f] Z[%.2f,%.2f]",
//                         x_min, x_max, y_min, y_max, z_min, z_max);
//             return;
//         }
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::POSITION;
//         controller_.setMotorToPositionMode();
        
//         RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs %s",
//                    x, y, z, time, relative ? "(相对运动)" : "");
        
//         controller_.runMotors(x, y, z, time);
        
//         keepPrinting = true;
//         monitorThread_ = std::thread([this, x, y, z]() {
//             positionMonitoringThread(x, y, z);
//         });
//     }
    
//     void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         int32_t vx = msg->vx;
//         int32_t vy = msg->vy;
//         int32_t vz = msg->vz;
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::SPEED;
//         controller_.setMotorToSpeedMode();
        
//         RCLCPP_INFO(this->get_logger(), "设置速度: X=%dmm/s, Y=%dmm/s, Z=%dmm/s", vx, vy, vz);
        
//         if (vx != 0 || vy != 0 || vz != 0) {
//             controller_.runMotorsSpeed(vx, vy, vz);
            
//             keepPrinting = true;
//             monitorThread_ = std::thread([this, vx, vy, vz]() {
//                 speedMonitoringThread(vx, vy, vz);
//             });
//         } else {
//             controller_.runMotorsSpeed(0, 0, 0);
//             RCLCPP_INFO(this->get_logger(), "速度为零，电机已停止");
//         }
//     }
    
//     void positionMonitoringThread(double target_x, double target_y, double target_z) {
//         const double threshold = 0.01;
//         bool position_stable = false;
        
//         Position last_pos = controller_.getCurrentPosition();
//         auto start_time = std::chrono::steady_clock::now();
        
//         while (keepPrinting && !exitProgram && !position_stable) {
//             auto now = std::chrono::steady_clock::now();
//             auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
//             if (elapsed >= 100) {
//                 Position current_pos = controller_.getCurrentPosition();
                
//                 RCLCPP_INFO(this->get_logger(), 
//                            "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 目标位置: X=%.2f Y=%.2f Z=%.2f",
//                            current_pos.x, current_pos.y, current_pos.z, target_x, target_y, target_z);

//                 double dx = std::abs(current_pos.x - last_pos.x);
//                 double dy = std::abs(current_pos.y - last_pos.y);
//                 double dz = std::abs(current_pos.z - last_pos.z);
                
//                 if (dx < threshold && dy < threshold && dz < threshold) {
//                     position_stable = true;
//                     targetReached = true;
//                     RCLCPP_INFO(this->get_logger(), "已到达目标位置");
//                     RCLCPP_INFO(this->get_logger(), "最终坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                                current_pos.x, current_pos.y, current_pos.z);
//                 }
                
//                 last_pos = current_pos;
//                 start_time = now;
//             }
            
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }
//     }
    
//     void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
//         while (keepPrinting && !exitProgram) {
//             Position current_pos = controller_.getCurrentPosition();
            
//             RCLCPP_INFO(this->get_logger(), 
//                        "实时坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 运动方向: X=%s%d Y=%s%d Z=%s%d",
//                        current_pos.x, current_pos.y, current_pos.z,
//                        (vx > 0 ? "+" : "-"), std::abs(vx),
//                        (vy > 0 ? "+" : "-"), std::abs(vy),
//                        (vz > 0 ? "+" : "-"), std::abs(vz));

//             if (isAtLimitPosition(current_pos, vx, vy, vz)) {
//                 controller_.runMotorsSpeed(0, 0, 0);
//                 targetReached = true;
//                 RCLCPP_INFO(this->get_logger(), "已到达极限位置，电机已停止");
//                 break;
//             }

//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//     }
    
//     bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
//         return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
//                ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
//                ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
//     }
    
//     void resetMotionParameters() {
//         if (monitorThread_.joinable()) {
//             keepPrinting = false;
//             monitorThread_.join();
//         }
        
//         for (int i = 0; i < 3; ++i) {
//             controller_.runMotorsSpeed(0, 0, 0);
//             usleep(5000);
//         }
//         targetReached = true;
        
//         Position current_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "已停止当前运动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                    current_pos.x, current_pos.y, current_pos.z);
//     }
    
//     void safeShutdown() {
//         exitProgram = true;
//         keepPrinting = false;
        
//         RCLCPP_INFO(this->get_logger(), "正在停止监控线程...");
//         controller_.stopReadingPosition();
//         controller_.runMotorsSpeed(0, 0, 0);
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭电机电源...");
//         controller_.powerOffMotors();
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭CAN接口...");
//         controller_.shutdownCanInterface("can2");
        
//         RCLCPP_INFO(this->get_logger(), "系统已安全关闭");
//     }
    
//     bool checkPositionInRange(double x, double y, double z) {
//         return (x >= x_min && x <= x_max && 
//                 y >= y_min && y <= y_max && 
//                 z >= z_min && z <= z_max);
//     }
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<MotorControlNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }




// #include <sstream>
// #include <iomanip>
// #include <unistd.h>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <atomic>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <cmath>
// #include <csignal>
// #include "motor_httx_pos_spe/MotorController.h"
// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// // 电机参数配置
// const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
// const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
// const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};

// // 全局控制变量
// std::atomic<bool> keepPrinting(false);
// std::atomic<bool> exitProgram(false);
// std::atomic<bool> targetReached(false);
// std::mutex cout_mutex;
// std::mutex pos_mutex;
// std::mutex motor_mutex;

// // 行程范围配置
// constexpr double x_min = 1e-6, x_max = 278.0;
// constexpr double y_min = 1e-6, y_max = 277.0;
// constexpr double z_min = 1e-6, z_max = 200.0;

// // 运行模式枚举
// enum class ControlMode {
//     POSITION,
//     SPEED
// };

// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node"), controller_("can2", X_MOTOR, Y_MOTOR, Z_MOTOR) {
//         // 初始化电机控制器（不检查返回值）
//         controller_.setupCanInterface("can2");
//         controller_.enableMotors();
//         controller_.startReadingPosition();
        
//         // 显示初始位置
//         Position init_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm", 
//                    init_pos.x, init_pos.y, init_pos.z);
        
//         // 创建订阅者
//         subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//             "motor_commands", 10, std::bind(&MotorControlNode::command_callback, this, std::placeholders::_1));
//     }
    
//     ~MotorControlNode() {
//         safeShutdown();
//     }

// private:
//     MotorController controller_;
//     std::thread monitorThread_;
//     ControlMode currentMode_ = ControlMode::POSITION;
//     rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr subscription_;
    
//     void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         RCLCPP_INFO(this->get_logger(), "收到命令: %s", msg->command_type.c_str());

//         if (msg->command_type == "stop") {
//             // 停止所有电机运动
//             controller_.runMotorsSpeed(0, 0, 0);
//             // 停止位置监控线程
//             if (monitorThread_.joinable()) {
//                 keepPrinting = false;
//                 monitorThread_.join();
//             }
//             // 重置状态标志
//             targetReached = true;
            
//             RCLCPP_INFO(this->get_logger(), "已完全停止所有电机运动");
//             return;
//         }
        
//         if (msg->command_type == "position") {
//             handlePositionCommand(msg);
//         } 
//         else if (msg->command_type == "speed") {
//             handleSpeedCommand(msg);
//         }
//     }
    
//     void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         double x = msg->x;
//         double y = msg->y;
//         double z = msg->z;
//         double time = msg->time;
//         bool relative = msg->is_relative;
        
//         if (time <= 0) {
//             RCLCPP_ERROR(this->get_logger(), "错误: 时间必须为正数！");
//             return;
//         }
        
//         if (relative) {
//             Position current = controller_.getCurrentPosition();
//             x += current.x;
//             y += current.y;
//             z += current.z;
            
//             if(fabs(x) < 1e-4) x = (x >= 0) ? 1e-3 : -1e-3;
//             if(fabs(y) < 1e-4) y = (y >= 0) ? 1e-3 : -1e-3;
//             if(fabs(z) < 1e-4) z = (z >= 0) ? 1e-3 : -1e-3;
//         }
        
//         if (!checkPositionInRange(x, y, z)) {
//             RCLCPP_ERROR(this->get_logger(), 
//                         "错误: 目标位置超出允许范围！X[%.2f,%.2f] Y[%.2f,%.2f] Z[%.2f,%.2f]",
//                         x_min, x_max, y_min, y_max, z_min, z_max);
//             return;
//         }
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::POSITION;
//         controller_.setMotorToPositionMode();
        
//         RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs %s",
//                    x, y, z, time, relative ? "(相对运动)" : "");
        
//         controller_.runMotors(x, y, z, time);
        
//         keepPrinting = true;
//         monitorThread_ = std::thread([this, x, y, z]() {
//             positionMonitoringThread(x, y, z);
//         });
//     }
    
//     void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         int32_t vx = msg->vx;
//         int32_t vy = msg->vy;
//         int32_t vz = msg->vz;
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::SPEED;
//         controller_.setMotorToSpeedMode();
        
//         RCLCPP_INFO(this->get_logger(), "设置速度: X=%dmm/s, Y=%dmm/s, Z=%dmm/s", vx, vy, vz);
        
//         if (vx != 0 || vy != 0 || vz != 0) {
//             controller_.runMotorsSpeed(vx, vy, vz);
            
//             keepPrinting = true;
//             monitorThread_ = std::thread([this, vx, vy, vz]() {
//                 speedMonitoringThread(vx, vy, vz);
//             });
//         } else {
//             controller_.runMotorsSpeed(0, 0, 0);
//             RCLCPP_INFO(this->get_logger(), "速度为零，电机已停止");
//         }
//     }
    
//     void positionMonitoringThread(double target_x, double target_y, double target_z) {
//         const double threshold = 0.01;
//         bool position_stable = false;
        
//         Position last_pos = controller_.getCurrentPosition();
//         auto start_time = std::chrono::steady_clock::now();
        
//         while (keepPrinting && !exitProgram && !position_stable) {
//             auto now = std::chrono::steady_clock::now();
//             auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
//             if (elapsed >= 100) {
//                 Position current_pos = controller_.getCurrentPosition();
                
//                 RCLCPP_INFO(this->get_logger(), 
//                            "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 目标位置: X=%.2f Y=%.2f Z=%.2f",
//                            current_pos.x, current_pos.y, current_pos.z, target_x, target_y, target_z);

//                 double dx = std::abs(current_pos.x - last_pos.x);
//                 double dy = std::abs(current_pos.y - last_pos.y);
//                 double dz = std::abs(current_pos.z - last_pos.z);
                
//                 if (dx < threshold && dy < threshold && dz < threshold) {
//                     position_stable = true;
//                     targetReached = true;
//                     RCLCPP_INFO(this->get_logger(), "已到达目标位置");
//                     RCLCPP_INFO(this->get_logger(), "最终坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                                current_pos.x, current_pos.y, current_pos.z);
//                 }
                
//                 last_pos = current_pos;
//                 start_time = now;
//             }
            
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }
//     }
    
//     void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
//         while (keepPrinting && !exitProgram) {
//             Position current_pos = controller_.getCurrentPosition();
            
//             RCLCPP_INFO(this->get_logger(), 
//                        "实时坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 运动方向: X=%s%d Y=%s%d Z=%s%d",
//                        current_pos.x, current_pos.y, current_pos.z,
//                        (vx > 0 ? "+" : "-"), std::abs(vx),
//                        (vy > 0 ? "+" : "-"), std::abs(vy),
//                        (vz > 0 ? "+" : "-"), std::abs(vz));

//             if (isAtLimitPosition(current_pos, vx, vy, vz)) {
//                 controller_.runMotorsSpeed(0, 0, 0);
//                 targetReached = true;
//                 RCLCPP_INFO(this->get_logger(), "已到达极限位置，电机已停止");
//                 break;
//             }

//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//     }
    
//     bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
//         return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
//                ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
//                ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
//     }
    
//     void resetMotionParameters() {
//         if (monitorThread_.joinable()) {
//             keepPrinting = false;
//             monitorThread_.join();
//         }
        
//         for (int i = 0; i < 3; ++i) {
//             controller_.runMotorsSpeed(0, 0, 0);
//             usleep(5000);
//         }
//         targetReached = true;
        
//         Position current_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "已停止当前运动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                    current_pos.x, current_pos.y, current_pos.z);
//     }
    
//     void safeShutdown() {
//         exitProgram = true;
//         keepPrinting = false;
        
//         RCLCPP_INFO(this->get_logger(), "正在停止监控线程...");
//         controller_.stopReadingPosition();
//         controller_.runMotorsSpeed(0, 0, 0);
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭电机电源...");
//         controller_.powerOffMotors();
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭CAN接口...");
//         controller_.shutdownCanInterface("can2");
        
//         RCLCPP_INFO(this->get_logger(), "系统已安全关闭");
//     }
    
//     bool checkPositionInRange(double x, double y, double z) {
//         return (x >= x_min && x <= x_max && 
//                 y >= y_min && y <= y_max && 
//                 z >= z_min && z <= z_max);
//     }
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<MotorControlNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }




// #include <sstream>
// #include <iomanip>
// #include <unistd.h>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <atomic>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <cmath>
// #include <csignal>
// #include "motor_httx_pos_spe/MotorController.h"
// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// // 电机参数配置
// const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
// const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
// const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};

// // 全局控制变量
// std::atomic<bool> keepPrinting(false);
// std::atomic<bool> exitProgram(false);
// std::atomic<bool> targetReached(false);
// std::mutex cout_mutex;
// std::mutex pos_mutex;
// std::mutex motor_mutex;

// // 行程范围配置
// constexpr double x_min = 1e-6, x_max = 278.0;
// constexpr double y_min = 1e-6, y_max = 277.0;
// constexpr double z_min = 1e-6, z_max = 200.0;

// // 运行模式枚举
// enum class ControlMode {
//     POSITION,
//     SPEED
// };

// class MotorControlNode : public rclcpp::Node {
// public:
//     MotorControlNode() : Node("motor_control_node"), controller_("can2", X_MOTOR, Y_MOTOR, Z_MOTOR) {
//         // 初始化电机控制器
//         controller_.setupCanInterface("can2");
//         controller_.enableMotors();
//         controller_.startReadingPosition();
        
//         // 显示初始位置
//         Position init_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm", 
//                    init_pos.x, init_pos.y, init_pos.z);
        
//         // 创建订阅者
//         subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//             "motor_commands", 10, std::bind(&MotorControlNode::command_callback, this, std::placeholders::_1));
//     }
    
//     ~MotorControlNode() {
//         safeShutdown();
//     }

// private:
//     MotorController controller_;
//     std::thread monitorThread_;
//     ControlMode currentMode_ = ControlMode::POSITION;
//     rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr subscription_;
    
//     void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         RCLCPP_INFO(this->get_logger(), "收到命令: %s", msg->command_type.c_str());

//         if (msg->command_type == "stop") {
//             // 停止所有电机运动
//             controller_.runMotorsSpeed(0, 0, 0);
//             // 停止位置监控线程
//             if (monitorThread_.joinable()) {
//                 keepPrinting = false;
//                 monitorThread_.join();
//             }
//             // 重置状态标志
//             targetReached = true;
            
//             RCLCPP_INFO(this->get_logger(), "已完全停止所有电机运动");
//             return;
//         }
        
//         if (msg->command_type == "position") {
//             handlePositionCommand(msg);
//         } 
//         else if (msg->command_type == "speed") {
//             handleSpeedCommand(msg);
//         }
//     }
    
//     void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         double x = msg->x;
//         double y = msg->y;
//         double z = msg->z;
//         double time = msg->time;
//         bool relative = msg->is_relative;
        
//         if (time <= 0) {
//             RCLCPP_ERROR(this->get_logger(), "错误: 时间必须为正数！");
//             return;
//         }
        
//         if (relative) {
//             Position current = controller_.getCurrentPosition();
//             x += current.x;
//             y += current.y;
//             z += current.z;
            
//             if(fabs(x) < 1e-4) x = (x >= 0) ? 1e-3 : -1e-3;
//             if(fabs(y) < 1e-4) y = (y >= 0) ? 1e-3 : -1e-3;
//             if(fabs(z) < 1e-4) z = (z >= 0) ? 1e-3 : -1e-3;
//         }
        
//         if (!checkPositionInRange(x, y, z)) {
//             RCLCPP_ERROR(this->get_logger(), 
//                         "错误: 目标位置超出允许范围！X[%.2f,%.2f] Y[%.2f,%.2f] Z[%.2f,%.2f]",
//                         x_min, x_max, y_min, y_max, z_min, z_max);
//             return;
//         }
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::POSITION;
//         controller_.setMotorToPositionMode();
        
//         RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs %s",
//                    x, y, z, time, relative ? "(相对运动)" : "");
        
//         controller_.runMotors(x, y, z, time);
        
//         keepPrinting = true;
//         monitorThread_ = std::thread([this, x, y, z]() {
//             positionMonitoringThread(x, y, z);
//         });
//     }
    
//     void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
//         int32_t vx = msg->vx;
//         int32_t vy = msg->vy;
//         int32_t vz = msg->vz;
        
//         resetMotionParameters();
//         currentMode_ = ControlMode::SPEED;
//         controller_.setMotorToSpeedMode();
        
//         RCLCPP_INFO(this->get_logger(), "设置速度: X=%dmm/s, Y=%dmm/s, Z=%dmm/s", vx, vy, vz);
        
//         if (vx != 0 || vy != 0 || vz != 0) {
//             controller_.runMotorsSpeed(vx, vy, vz);
            
//             keepPrinting = true;
//             monitorThread_ = std::thread([this, vx, vy, vz]() {
//                 speedMonitoringThread(vx, vy, vz);
//             });
//         } else {
//             controller_.runMotorsSpeed(0, 0, 0);
//             RCLCPP_INFO(this->get_logger(), "速度为零，电机已停止");
//         }
//     }
    
//     void positionMonitoringThread(double target_x, double target_y, double target_z) {
//         const double threshold = 0.01;
//         bool position_stable = false;
        
//         Position last_pos = controller_.getCurrentPosition();
//         auto start_time = std::chrono::steady_clock::now();
        
//         while (keepPrinting && !exitProgram && !position_stable) {
//             auto now = std::chrono::steady_clock::now();
//             auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
            
//             if (elapsed >= 100) {
//                 Position current_pos = controller_.getCurrentPosition();
                
//                 RCLCPP_INFO(this->get_logger(), 
//                            "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 目标位置: X=%.2f Y=%.2f Z=%.2f",
//                            current_pos.x, current_pos.y, current_pos.z, target_x, target_y, target_z);

//                 double dx = std::abs(current_pos.x - last_pos.x);
//                 double dy = std::abs(current_pos.y - last_pos.y);
//                 double dz = std::abs(current_pos.z - last_pos.z);
                
//                 if (dx < threshold && dy < threshold && dz < threshold) {
//                     position_stable = true;
//                     targetReached = true;
//                     RCLCPP_INFO(this->get_logger(), "已到达目标位置");
//                     RCLCPP_INFO(this->get_logger(), "最终坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                                current_pos.x, current_pos.y, current_pos.z);
//                 }
                
//                 last_pos = current_pos;
//                 start_time = now;
//             }
            
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }
//     }
    
//     void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
//         while (keepPrinting && !exitProgram) {
//             Position current_pos = controller_.getCurrentPosition();
            
//             RCLCPP_INFO(this->get_logger(), 
//                        "实时坐标: X=%.2fmm, Y=%.2fmm, Z=%.2fmm | 运动方向: X=%s%d Y=%s%d Z=%s%d",
//                        current_pos.x, current_pos.y, current_pos.z,
//                        (vx > 0 ? "+" : "-"), std::abs(vx),
//                        (vy > 0 ? "+" : "-"), std::abs(vy),
//                        (vz > 0 ? "+" : "-"), std::abs(vz));

//             if (isAtLimitPosition(current_pos, vx, vy, vz)) {
//                 controller_.runMotorsSpeed(0, 0, 0);
//                 targetReached = true;
//                 RCLCPP_INFO(this->get_logger(), "已到达极限位置，电机已停止");
//                 break;
//             }

//             std::this_thread::sleep_for(std::chrono::milliseconds(100));
//         }
//     }
    
//     bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
//         return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
//                ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
//                ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
//     }
    
//     void resetMotionParameters() {
//         if (monitorThread_.joinable()) {
//             keepPrinting = false;
//             monitorThread_.join();
//         }
        
//         for (int i = 0; i < 3; ++i) {
//             controller_.runMotorsSpeed(0, 0, 0);
//             usleep(5000);
//         }
//         targetReached = true;
        
//         Position current_pos = controller_.getCurrentPosition();
//         RCLCPP_INFO(this->get_logger(), "已停止当前运动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm",
//                    current_pos.x, current_pos.y, current_pos.z);
//     }
    
//     void safeShutdown() {
//         exitProgram = true;
//         keepPrinting = false;
        
//         RCLCPP_INFO(this->get_logger(), "正在停止监控线程...");
//         controller_.stopReadingPosition();
//         controller_.runMotorsSpeed(0, 0, 0);
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭电机电源...");
//         controller_.powerOffMotors();
        
//         RCLCPP_INFO(this->get_logger(), "正在关闭CAN接口...");
//         controller_.shutdownCanInterface("can2");
        
//         RCLCPP_INFO(this->get_logger(), "系统已安全关闭");
//     }
    
//     bool checkPositionInRange(double x, double y, double z) {
//         return (x >= x_min && x <= x_max && 
//                 y >= y_min && y <= y_max && 
//                 z >= z_min && z <= z_max);
//     }
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<MotorControlNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }




// #include <memory>
// #include <sstream>
// #include <iomanip>
// #include <unistd.h>
// #include <iostream>
// #include <string>
// #include <vector>
// #include <algorithm>
// #include <atomic>
// #include <thread>
// #include <chrono>
// #include <mutex>
// #include <cmath>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"
// #include "base_interfaces_demo/msg/motor_status.hpp"
// #include "motor_httx_pos_spe/MotorController.h"

// using namespace std::chrono_literals;

// // 电机参数配置
// const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
// const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
// const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};

// // 全局控制变量
// std::atomic<bool> keepPrinting(false);
// std::atomic<bool> exitProgram(false);
// std::atomic<bool> targetReached(false);
// std::mutex cout_mutex;
// std::mutex pos_mutex;

// // 行程范围配置
// constexpr double x_min = 1e-6, x_max = 278.0;
// constexpr double y_min = 1e-6, y_max = 277.0;
// constexpr double z_min = 1e-6, z_max = 200.0;

// // 运行模式枚举
// enum class ControlMode {
//     POSITION,
//     SPEED
// };

// // 电机控制器全局实例
// std::shared_ptr<MotorController> controller;
// ControlMode currentMode = ControlMode::POSITION;
// std::thread monitorThread;

// // ================= 位置模式相关函数 =================

// void positionMonitoringThread(double target_x, double target_y, double target_z) {
//     const double threshold = 0.01;
//     bool position_stable = false;
    
//     Position last_pos;
//     {
//         std::lock_guard<std::mutex> lock(pos_mutex);
//         last_pos = controller->getCurrentPosition();
//     }
    
//     auto start_time = std::chrono::steady_clock::now();
    
//     while (keepPrinting && !exitProgram && !position_stable) {
//         auto now = std::chrono::steady_clock::now();
//         auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
//         if (elapsed >= 100) {
//             Position current_pos;
//             {
//                 std::lock_guard<std::mutex> lock(pos_mutex);
//                 current_pos = controller->getCurrentPosition();
//             }
            
//             {
//                 std::lock_guard<std::mutex> lock(cout_mutex);
//                 std::cout << "\r";
//                 std::cout << std::fixed << std::setprecision(2);
//                 std::cout << "当前位置: X=" << current_pos.x << "mm, Y=" << current_pos.y 
//                           << "mm, Z=" << current_pos.z << "mm";
//                 std::cout << " | 目标位置: X=" << target_x << " Y=" << target_y << " Z=" << target_z;
//                 std::cout << std::flush;
//             }

//             double dx = std::abs(current_pos.x - last_pos.x);
//             double dy = std::abs(current_pos.y - last_pos.y);
//             double dz = std::abs(current_pos.z - last_pos.z);
            
//             if (dx < threshold && dy < threshold && dz < threshold) {
//                 position_stable = true;
//                 targetReached = true;
//                 {
//                     std::lock_guard<std::mutex> lock(cout_mutex);
//                     std::cout << "\n已到达目标位置" << std::endl;
//                     std::cout << "最终坐标: X=" << current_pos.x << "mm, Y=" << current_pos.y 
//                               << "mm, Z=" << current_pos.z << "mm" << std::endl;
//                 }
//             }
            
//             last_pos = current_pos;
//             start_time = now;
//         }
        
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }
// }

// // ================= 速度模式相关函数 =================

// bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
//     return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
//            ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
//            ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
// }

// void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
//     while (keepPrinting && !exitProgram) {
//         Position current_pos;
//         {
//             std::lock_guard<std::mutex> lock(pos_mutex);
//             current_pos = controller->getCurrentPosition();
//         }
        
//         {
//             std::lock_guard<std::mutex> lock(cout_mutex);
//             std::cout << "\r";
//             std::cout << std::fixed << std::setprecision(2);
//             std::cout << "实时坐标: X=" << current_pos.x << "mm, Y=" << current_pos.y 
//                       << "mm, Z=" << current_pos.z << "mm";
//             std::cout << " | 运动方向: X=" << (vx > 0 ? "+" : "-") << std::abs(vx)
//                       << " Y=" << (vy > 0 ? "+" : "-") << std::abs(vy)
//                       << " Z=" << (vz > 0 ? "+" : "-") << std::abs(vz);
//             std::cout << std::flush;
//         }

//         if (isAtLimitPosition(current_pos, vx, vy, vz)) {
//             controller->runMotorsSpeed(0, 0, 0);
//             targetReached = true;
//             {
//                 std::lock_guard<std::mutex> lock(cout_mutex);
//                 std::cout << "\n已到达极限位置，电机已停止" << std::endl;
//             }
//             break;
//         }

//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }
// }

// // ================= 通用函数 =================

// void safeShutdown() {
//     exitProgram = true;
//     keepPrinting = false;
    
//     {
//         std::lock_guard<std::mutex> lock(cout_mutex);
//         RCLCPP_INFO(rclcpp::get_logger("motor_controller"), "正在停止监控线程...");
//     }
    
//     controller->stopReadingPosition();
//     controller->runMotorsSpeed(0, 0, 0);
    
//     {
//         std::lock_guard<std::mutex> lock(cout_mutex);
//         RCLCPP_INFO(rclcpp::get_logger("motor_controller"), "正在关闭电机电源...");
//     }
    
//     controller->powerOffMotors();
    
//     {
//         std::lock_guard<std::mutex> lock(cout_mutex);
//         RCLCPP_INFO(rclcpp::get_logger("motor_controller"), "系统已安全关闭");
//     }
// }

// bool checkPositionInRange(double x, double y, double z) {
//     return (x >= x_min && x <= x_max && 
//             y >= y_min && y <= y_max && 
//             z >= z_min && z <= z_max);
// }

// // ================= ROS2 订阅者类 =================

// class MotorControllerNode : public rclcpp::Node
// {
// public:
//     MotorControllerNode() : Node("motor_controller")
//     {
//         //创建目标位置订阅者
//         // subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//         //     "huatai3_pos_spe_pd", 10, std::bind(&MotorControllerNode::command_callback, this, std::placeholders::_1));
//         subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
//                "huatai3_pos_spe_pd", 10,
//                [this](const base_interfaces_demo::msg::MotorCommand::SharedPtr msg1) {
//                 this->command_callback(msg1);
//             });

//         // 创建当前位置发布者
//         position_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorStatus>(
//             "huatai3_pos_spe_p", 10);

        
//         // 初始化电机控制器
//         controller = std::make_shared<MotorController>("can2", X_MOTOR, Y_MOTOR, Z_MOTOR);
//         controller->setupCanInterface("can2");
//         controller->enableMotors();
//         controller->startReadingPosition();
        
//         Position init_pos;
//         {
//             std::lock_guard<std::mutex> lock(pos_mutex);
//             init_pos = controller->getCurrentPosition();
//         }
        
//         RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
//         RCLCPP_INFO(this->get_logger(), "当前位置: X=%.2fmm, Y=%.2fmm, Z=%.2fmm", 
//                    init_pos.x, init_pos.y, init_pos.z);
//     }

//     ~MotorControllerNode() {
//         safeShutdown();
//         if (monitorThread.joinable()) {
//             monitorThread.join();
//         }
//     }

// private:
//     void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg1)
//     {
//         std::lock_guard<std::mutex> lock(cout_mutex);
//         RCLCPP_INFO(this->get_logger(), "收到命令: 类型=%s", msg1->command_type.c_str());
        
//         targetReached = false;
        
//         if (msg1->command_type == "position") {
//             // 位置模式处理
//             double x = msg1->x;
//             double y = msg1->y;
//             double z = msg1->z;
            
//             if (msg1->is_relative) {
//                 Position current;
//                 {
//                     std::lock_guard<std::mutex> lock(pos_mutex);
//                     current = controller->getCurrentPosition();
//                 }
//                 x += current.x;
//                 y += current.y;
//                 z += current.z;
//             }
            
//             if (!checkPositionInRange(x, y, z)) {
//                 RCLCPP_ERROR(this->get_logger(), "目标位置超出允许范围！");
//                 return;
//             }
            
//             if (monitorThread.joinable()) {
//                 keepPrinting = false;
//                 monitorThread.join();
//             }
            
//             RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs", 
//                        x, y, z, msg1->time);
            
//             controller->setMotorToPositionMode();
//             currentMode = ControlMode::POSITION;
//             controller->runMotors(x, y, z, msg1->time);
            
//             keepPrinting = true;
//             monitorThread = std::thread([x, y, z]() {
//                 positionMonitoringThread(x, y, z);
//             });
            
//         } else if (msg1->command_type == "speed") {
//             // 速度模式处理
//             if (monitorThread.joinable()) {
//                 keepPrinting = false;
//                 monitorThread.join();
//             }
            
//             RCLCPP_INFO(this->get_logger(), "设置速度: X=%dmm/s, Y=%dmm/s, Z=%dmm/s", 
//                        msg1->vx, msg1->vy, msg1->vz);
            
//             controller->setMotorToSpeedMode();
//             currentMode = ControlMode::SPEED;
//             controller->runMotorsSpeed(msg1->vx, msg1->vy, msg1->vz);
            
//             keepPrinting = true;
//             monitorThread = std::thread([msg1]() {
//                 speedMonitoringThread(msg1->vx, msg1->vy, msg1->vz);
//             });
            
//         } else if (msg1->command_type == "stop") {
//             // 停止命令处理
//             RCLCPP_INFO(this->get_logger(), "收到停止命令");
//             controller->runMotorsSpeed(0, 0, 0);
//             keepPrinting = false;
//             targetReached = true;
            
//         } else if (msg1->command_type == "exit") {
//             // 退出命令处理
//             RCLCPP_INFO(this->get_logger(), "收到退出命令");
//             exitProgram = true;
//             rclcpp::shutdown();
            
//         } else {
//             RCLCPP_WARN(this->get_logger(), "未知命令类型: %s", msg1->command_type.c_str());
//         }
//     }

//     void publishCurrentPosition(bool reached = true) {
//         Position pos = controller->getCurrentPosition();
//         auto msg = base_interfaces_demo::msg::MotorStatus();
//         msg.x = pos.x;
//         msg.y = pos.y;
//         msg.z = pos.z;
//         msg.reached_target = reached;
//         position_publisher_->publish(msg);
        
//         RCLCPP_INFO(this->get_logger(), 
//             "已发布位置: X=%.2f, Y=%.2f, Z=%.2f 状态: %s", 
//             pos.x, pos.y, pos.z, reached ? "已到达" : "运动中");
//     }

//     rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr subscription_;
//     rclcpp::Publisher<base_interfaces_demo::msg::MotorStatus>::SharedPtr position_publisher_;
// };

// int main(int argc, char * argv[])
// {
//     rclcpp::init(argc, argv);
//     auto node = std::make_shared<MotorControllerNode>();
//     rclcpp::spin(node);
//     rclcpp::shutdown();
//     return 0;
// }




#include <memory>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "base_interfaces_demo/msg/motor_command.hpp"
#include "base_interfaces_demo/msg/motor_status.hpp"
#include "motor_httx_aut_pos/MotorController.h"

using namespace std::chrono_literals;

// 电机参数配置
const MotorInfo X_MOTOR = {41932, 9119059, 2.0, 181.05228};
const MotorInfo Y_MOTOR = {62231, 9113638, 2.0, 181.05228};
const MotorInfo Z_MOTOR = {60700, 2681700, 5.0, 181.05228};


// 全局控制变量
std::atomic<bool> keepPrinting(false);
std::atomic<bool> exitProgram(false);
std::atomic<bool> targetReached(false);
std::mutex cout_mutex;
std::mutex pos_mutex;

// 行程范围配置
constexpr double x_min = 1e-6, x_max = 278.0;
constexpr double y_min = 1e-6, y_max = 277.0;
constexpr double z_min = 1e-6, z_max = 200.0;

// 运行模式枚举
enum class ControlMode {
    POSITION,
    SPEED
};

// 电机控制器全局实例
std::shared_ptr<MotorController> controller;
ControlMode currentMode = ControlMode::POSITION;
std::thread monitorThread;

class MotorControllerNode : public rclcpp::Node
{
public:
    MotorControllerNode() : Node("motor_controller_node1")
    {
        // 创建命令订阅者
        command_subscriber_ = this->create_subscription<base_interfaces_demo::msg::MotorCommand>(
            "huatai3_pos_spe_pd", 10, 
            [this](const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
                this->command_callback(msg);
            });

        // 创建状态发布者
        status_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorStatus>(
            "huatai3_pos_spe_p", 10);

        // 初始化电机控制器
        controller = std::make_shared<MotorController>("can2", X_MOTOR, Y_MOTOR, Z_MOTOR);
        controller->setupCanInterface("can2");
        controller->enableMotors();
        controller->startReadingPosition();
        controller->requestMotorSpeed();
        
        // 启动状态发布定时器
        status_timer_ = this->create_wall_timer(
            100ms, [this]() { this->publish_status(); });
        
        RCLCPP_INFO(this->get_logger(), "三轴电机控制器已启动");
    }

    ~MotorControllerNode() {
        safeShutdown();
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }

private:
    void command_callback(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg)
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        RCLCPP_INFO(this->get_logger(), "收到命令: 类型=%s", msg->command_type.c_str());
        
        targetReached = false;
        
        if (msg->command_type == "position") {
            handlePositionCommand(msg);
        } else if (msg->command_type == "speed") {
            handleSpeedCommand(msg);
        } else if (msg->command_type == "stop") {
            handleStopCommand();
        } else if (msg->command_type == "exit") {
            handleExitCommand();
        } else {
            RCLCPP_WARN(this->get_logger(), "未知命令类型: %s", msg->command_type.c_str());
        }
    }

    void handlePositionCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
        double x = msg->x;
        double y = msg->y;
        double z = msg->z;
        
        if (msg->is_relative) {
            Position current = controller->getCurrentPosition();
            x += current.x;
            y += current.y;
            z += current.z;
        }
        
        if (!checkPositionInRange(x, y, z)) {
            RCLCPP_ERROR(this->get_logger(), "目标位置超出允许范围！");
            return;
        }
        
        if (monitorThread.joinable()) {
            keepPrinting = false;
            monitorThread.join();
        }
        
        RCLCPP_INFO(this->get_logger(), "正在移动到 X=%.2fmm, Y=%.2fmm, Z=%.2fmm, 时间=%.2fs", 
                   x, y, z, msg->time);
        
        controller->setMotorToPositionMode();
        currentMode = ControlMode::POSITION;
        controller->runMotors(x, y, z, msg->time);
        
        keepPrinting = true;
        monitorThread = std::thread([this, x, y, z]() {
            positionMonitoringThread(x, y, z);
        });
    }

    void handleSpeedCommand(const base_interfaces_demo::msg::MotorCommand::SharedPtr msg) {
        if (monitorThread.joinable()) {
            keepPrinting = false;
            monitorThread.join();
        }
        
        RCLCPP_INFO(this->get_logger(), "设置速度: X=%fmm/s, Y=%fmm/s, Z=%fmm/s", 
                   msg->vx, msg->vy, msg->vz);
        
        controller->setMotorToSpeedMode();
        currentMode = ControlMode::SPEED;
        controller->runMotorsSpeed(msg->vx, msg->vy, msg->vz);
        
        keepPrinting = true;
        monitorThread = std::thread([this, msg]() {
            speedMonitoringThread(msg->vx, msg->vy, msg->vz);
        });
    }

    void handleStopCommand() {
        RCLCPP_INFO(this->get_logger(), "收到停止命令");
        controller->runMotorsSpeed(0, 0, 0);
        controller->powerOffMotors();
        controller->enableMotors();
        keepPrinting = false;
        targetReached = true;
        publish_status(true); // 发布最终状态
    }

    void handleExitCommand() {
        RCLCPP_INFO(this->get_logger(), "收到退出命令");
        exitProgram = true;
        rclcpp::shutdown();
    }

    void positionMonitoringThread(double /*target_x*/, double /*target_y*/, double/* target_z*/) {
        const double threshold = 0.01;
        bool position_stable = false;
        
        Position last_pos = controller->getCurrentPosition();
        
        while (keepPrinting && !exitProgram && !position_stable) {
            Position current_pos = controller->getCurrentPosition();
            
            double dx = std::abs(current_pos.x - last_pos.x);
            double dy = std::abs(current_pos.y - last_pos.y);
            double dz = std::abs(current_pos.z - last_pos.z);
            
            if (dx < threshold && dy < threshold && dz < threshold) {
                position_stable = true;
                targetReached = true;
                RCLCPP_INFO(this->get_logger(), "已到达目标位置");
            }
            
            last_pos = current_pos;
            std::this_thread::sleep_for(100ms);
        }
    }

    void speedMonitoringThread(int32_t vx, int32_t vy, int32_t vz) {
        while (keepPrinting && !exitProgram) {
            Position current_pos = controller->getCurrentPosition();

            if (isAtLimitPosition(current_pos, vx, vy, vz)) {
                controller->runMotorsSpeed(0, 0, 0);
                targetReached = true;
                RCLCPP_INFO(this->get_logger(), "已到达极限位置，电机已停止");
                break;
            }

            std::this_thread::sleep_for(100ms);
        }
    }

    // void publish_status(bool reached = false) {
    //     auto msg = base_interfaces_demo::msg::MotorStatus();
    //     Position pos = controller->getCurrentPosition();
    //     msg.x = pos.x;
    //     msg.y = pos.y;
    //     msg.z = pos.z;
    //     msg.reached_target = targetReached;
    //     status_publisher_->publish(msg);
    // }
    void publish_status(bool reached = false) {
        auto msg = base_interfaces_demo::msg::MotorStatus();
        Position pos = controller->getCurrentPosition();
        {
            std::lock_guard<std::mutex> lock(pos_mutex); // 确保线程安全
            msg.x = pos.x;
            msg.y = pos.y;
            msg.z = pos.z;
        }

        double vx, vy, vz;
        {
            std::lock_guard<std::mutex> lock(speed_mutex); // 确保线程安全
            vx = controller->getCurrentSpeedX();  // 使用getter方法
            vy = controller->getCurrentSpeedY();
            vz = controller->getCurrentSpeedZ();
        }

        msg.vx = vx;
        msg.vy = vy;
        msg.vz = vz;
        msg.reached_target = targetReached;
        status_publisher_->publish(msg);
        controller->requestMotorSpeed();

    }

    // 添加速度互斥锁
    std::mutex speed_mutex;



    bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
        return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
               ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
               ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
    }

    void safeShutdown() {
        exitProgram = true;
        keepPrinting = false;
        controller->stopReadingPosition();
        controller->runMotorsSpeed(0, 0, 0);
        controller->powerOffMotors();

        RCLCPP_INFO(this->get_logger(), "系统已安全关闭");
    }

    bool checkPositionInRange(double x, double y, double z) {
        return (x >= x_min && x <= x_max && 
                y >= y_min && y <= y_max && 
                z >= z_min && z <= z_max);
    }

    rclcpp::Subscription<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_subscriber_;
    rclcpp::Publisher<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_publisher_;
    rclcpp::TimerBase::SharedPtr status_timer_;
    MotorSpeedStatus currentSpeedStatus_;
};

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<MotorControllerNode>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
