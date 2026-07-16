#include "motor_httx_aut_spe/MotorController.h"
#include <sstream>
MotorController::MotorController(const std::string& canInterface, 
                const MotorInfo& motorXInfo, 
                const MotorInfo& motorYInfo, 
                const MotorInfo& motorZInfo)
    : socketFd(-1),                     // 初始化 CAN 通信套接字为 -1
    currentX(0.0), currentY(0.0), currentZ(0.0), // 初始化电机当前位置为 0
    currentSpeedX(0.0), currentSpeedY(0.0), currentSpeedZ(0.0), // 初始化速度
    stopFlag(false),                  // 位置读取线程的停止标志，初始为 false
    canInterface(canInterface),       // 保存 CAN 接口名称
    motorX(motorXInfo),               // 初始化 X 轴电机配置
    motorY(motorYInfo),               // 初始化 Y 轴电机配置
    motorZ(motorZInfo) {              // 初始化 Z 轴电机配置
    // startSocketCAN(canInterface);       // 启动 CAN 通信接口
}

MotorController::~MotorController() {
    stopReadingPosition();
    stopSocketCAN();
}

void MotorController::setupCanInterface(const std::string& canInterface) {
    // 关闭之前的 CAN 接口（如果有）
    std::string commandDown = "sudo ip link set " + canInterface + " down";
    int retDown = system(commandDown.c_str());
    if (retDown == 0) {
        std::cout << "CAN 接口 " << canInterface << " 已关闭。" << std::endl;
    } else {
        std::cerr << "关闭 CAN 接口 " << canInterface << " 失败。" << std::endl;
    }

    // 配置并重新开启 CAN 接口
    std::string commandUp = "sudo ip link set " + canInterface + " up type can bitrate 500000";
    int retUp = system(commandUp.c_str());
    if (retUp == 0) {
        std::cout << "CAN 接口 " << canInterface << " 已开启。" << std::endl;

        // 初始化 SocketCAN
        startSocketCAN(canInterface);
        if (socketFd >= 0) {
            std::cout << "SocketCAN 初始化成功，接口: " << canInterface << std::endl;
        } else {
            std::cerr << "SocketCAN 初始化失败，请检查。" << std::endl;
        }
    } else {
        std::cerr << "CAN 接口 " << canInterface << " 开启失败。" << std::endl;
    }
}

void MotorController::shutdownCanInterface(const std::string& canInterface) {
    // 关闭 SocketCAN
    stopSocketCAN();
    
    // 使用系统命令关闭 CAN 接口
    std::string command = "sudo ip link set " + canInterface + " down";
    int ret = system(command.c_str());
    if (ret == 0) {
        std::cout << "CAN 接口 " << canInterface << " 已关闭。" << std::endl;
    } else {
        std::cerr << "CAN 接口 " << canInterface << " 关闭失败。" << std::endl;
    }
}

void MotorController::enableMotors() {
    sendCommand("000#0100"); // 开启站点上报
    std::cout << "\n开启全部站点上报。" << std::endl;

    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#2B4060000F000000";
        sendCommand(command);
        std::cout << "电机 " << motorId << " 使能。" << std::endl;
    }
}

void MotorController::powerOffMotors() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#2B40600006000000";
        sendCommand(command);
        std::cout << "电机 " << motorId << " 下电。" << std::endl;
    }

    sendCommand("000#0200"); // 关闭站点上报
    std::cout << "关闭全部站点上报。\n" << std::endl;
}


void MotorController::setMotorToPositionMode() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#2F60600001000000";
        sendCommand(command);
        std::cout << "电机 " << motorId << " 设置为位置模式。" << std::endl;
    }
    std::cout << "位置模式打开\n" << std::endl;
}

// void MotorController::setMotorToSpeedMode() {
//     std::vector<std::string> motorIds = {"601", "602", "603"};
//     for (const auto& motorId : motorIds) {
//         std::string command = motorId + "#2F60600003000000";
//         sendCommand(command);
//         std::cout << "电机 " << motorId << " 设置为速度模式。" << std::endl;
//     }
//     std::cout << "速度模式打开\n" << std::endl;

// }
void MotorController::setMotorToSpeedMode() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    
    // 1. 先停止所有电机
    for (const auto& motorId : motorIds) {
        std::string stopCommand = motorId + "#2B40600000000000";
        sendCommand(stopCommand);
    }
    usleep(50000); // 等待50ms确保停止指令生效 (移除单引号)

    // 2. 设置速度模式参数
    for (const auto& motorId : motorIds) {
        // 设置控制字为"准备切换"(0x06)
        std::string prepCommand = motorId + "#2B40600006000000";
        sendCommand(prepCommand);
        
        // 设置目标速度为零 (0x60FF)
        std::string zeroSpeed = motorId + "#2360FF0000000000";
        sendCommand(zeroSpeed);
        
        // 设置模式为速度模式(3)
        std::string modeCommand = motorId + "#2F60600003000000";
        sendCommand(modeCommand);
        
        // 设置控制字为"启动"(0x0F)
        std::string startCommand = motorId + "#2B4060000F000000";
        sendCommand(startCommand);
        
        std::cout << "电机 " << motorId << " 安全切换到速度模式" << std::endl;
    }
    
    // 3. 再次确认速度为零
    usleep(100000); // 等待100ms (移除单引号)
    for (const auto& motorId : motorIds) {
        std::string verifyCommand = motorId + "#2360FF0000000000";
        sendCommand(verifyCommand);
    }
    
    std::cout << "所有电机已安全进入速度模式(速度=0)\n" << std::endl;
}

void MotorController::runMotorsSynchronized(double xTravel, double yTravel, double zTravel, double totalTime) {
    Position currentPosition;

    // 获取当前位置
    {
        std::lock_guard<std::mutex> lock(positionMutex);
        currentPosition = {currentX, currentY, currentZ};
    }

     // 打印当前位置
    std::cout << std::fixed << std::setprecision(2); // 保留两位小数
    std::cout << "当前滑台位置: X=" << currentPosition.x 
              << ", Y=" << currentPosition.y 
              << ", Z=" << currentPosition.z << std::endl;

    // 计算各轴的位移需求
    double xDistanceNeeded = std::abs(xTravel - currentPosition.x);
    double yDistanceNeeded = std::abs(yTravel - currentPosition.y);
    double zDistanceNeeded = std::abs(zTravel - currentPosition.z);

    // 计算各轴的速度需求以匹配总时间
    double vx = xDistanceNeeded / totalTime;
    double vy = yDistanceNeeded / totalTime;
    double vz = zDistanceNeeded / totalTime;

    // 将速度转换为 RPM
    int rpmX = static_cast<int>(vx / motorX.travelPerTurn * 60.0);
    int rpmY = static_cast<int>(vy / motorY.travelPerTurn * 60.0);
    int rpmZ = static_cast<int>(vz / motorZ.travelPerTurn * 60.0);

    // 转换目标位置为增量脉冲
    uint32_t xInc = travelToInc(motorX, xTravel);
    uint32_t yInc = travelToInc(motorY, yTravel);
    uint32_t zInc = travelToInc(motorZ, zTravel);

    // 范围检查：限制 xInc, yInc, zInc 在 [startInc, endInc] 范围内
    if (xInc < motorX.startInc) xInc = motorX.startInc;
    if (xInc > motorX.endInc) xInc = motorX.endInc;

    if (yInc < motorY.startInc) yInc = motorY.startInc;
    if (yInc > motorY.endInc) yInc = motorY.endInc;

    if (zInc < motorZ.startInc) zInc = motorZ.startInc;
    if (zInc > motorZ.endInc) zInc = motorZ.endInc;

    // 发送位置命令
    if (xDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x601, decimalToCANCommand(xInc)));
    }
    if (yDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x602, decimalToCANCommand(yInc)));
    }
    if (zDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x603, decimalToCANCommand(zInc)));
    }

    // 发送速度命令
    sendRPMs(rpmX, rpmY, rpmZ);

    // 发送特殊命令开始运动
    if (xDistanceNeeded > 1e-3) sendSpecialCommands(0x601);
    if (yDistanceNeeded > 1e-3) sendSpecialCommands(0x602);
    if (zDistanceNeeded > 1e-3) sendSpecialCommands(0x603);
}

void MotorController::runMotors(double xTravel, double yTravel, double zTravel, double totalTime) {
    Position currentPosition;

    // 获取当前位置，使用互斥锁保护共享数据
    {
        std::lock_guard<std::mutex> lock(positionMutex);
        currentPosition = {currentX, currentY, currentZ};
    }

    // 打印当前位置
    // std::cout << "当前位置: X=" << currentPosition.x 
    //         << ", Y=" << currentPosition.y 
    //         << ", Z=" << currentPosition.z << std::endl;

    // 计算各轴所需的行程
    double xDistanceNeeded = std::abs(xTravel - currentPosition.x);
    double yDistanceNeeded = std::abs(yTravel - currentPosition.y);
    double zDistanceNeeded = std::abs(zTravel - currentPosition.z);

    // 计算各轴所需速度 (mm/s)
    double vx = xDistanceNeeded / totalTime;
    double vy = yDistanceNeeded / totalTime;
    double vz = zDistanceNeeded / totalTime;

    // 将速度转换为 RPM
    int rpmX = static_cast<int>(vx / motorX.travelPerTurn * 60.0);
    int rpmY = static_cast<int>(vy / motorY.travelPerTurn * 60.0);
    int rpmZ = static_cast<int>(vz / motorZ.travelPerTurn * 60.0);

    bool xMove = xDistanceNeeded > 1e-3;
    bool yMove = yDistanceNeeded > 1e-3;
    bool zMove = zDistanceNeeded > 1e-3;

    if (!xMove && !yMove && !zMove) {
        std::cout << "所有轴已在目标位置，无需运动。" << std::endl;
        return;
    }

    // 转换目标位置为增量脉冲
    uint32_t xInc = travelToInc(motorX, xTravel);
    uint32_t yInc = travelToInc(motorY, yTravel);
    uint32_t zInc = travelToInc(motorZ, zTravel);

    // 范围检查：限制 xInc, yInc, zInc 在 [startInc, endInc] 范围内
    if (xInc < motorX.startInc) xInc = motorX.startInc;
    if (xInc > motorX.endInc) xInc = motorX.endInc;

    if (yInc < motorY.startInc) yInc = motorY.startInc;
    if (yInc > motorY.endInc) yInc = motorY.endInc;

    if (zInc < motorZ.startInc) zInc = motorZ.startInc;
    if (zInc > motorZ.endInc) zInc = motorZ.endInc;

    // double timeX = xMove ? xDistanceNeeded / (rpmX * motorX.travelPerTurn) : 0.0;
    // double timeY = yMove ? yDistanceNeeded / (rpmX * motorY.travelPerTurn) : 0.0;
    // double timeZ = zMove ? zDistanceNeeded / (rpmX * motorZ.travelPerTurn) : 0.0;

    // double motionTime = std::max(std::max(timeX, timeY), timeZ);

    // int rpmXFinal = xMove ? static_cast<int>(xDistanceNeeded / (motionTime * motorX.travelPerTurn)) : 0;
    // int rpmYFinal = yMove ? static_cast<int>(yDistanceNeeded / (motionTime * motorY.travelPerTurn)) : 0;
    // int rpmZFinal = zMove ? static_cast<int>(zDistanceNeeded / (motionTime * motorZ.travelPerTurn)) : 0;

    if (xMove) {
        auto command = decimalToCANCommand(xInc);
        std::string commandStr = frameToCommandString(0x601, command);
        std::cout << "X轴发送的 CAN 命令字符串: " << commandStr << std::endl; // 打印 X 轴的命令字符串
        sendCommand(commandStr);
    }
    if (yMove) {
        auto command = decimalToCANCommand(yInc);
        std::string commandStr = frameToCommandString(0x602, command);
        std::cout << "Y轴发送的 CAN 命令字符串: " << commandStr << std::endl; // 打印 Y 轴的命令字符串
        sendCommand(commandStr);
    }
    if (zMove) {
        auto command = decimalToCANCommand(zInc);
        std::string commandStr = frameToCommandString(0x603, command);
        std::cout << "Z轴发送的 CAN 命令字符串: " << commandStr << std::endl; // 打印 Z 轴的命令字符串
        sendCommand(commandStr);
    }


    sendRPMs(rpmX, rpmY, rpmZ);

    if (xMove) sendSpecialCommands(0x601);
    if (yMove) sendSpecialCommands(0x602);
    if (zMove) sendSpecialCommands(0x603);
}

void MotorController::runMotorsSpeed(int32_t vx, int32_t vy, int32_t vz) {
 
    // 将速度(mm/s)转换为 RPM 
    int rpmX = static_cast<int>(vx / motorX.travelPerTurn * 60.0);
    int rpmY = static_cast<int>(vy / motorY.travelPerTurn * 60.0);
    int rpmZ = static_cast<int>(vz / motorZ.travelPerTurn * 60.0);

    //判断最小速度
    // if(vx>1e-2||vx<-1e-2)
    // {
    //     bool xMove = true;
    // }
    // if(vy>1e-2||vy<-1e-2)
    // {
    //     bool yMove = true;
    // }
    // if(vz>1e-2||vz<-1e-2)
    // {
    //     bool zMove = true;
    // }
    bool xMove = (vx > 1e-2)||(vx<-1e-2);
    bool yMove = (vy > 1e-2)||(vy<-1e-2);
    bool zMove = (vz > 1e-2)||(vz<-1e-2);

   
    sendRPMsSpeed(rpmX, rpmY, rpmZ);

    if (xMove) sendSpecialCommands(0x601);
    if (yMove) sendSpecialCommands(0x602);
    if (zMove) sendSpecialCommands(0x603);

    if (!xMove && !yMove && !zMove) {
        std::cout << "所有轴速度为零，无需运动。" << std::endl;
        return;
    }

}


// void MotorController::startReadingPosition() {
//     // 如果线程已经启动，则直接返回
//     if (positionThread.joinable()) {
//         std::cerr << "读取线程已启动，无法重复启动！" << std::endl;
//         return;
//     }

//     // 检查 socketFd 是否有效
//     if (socketFd < 0) {
//         std::cerr << "SocketCAN 文件描述符无效，请确保 CAN 接口已正确打开！" << std::endl;
//         return;
//     }

//     stopFlag = false;

//     // // 设置打印间隔为 1.5 秒
//     // const auto printInterval = std::chrono::milliseconds(1500);

//     // 创建线程，用于读取并处理 CAN 数据
//     positionThread = std::thread([this]() {
//         while (!stopFlag) {
//             if (socketFd < 0) {
//                 std::cerr << "SocketCAN 文件描述符无效，无法读取。" << std::endl;
//                 break; // 退出线程
//             }

//             struct can_frame frame;
//             int nbytes = read(socketFd, &frame, sizeof(frame));
//             if (nbytes < 0) {
//                 if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                     // 非阻塞模式下，无数据可读，继续尝试
//                     std::this_thread::sleep_for(std::chrono::milliseconds(10));
//                     continue;
//                 } else {
//                     perror("SocketCAN 读取失败");
//                     break; // 遇到严重错误，退出线程
//                 }
//             }

//             // 提取增量脉冲数
//             uint32_t pulses = 0;
//             for (int i = 0; i < frame.can_dlc && i < 4; ++i) {
//                 pulses |= frame.data[i] << (8 * i);
//             }

//             // 锁住数据以确保线程安全
//             {
//                 std::lock_guard<std::mutex> lock(positionMutex);

//                 double* currentPosition = nullptr;
//                 std::chrono::steady_clock::time_point* lastPrint = nullptr;
//                 MotorInfo* motor = nullptr;

//                 if (frame.can_id == 0x181) { 
//                     currentPosition = &currentX;
//                     lastPrint = &lastPrintX;
//                     motor = &motorX;
//                 } else if (frame.can_id == 0x281) { 
//                     currentPosition = &currentY;
//                     lastPrint = &lastPrintY;
//                     motor = &motorY;
//                 } else if (frame.can_id == 0x381) { 
//                     currentPosition = &currentZ;
//                     lastPrint = &lastPrintZ;
//                     motor = &motorZ;
//                 } else {
//                     continue; // 未知的 CAN ID，跳过
//                 }

//                 if (motor) {
//                     *currentPosition = incToTravel(*motor, pulses);
//                     if (*currentPosition > 500) {
//                         *currentPosition = 0;
//                         // std::cerr << "轴 " << std::hex << frame.can_id
//                         //           << " 位置超出范围，已重置为 0 mm\n" << std::endl;
//                     }

//                     auto now = std::chrono::steady_clock::now();
//                     if (now - *lastPrint > refreshInterval) {
//                         *lastPrint = now;
//                         // std::cout << "轴 " << std::hex << frame.can_id
//                         //           << " 位置: " << std::fixed << std::setprecision(2)
//                         //           << *currentPosition << " mm" << std::endl;
//                     }
//                 }
//             }

//             // 适当休眠以避免 CPU 过载
//             std::this_thread::sleep_for(std::chrono::milliseconds(10));
//         }

//         std::cout << "读取线程已安全退出。" << std::endl;
//     });
// }




void MotorController::startReadingPosition() {
    // 如果线程已经启动，则直接返回
    if (positionThread.joinable()) {
        std::cerr << "读取线程已启动，无法重复启动！" << std::endl;
        return;
    }

    // 检查 socketFd 是否有效
    if (socketFd < 0) {
        std::cerr << "SocketCAN 文件描述符无效，请确保 CAN 接口已正确打开！" << std::endl;
        return;
    }

    stopFlag = false;

    // 创建线程，用于读取并处理 CAN 数据
    positionThread = std::thread(&MotorController::startReadingPositionThread, this);
}

void MotorController::startReadingPositionThread() {
    while (!stopFlag) {
        if (socketFd < 0) {
            std::cerr << "SocketCAN 文件描述符无效，无法读取。" << std::endl;
            break; // 退出线程
        }

        

        struct can_frame frame;
        int nbytes = read(socketFd, &frame, sizeof(frame));
        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式下，无数据可读，继续尝试
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                perror("SocketCAN 读取失败");
                break; // 遇到严重错误，退出线程
            }
        }

        // 提取增量脉冲数或速度数据
        uint32_t data = 0;
        for (int i = 0; i < frame.can_dlc && i < 4; ++i) {
            data |= frame.data[i] << (8 * i);
        }

        // 根据 CAN ID 判断是位置还是速度数据
        uint32_t motorId = 0;
        if (frame.can_id == 0x181) motorId = 0x601;
        else if (frame.can_id == 0x281) motorId = 0x602;
        else if (frame.can_id == 0x381) motorId = 0x603;
        else if (frame.can_id == 0x581) motorId = 0x601;
        else if (frame.can_id == 0x582) motorId = 0x602;
        else if (frame.can_id == 0x583) motorId = 0x603;
        else {
            continue; // 未知的 CAN ID，跳过
        }

        std::lock_guard<std::mutex> lock(positionMutex);

        if (frame.can_id >= 0x181 && frame.can_id <= 0x381) {
            // 处理位置数据
            double* currentPosition = nullptr;
            std::chrono::steady_clock::time_point* lastPrint = nullptr;
            MotorInfo* motor = nullptr;

            if (frame.can_id == 0x181) { 
                currentPosition = &currentX;
                lastPrint = &lastPrintX;
                motor = &motorX;
            } else if (frame.can_id == 0x281) { 
                currentPosition = &currentY;
                lastPrint = &lastPrintY;
                motor = &motorY;
            } else if (frame.can_id == 0x381) { 
                currentPosition = &currentZ;
                lastPrint = &lastPrintZ;
                motor = &motorZ;
            } else {
                continue; // 未知的 CAN ID，跳过
            }

            if (motor) {
                *currentPosition = incToTravel(*motor, data);
                if (*currentPosition > 500) {
                    *currentPosition = 0;
                    // std::cerr << "轴 " << std::hex << frame.can_id
                    //           << " 位置超出范围，已重置为 0 mm\n" << std::endl;
                }

                auto now = std::chrono::steady_clock::now();
                if (now - *lastPrint > refreshInterval) {
                    *lastPrint = now;
                    // std::cout << "轴 " << std::hex << frame.can_id
                    //           << " 位置: " << std::fixed << std::setprecision(2)
                    //           << *currentPosition << " mm" << std::endl;
                }
            }
        }
        else if (frame.can_id >= 0x581 && frame.can_id <= 0x583) {
            // 提取后4字节（速度数据）
            int32_t speed_raw = 0;
            speed_raw |= (frame.data[4] << 0);   // 最低字节
            speed_raw |= (frame.data[5] << 8);
            speed_raw |= (frame.data[6] << 16);
            speed_raw |= (frame.data[7] << 24);  // 最高字节（符号位）
        
            // 打印原始数据（调试用）
            std::cout << "CAN ID: " << std::hex << frame.can_id 
                      << ", 原始速度: " << std::dec << speed_raw << std::endl;
        
            // 计算 RPM（需根据电机协议调整比例因子）
            const double scale = 17920.0;  // 示例值，需确认！
            double rpm = static_cast<double>(speed_raw) / scale;
        
            // 计算线速度 (mm/s)
            double speedMmPerSec = 0.0;
            if (frame.can_id == 0x581) {
                speedMmPerSec = rpm * motorX.travelPerTurn / 60.0;
            } else if (frame.can_id == 0x582) {
                speedMmPerSec = rpm * motorY.travelPerTurn / 60.0;
            } else if (frame.can_id == 0x583) {
                speedMmPerSec = rpm * motorZ.travelPerTurn / 60.0;
            }
        
            // 更新速度变量
            std::lock_guard<std::mutex> lock(speedMutex);
            if (frame.can_id == 0x581) currentSpeedX = speedMmPerSec;
            else if (frame.can_id == 0x582) currentSpeedY = speedMmPerSec;
            else if (frame.can_id == 0x583) currentSpeedZ = speedMmPerSec;
        
            // 打印实时速度
            std::cout << "\r当前速度: X=" << currentSpeedX << "mm/s, "
                      << "Y=" << currentSpeedY << "mm/s, "
                      << "Z=" << currentSpeedZ << "mm/s" << std::flush;
        }
    }

    std::cout << "读取线程已安全退出。" << std::endl;
}


void MotorController::stopReadingPosition() {
    stopFlag = true; // 设置标志以停止线程
    if (positionThread.joinable()) {
        positionThread.detach(); // 等待线程退出
        std::cout << "读取线程已停止。\n" << std::endl;
    } else {
        std::cerr << "读取线程未运行，无法停止！" << std::endl;
    }
}

Position MotorController::getCurrentPosition() {
    std::lock_guard<std::mutex> lock(positionMutex); // 确保线程安全
    return {currentX, currentY, currentZ};
}

// void MotorController::getCurrentSpeed() {
//     stopFlag = true; // 设置标志以停止线程
//     if (positionThread.joinable()) {
//         positionThread.detach(); // 等待线程退出
//         std::cout << "读取线程已停止。\n" << std::endl;
//     } else {
//         std::cerr << "读取线程未运行，无法停止！" << std::endl;
//     }
// }

void MotorController::startSocketCAN(const std::string& canInterface) {
    // 如果套接字已经打开，先关闭
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
        std::cerr << "检测到已打开的 SocketCAN，已关闭旧连接。" << std::endl;
    }

    // 创建 SocketCAN 套接字
    socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socketFd < 0) {
        perror("SocketCAN 初始化失败");
        return;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, canInterface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    // 检查 CAN 接口是否被占用
    if (ioctl(socketFd, SIOCGIFINDEX, &ifr) < 0) {
        perror("SocketCAN 接口检查失败");
        close(socketFd);
        socketFd = -1;
        return;
    }

    struct sockaddr_can addr;
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // 绑定套接字到 CAN 接口
    if (bind(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("SocketCAN 绑定失败");
        close(socketFd);
        socketFd = -1;

        // 检测资源占用，尝试释放
        std::string command = "sudo ip link set " + canInterface + " down";
        std::cerr << "尝试释放 CAN 接口资源: " << command << std::endl;
        int systemResult = system(command.c_str());
        if (systemResult == 0) {
            std::cout << "CAN 接口 " << canInterface << " 已释放，请重试。" << std::endl;
        } else {
            std::cerr << "释放 CAN 接口失败，请手动检查。" << std::endl;
        }
        return;
    }

    // 设置非阻塞模式
    int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    } else {
        perror("设置非阻塞模式失败");
    }

    std::cout << "SocketCAN " << canInterface << " 已成功启动。" << std::endl;
}

void MotorController::stopSocketCAN() {
    // // 首先停止读取线程
    // stopFlag = true;
    // if (positionThread.joinable()) {
    //     positionThread.detach();
    // }
    
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
    }

    // positionThread = std::thread([this]() {
    //     try {
    //         while (!stopFlag) {
    //             // 线程逻辑...
    //         }
    //     } catch (const std::exception& e) {
    //         std::cerr << "线程中捕获异常: " << e.what() << std::endl;
    //     } catch (...) {
    //         std::cerr << "线程中捕获未知异常" << std::endl;
    //     }

    //     std::cout << "读取线程已安全退出。\n" << std::endl;
    // });


}

double MotorController::incToTravel(const MotorInfo& motor, uint32_t inc) {
    double degrees = static_cast<double>(inc - motor.startInc) / motor.incPerDegree;
    return (degrees / 360) * motor.travelPerTurn;
}

uint32_t MotorController::travelToInc(const MotorInfo& motor, double travel) {
    double totalInc = (travel / motor.travelPerTurn) * 360 * motor.incPerDegree;
    return motor.startInc + static_cast<uint32_t>(totalInc);
}

std::vector<uint8_t> MotorController::decimalToCANCommand(uint32_t position) {
    std::vector<uint8_t> canCommand(8);
    canCommand[0] = 0x23;                // 命令类型，表示写入 4 个字节（在 CANopen 中常用）
    canCommand[1] = 0x7A;                // 索引的高字节（CANopen 对象字典索引的一部分）
    canCommand[2] = 0x60;                // 索引的低字节
    canCommand[3] = 0x00;                // 子索引
    canCommand[4] = position & 0xFF;     // 取位置数据的最低字节
    canCommand[5] = (position >> 8) & 0xFF;  // 取位置数据的次低字节
    canCommand[6] = (position >> 16) & 0xFF; // 取位置数据的次高字节
    canCommand[7] = (position >> 24) & 0xFF; // 取位置数据的最高字节
    return canCommand;
}

std::vector<uint8_t> MotorController::rpmToCANCommand(int rpm) {
    std::vector<uint8_t> canCommand(8);
    uint32_t hexSpeed = rpm * 17920;

    canCommand[0] = 0x23;
    canCommand[1] = 0x81;
    canCommand[2] = 0x60;
    canCommand[3] = 0x00;
    canCommand[4] = hexSpeed & 0xFF;
    canCommand[5] = (hexSpeed >> 8) & 0xFF;
    canCommand[6] = (hexSpeed >> 16) & 0xFF;
    canCommand[7] = (hexSpeed >> 24) & 0xFF;

    return canCommand;
}

std::vector<uint8_t> MotorController::rpmToCANCommandSpeed(int rpm) {
    std::vector<uint8_t> canCommand(8);
    uint32_t hexSpeed = rpm * 17920;

    canCommand[0] = 0x23;
    canCommand[1] = 0xFF;
    canCommand[2] = 0x60;
    canCommand[3] = 0x00;
    canCommand[4] = hexSpeed & 0xFF;
    canCommand[5] = (hexSpeed >> 8) & 0xFF;
    canCommand[6] = (hexSpeed >> 16) & 0xFF;
    canCommand[7] = (hexSpeed >> 24) & 0xFF;

    return canCommand;
}

void MotorController::sendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(sendMutex);
    std::string fullCommand = "cansend " + canInterface + " " + command;
    int ret = system(fullCommand.c_str());
    if (ret != 0) {
        std::cerr << "发送命令失败: " << fullCommand << std::endl;
    }
    // // 延时，控制发送频率
    // std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

std::string MotorController::frameToCommandString(uint32_t canId, const std::vector<uint8_t>& data) {
    std::ostringstream oss;

    // 生成 CAN ID 部分
    oss << std::hex << std::setw(3) << std::setfill('0') << canId << "#";

    // 生成数据部分
    for (size_t i = 0; i < data.size(); ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }

    return oss.str();
}

void MotorController::sendRPMs(int rpmX, int rpmY, int rpmZ) {
    auto commandX = frameToCommandString(0x601, rpmToCANCommand(rpmX));
    sendCommand(commandX);

    auto commandY = frameToCommandString(0x602, rpmToCANCommand(rpmY));
    sendCommand(commandY);

    auto commandZ = frameToCommandString(0x603, rpmToCANCommand(rpmZ));
    sendCommand(commandZ);
}

void MotorController::sendRPMsSpeed(int rpmX, int rpmY, int rpmZ) {
    auto commandX = frameToCommandString(0x601, rpmToCANCommandSpeed(rpmX));
    sendCommand(commandX);

    auto commandY = frameToCommandString(0x602, rpmToCANCommandSpeed(rpmY));
    sendCommand(commandY);

    auto commandZ = frameToCommandString(0x603, rpmToCANCommandSpeed(rpmZ));
    sendCommand(commandZ);
}

void MotorController::sendSpecialCommands(uint32_t canId) {
    // 构造特殊命令1的数据
    std::vector<uint8_t> command1Data(8);
    command1Data[0] = 0x2b;
    command1Data[1] = 0x40;
    command1Data[2] = 0x60;
    command1Data[3] = 0x00;          
    command1Data[4] = 0x2F;
    command1Data[5] = 0x00;
    command1Data[6] = 0x00;
    command1Data[7] = 0x00;

    // 转换为字符串并发送
    std::string command1Str = frameToCommandString(canId, command1Data);
    sendCommand(command1Str);

    // 构造特殊命令2的数据
    std::vector<uint8_t> command2Data(8);
    command2Data[0] = 0x2b;
    command2Data[1] = 0x40;
    command2Data[2] = 0x60;
    command2Data[3] = 0x00;          
    command2Data[4] = 0x3F;
    command2Data[5] = 0x00;
    command2Data[6] = 0x00;
    command2Data[7] = 0x00;

    // 转换为字符串并发送
    std::string command2Str = frameToCommandString(canId, command2Data);
    sendCommand(command2Str);
}


// void MotorController::requestMotorSpeed(uint32_t canId) {
//     std::vector<uint8_t> speedRequestData(8, 0x00);
//     // 根据你的CAN协议设置请求速度的数据
//     // 假设 406c600000000000 对应 CAN ID 0x601, 0x602, 0x603
//     // 这里需要根据实际协议设置请求数据
//     speedRequestData[0] = 0x43; // 示例值，需根据协议调整
//     speedRequestData[1] = 0x6C;
//     speedRequestData[2] = 0x60;
//     speedRequestData[3] = 0x00;
//     speedRequestData[4] = 0x00;
//     speedRequestData[5] = 0x00;
//     speedRequestData[6] = 0x00;
//     speedRequestData[7] = 0x00;

//     std::string commandStr = frameToCommandString(canId, speedRequestData);
//     sendCommand(commandStr);
// }
void MotorController::requestMotorSpeed() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#406c600000000000";
        sendCommand(command);
        // std::cout << "电机 " << motorId << " 下电。" << std::endl;
    }

    // sendCommand("000#0200"); // 关闭站点上报
    // std::cout << "关闭全部站点上报。\n" << std::endl;
}