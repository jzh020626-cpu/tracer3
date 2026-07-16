#include <sstream>
#include "motor_httx_pos_spe/MotorController.h"
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

// ================= 位置模式相关函数 =================

bool parsePositionInput(const std::string& input, double& x, double& y, double& z, double& time, bool& relative) {
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    size_t relPos = lowerInput.find("rel");
    relative = (relPos != std::string::npos);
    
    std::string numbersPart = input.substr(0, relPos);
    std::istringstream iss(numbersPart);
    
    if (!(iss >> x >> y >> z >> time)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "输入格式错误！请按 X(mm) Y(mm) Z(mm) Time(s) [rel] 格式输入" << std::endl;
        std::cerr << "绝对运动示例: 50.0 30.0 20.0 5.0" << std::endl;
        std::cerr << "相对运动示例: 10.0 5.0 0.0 2.0 rel" << std::endl;
        return false;
    }
    
    return true;
}

void positionMonitoringThread(MotorController& controller, double target_x, double target_y, double target_z) {
    const double threshold = 0.01;
    bool position_stable = false;
    
    Position last_pos;
    {
        std::lock_guard<std::mutex> lock(pos_mutex);
        last_pos = controller.getCurrentPosition();
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (keepPrinting && !exitProgram && !position_stable) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        
        if (elapsed >= 100) {
            Position current_pos;
            {
                std::lock_guard<std::mutex> lock(pos_mutex);
                current_pos = controller.getCurrentPosition();
            }
            
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                // std::cout << "\r";
                // std::cout << std::fixed << std::setprecision(2);
                // std::cout << "当前位置: X=" << current_pos.x << "mm, Y=" << current_pos.y 
                //           << "mm, Z=" << current_pos.z << "mm";
                // std::cout << " | 目标位置: X=" << target_x << " Y=" << target_y << " Z=" << target_z;
                // std::cout << std::flush;
            }

            double dx = std::abs(current_pos.x - last_pos.x);
            double dy = std::abs(current_pos.y - last_pos.y);
            double dz = std::abs(current_pos.z - last_pos.z);
            
            if (dx < threshold && dy < threshold && dz < threshold) {
                position_stable = true;
                targetReached = true;
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "\n已到达目标位置" << std::endl;
                    std::cout << "最终坐标: X=" << current_pos.x << "mm, Y=" << current_pos.y 
                              << "mm, Z=" << current_pos.z << "mm" << std::endl;
                }
            }
            
            last_pos = current_pos;
            start_time = now;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// ================= 速度模式相关函数 =================

bool parseSpeedInput(const std::string& input, int32_t& vx, int32_t& vy, int32_t& vz) {
    std::istringstream iss(input);
    
    if (!(iss >> vx >> vy >> vz)) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "输入格式错误！请按 vx(mm/s) vy(mm/s) vz(mm/s) 格式输入" << std::endl;
        std::cerr << "速度示例: 50.0 30.0 20.0" << std::endl;
        return false;
    }
    
    std::string remaining;
    if (iss >> remaining) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "警告: 检测到多余输入 '" << remaining << "'，将只使用前三个速度值" << std::endl;
    }
    
    return true;
}

bool isAtLimitPosition(const Position& pos, int32_t vx, int32_t vy, int32_t vz) {
    return ((vx > 0 && pos.x >= x_max) || (vx < 0 && pos.x <= x_min)) ||
           ((vy > 0 && pos.y >= y_max) || (vy < 0 && pos.y <= y_min)) ||
           ((vz > 0 && pos.z >= z_max) || (vz < 0 && pos.z <= z_min));
}

void speedMonitoringThread(MotorController& controller, int32_t vx, int32_t vy, int32_t vz) {
    while (keepPrinting && !exitProgram) {
        Position current_pos;
        {
            std::lock_guard<std::mutex> lock(pos_mutex);
            current_pos = controller.getCurrentPosition();
        }
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "\r";
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "实时坐标: X=" << current_pos.x << "mm, Y=" << current_pos.y 
                      << "mm, Z=" << current_pos.z << "mm";
            std::cout << " | 运动方向: X=" << (vx > 0 ? "+" : "-") << std::abs(vx)
                      << " Y=" << (vy > 0 ? "+" : "-") << std::abs(vy)
                      << " Z=" << (vz > 0 ? "+" : "-") << std::abs(vz);
            std::cout << std::flush;
        }

        if (isAtLimitPosition(current_pos, vx, vy, vz)) {
            controller.runMotorsSpeed(0, 0, 0);
            targetReached = true;
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "\n已到达极限位置，电机已停止" << std::endl;
            }
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// ================= 通用函数 =================

void safeShutdown(MotorController& controller) {
    exitProgram = true;
    keepPrinting = false;
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "\n正在停止监控线程..." << std::endl;
    }
    
    controller.stopReadingPosition();
    controller.runMotorsSpeed(0, 0, 0);
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "正在关闭电机电源..." << std::endl;
    }
    
    controller.powerOffMotors();
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "正在关闭CAN接口..." << std::endl;
    }
    
    controller.shutdownCanInterface("can0");
    
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "系统已安全关闭" << std::endl;
    }
}

bool checkPositionInRange(double x, double y, double z) {
    return (x >= x_min && x <= x_max && 
            y >= y_min && y <= y_max && 
            z >= z_min && z <= z_max);
}

// ================= 主函数 =================

int main() {
    try {
        MotorController controller("can0", X_MOTOR, Y_MOTOR, Z_MOTOR);
        controller.setupCanInterface("can0");
        controller.enableMotors();
        controller.startReadingPosition();
        
        Position init_pos;
        {
            std::lock_guard<std::mutex> lock(pos_mutex);
            init_pos = controller.getCurrentPosition();
        }
        
        {
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "三轴电机控制器已启动" << std::endl;
            std::cout << "当前位置: X=" << init_pos.x << "mm, Y=" << init_pos.y 
                      << "mm, Z=" << init_pos.z << "mm" << std::endl;
            std::cout << "请选择控制模式:" << std::endl;
            std::cout << "1. 位置模式 (输入 'position' 进入)" << std::endl;
            std::cout << "   - 绝对运动格式: X(mm) Y(mm) Z(mm) Time(s)" << std::endl;
            std::cout << "   - 相对运动格式: X(mm) Y(mm) Z(mm) Time(s) rel" << std::endl;
            std::cout << "2. 速度模式 (输入 'speed' 进入)" << std::endl;
            std::cout << "   - 输入格式: vx(mm/s) vy(mm/s) vz(mm/s)" << std::endl;
            std::cout << "特殊命令: 'exit'退出程序" << std::endl;
        }

        std::thread monitorThread;
        std::string input;
        ControlMode currentMode = ControlMode::POSITION;
        controller.setMotorToPositionMode();
        
        // 清除所有运动参数
        auto resetMotionParameters = [&]() {
            if (monitorThread.joinable()) {
                keepPrinting = false;
                monitorThread.join();
            }
            // controller.runMotorsSpeed(0, 0, 0);
            for (int i = 0; i < 3; ++i) {
                controller.runMotorsSpeed(0, 0, 0);
                //----------------------------------------------------
                controller.runMotors(0, 0, 0, 0.1);
                //----------------------------------------------------
                usleep(5000);
            }
            targetReached = true;
            
            // 获取当前位置作为新起点
            Position current_pos;
            {
                std::lock_guard<std::mutex> lock(pos_mutex);
                current_pos = controller.getCurrentPosition();
            }
            
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "已清除所有运动参数" << std::endl;
                std::cout << "新起点位置: X=" << current_pos.x << "mm, Y=" 
                          << current_pos.y << "mm, Z=" << current_pos.z << "mm" << std::endl;
                
            }
        };
        
        while (!exitProgram) {
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                if (!targetReached) {
                    std::cout << "\n请输入指令: " << std::flush;
                }
            }
            
            std::getline(std::cin, input);
            
            if (input == "exit") {
                safeShutdown(controller);
                if (monitorThread.joinable()) {
                    monitorThread.join();
                }
                break;
            }
            else if (input == "position") {     
                resetMotionParameters();
                controller.stopReadingPosition();
                controller.powerOffMotors();
                controller.enableMotors();
                currentMode = ControlMode::POSITION;
                controller.setMotorToPositionMode();
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "已切换到位置模式" << std::endl;
                }
                continue;
            }
            else if (input == "speed") {

                // resetMotionParameters();
                // currentMode = ControlMode::SPEED;
                // controller.setMotorToSpeedMode();
                // 1. 先停止所有运动
                controller.runMotorsSpeed(0, 0, 0);
                // 2. 再切换模式
                currentMode = ControlMode::SPEED;
                controller.setMotorToSpeedMode();
                // 3. 再次确认停止
                controller.runMotorsSpeed(0, 0, 0);
                // 4. 重置状态
                resetMotionParameters();

                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "已切换到速度模式" << std::endl;
                }
                continue;
            }else if (input == "stop") {     
                // controller.stopReadingPosition();
                controller.runMotorsSpeed(0, 0, 0);
                controller.powerOffMotors();
                controller.enableMotors();
                
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "运动暂停" << std::endl;
                continue;
            }
            
            targetReached = false;
            
            if (currentMode == ControlMode::POSITION) {
                double x = 0, y = 0, z = 0, time = 0;
                bool relative = false;
                
                if (!parsePositionInput(input, x, y, z, time, relative)) {
                    continue;
                }
                
                if (time <= 0) {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cerr << "错误: 时间必须为正数！" << std::endl;
                    continue;
                }
                
                if (relative) {
                    Position current;
                    {
                        std::lock_guard<std::mutex> lock(pos_mutex);
                        current = controller.getCurrentPosition();
                    }
                    x += current.x;
                    y += current.y;
                    z += current.z;
                    
                    if(fabs(x) < 1e-4) x = (x >= 0) ? 1e-3 : -1e-3;
                    if(fabs(y) < 1e-4) y = (y >= 0) ? 1e-3 : -1e-3;
                    if(fabs(z) < 1e-4) z = (z >= 0) ? 1e-3 : -1e-3;
                }
                
                if (!checkPositionInRange(x, y, z)) {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cerr << "错误: 目标位置超出允许范围！" << std::endl;
                    std::cerr << "允许范围: X[" << x_min << "," << x_max << "] Y[" 
                              << y_min << "," << y_max << "] Z[" << z_min << "," << z_max << "]" << std::endl;
                    continue;
                }
                
                if (monitorThread.joinable()) {
                    keepPrinting = false;
                    monitorThread.join();
                }
                
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "正在移动到 X=" << x << "mm, Y=" << y << "mm, Z=" << z 
                              << "mm, 时间=" << time << "s" << std::endl;
                    if (relative) {
                        std::cout << "(相对运动模式)" << std::endl;
                    }
                }


                controller.runMotors(x, y, z, time);
                
                keepPrinting = true;
                monitorThread = std::thread([&controller, x, y, z]() {
                    positionMonitoringThread(controller, x, y, z);
                });
            }
            else { // SPEED模式
                int32_t vx = 0, vy = 0, vz = 0;
                
                if (!parseSpeedInput(input, vx, vy, vz)) {
                    continue;
                }
                
                if (monitorThread.joinable()) {
                    keepPrinting = false;
                    monitorThread.join();
                }
                
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "设置速度: X=" << vx << "mm/s, Y=" << vy 
                              << "mm/s, Z=" << vz << "mm/s" << std::endl;
                }
                
                // 只有在有速度输入时才启动电机
                if (vx != 0 || vy != 0 || vz != 0) {
                    controller.runMotorsSpeed(vx, vy, vz);
                    
                    keepPrinting = true;
                    monitorThread = std::thread([&controller, vx, vy, vz]() {
                        speedMonitoringThread(controller, vx, vy, vz);
                    });
                } else {
                    // 速度为0时停止电机
                    controller.runMotorsSpeed(0, 0, 0);
                    {
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cout << "速度为零，电机已停止" << std::endl;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cerr << "发生错误: " << e.what() << std::endl;
        return 1;
    }
  
    return 0;
}




