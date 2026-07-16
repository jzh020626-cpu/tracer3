#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <cstring>
#include <sys/socket.h>
#include <sys/ioctl.h>  // ioctl 的定义
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <iomanip>  // for std::setprecision
#include <mutex>

// MotorInfo 结构体：电机配置参数
/**
 * @brief 电机信息结构体，定义电机参数
 * @param startInc 电机起始增量
 * @param endInc 电机结束增量
 * @param travelPerTurn 电机每转行程（单位：毫米）
 * @param incPerDegree 每度的增量脉冲数
 */
struct MotorInfo {
    uint32_t startInc;        // 电机初始增量位置
    uint32_t endInc;          // 电机终止增量位置
    double travelPerTurn;     // 每转行程（单位：mm）
    double incPerDegree;      // 每度对应的增量数
};

// 位置信息结构体，表示滑台当前位置
/**
 * @brief 位置信息结构体，表示滑台当前位置
 * @param x X 轴当前位置（单位：毫米）
 * @param y Y 轴当前位置（单位：毫米）
 * @param z Z 轴当前位置（单位：毫米）
 */
struct Position {
    double x;  // X 轴位置（单位：mm）
    double y;  // Y 轴位置（单位：mm）
    double z;  // Z 轴位置（单位：mm）
};

struct MotorSpeedStatus {
    double vx;
    double vy;
    double vz;
};

// MotorController 类，用于控制滑台电机
class MotorController {
public:
    // 构造函数：初始化 CAN 接口和电机参数
    /**
     * @brief 构造函数：初始化 CAN 接口和电机参数
     * @param canInterface CAN 接口名称（如 "can0"）
     * @param motorXInfo X 轴电机参数
     * @param motorYInfo Y 轴电机参数
     * @param motorZInfo Z 轴电机参数
     */
    MotorController(const std::string& canInterface, const MotorInfo& motorXInfo, const MotorInfo& motorYInfo, const MotorInfo& motorZInfo);

    // 析构函数：停止位置读取和关闭 CAN 通信
    /**
     * @brief 析构函数：停止位置读取线程并关闭 CAN 通信
     */
    ~MotorController();

    // 初始化 CAN 接口
    /**
     * @brief 初始化 CAN 接口
     * @param canInterface CAN 接口名称（如 "can0"）
     */
    void setupCanInterface(const std::string& canInterface);

    // 关闭 CAN 接口
    /**
     * @brief 关闭 CAN 接口
     * @param canInterface CAN 接口名称（如 "can0"）
     */
    void shutdownCanInterface(const std::string& canInterface);

    // 使能电机
    /**
     * @brief 使能电机
     */
    void enableMotors();

    // 电机下电
    /**
     * @brief 使能电机
     */
    void powerOffMotors();

    // 实时返回电机运动速度
    void motorSpeedReturn();


    // 设置电机为位置模式
    /**
     * @brief 设置电机为位置模式
     */
    void setMotorToPositionMode();

    // 设置电机为速度模式
    /**
     * @brief 设置电机为速度模式
     */
    void setMotorToSpeedMode();

    // void runMotorsSynchronized(double xTravel, double yTravel, double zTravel, double totalTime);
    void runMotorsSynchronized(double xTravel, double yTravel, double zTravel, double totalTime);

    // 控制电机运动到指定位置
    /**
     * @brief 控制电机运动到指定位置
     * @param xTravel X 轴目标位置（单位：毫米）
     * @param yTravel Y 轴目标位置（单位：毫米）
     * @param zTravel Z 轴目标位置（单位：毫米）
     * @param totalTime 电机运行的时间（单位：秒）
     */
    //void runMotors(double xTravel, double yTravel, double zTravel, int rpmX);
    void runMotors(double xTravel, double yTravel, double zTravel, double totalTime);


    // 控制电机运动到指定位置
    /**
     * @brief 控制电机运动到指定位置
     * @param vx X 轴目标位置（单位：毫米）
     * @param vy Y 轴目标位置（单位：毫米）
     * @param vz Z 轴目标位置（单位：毫米）
     */
    //void runMotors(double xTravel, double yTravel, double zTravel, int rpmX);
    void runMotorsSpeed(int32_t vx, int32_t vy, int32_t vz);

    // 启动读取电机位置的线程
    /**
     * @brief 启动读取电机位置的线程
     */
    void startReadingPosition();

    // 停止读取电机位置的线程
    /**
     * @brief 停止读取电机位置的线程
     */
    void stopReadingPosition();

    /**
     * @brief 获取电机的当前位置
     * @return Position 结构体，包含 X、Y、Z 的当前坐标
     */
    Position getCurrentPosition();

    void requestMotorSpeed();
   
    
    double getCurrentSpeedX() const { return currentSpeedX; }
    double getCurrentSpeedY() const { return currentSpeedY; }
    double getCurrentSpeedZ() const { return currentSpeedZ; }





private:
    int socketFd;                        // CAN 通信套接字文件描述符
    double currentX, currentY, currentZ; // 电机当前位置
    double currentSpeedX, currentSpeedY, currentSpeedZ;
    bool stopFlag;                       // 控制位置读取线程停止的标志
    std::string canInterface;            // CAN 接口名称
    MotorInfo motorX, motorY, motorZ;    // 电机配置信息
    std::thread positionThread;          // 位置读取线程
    mutable std::mutex positionMutex;            // 用于保护当前位置访问的互斥锁
    mutable std::mutex speedMutex; // 添加 mutable 关键字
    std::mutex sendMutex;
    std::chrono::steady_clock::time_point lastPrintX;
    std::chrono::steady_clock::time_point lastPrintY;
    std::chrono::steady_clock::time_point lastPrintZ;
    const std::chrono::milliseconds refreshInterval = std::chrono::milliseconds(1000); // 1秒刷新间隔
    /**
     * @brief 初始化 SocketCAN
     */
    // 内部函数：启动 CAN 通信
    void startSocketCAN(const std::string& canInterface);

    /**
     * @brief 停止 SocketCAN
     */
    // 内部函数：关闭 CAN 通信
    void stopSocketCAN() ;

    /**
     * @brief 增量到行程的转换
     * @param motor 电机信息
     * @param inc   当前增量值
     * @return 对应的行程值（单位：mm）
     */
    double incToTravel(const MotorInfo& motor, uint32_t inc);

    /**
     * @brief 行程到增量的转换
     * @param motor 电机信息
     * @param travel 目标行程（单位：mm）
     * @return 对应的增量值
     */
    uint32_t travelToInc(const MotorInfo& motor, double travel);

    /**
     * @brief 将位置转为 CAN 命令
     * @param position 目标位置的增量值
     * @return 对应的 CAN 数据帧
     */
    std::vector<uint8_t> decimalToCANCommand(uint32_t position);

    /**
     * @brief 将速度（RPM）转换为 CAN 指令
     * @param rpm 目标速度（单位：RPM）
     * @return 对应的 CAN 数据帧
     */
    std::vector<uint8_t> rpmToCANCommand(int rpm);

    /**
     * @brief 在速度模式下将速度（RPM）转换为 CAN 指令
     * @param rpm 目标速度（单位：RPM）
     * @return 对应的 CAN 数据帧
     */
    std::vector<uint8_t> rpmToCANCommandSpeed(int rpm);

    /**
     * @brief 发送 CAN 命令
     * @param command 完整的命令字符串
     */
    void sendCommand(const std::string& command);

    // 内部函数：CAN帧数据转换命令字符串
    /**
     * @brief 将 CAN 帧数据转换为命令字符串
     * @param canId CAN 帧 ID
     * @param data CAN 数据字节流
     * @return 格式化的命令字符串（如 "601#AABBCCDD"）
     */
    std::string frameToCommandString(uint32_t canId, const std::vector<uint8_t>& data);

    /**
     * @brief 发送目标速度到电机
     * @param rpmX X 轴速度
     * @param rpmY Y 轴速度
     * @param rpmZ Z 轴速度
     */
    void sendRPMs(int rpmX, int rpmY, int rpmZ);

    /**
     * @brief 发送目标速度到电机
     * @param rpmX X 轴速度
     * @param rpmY Y 轴速度
     * @param rpmZ Z 轴速度
     */
    void sendRPMsSpeed(int rpmX, int rpmY, int rpmZ);

    /**
     * @brief 发送特殊命令到电机（如控制字）
     * @param canId CAN ID
     */
    void sendSpecialCommands(uint32_t canId);


    //void requestMotorSpeed();

    void processSpeedData(const struct can_frame& frame);

    void startReadingPositionThread();     // 新增：启动读取线程的辅助函数

};

#endif // MOTOR_CONTROLLER_H




