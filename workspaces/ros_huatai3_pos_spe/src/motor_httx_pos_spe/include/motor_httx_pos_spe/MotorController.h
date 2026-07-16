#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cmath>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <iomanip>
#include <mutex>
#include <atomic>

// MotorInfo 结构体：电机配置参数
struct MotorInfo {
    uint32_t startInc;        // 电机初始增量位置
    uint32_t endInc;          // 电机终止增量位置
    double travelPerTurn;     // 每转行程（mm）
    double incPerDegree;      // 每度对应的增量数
};

// 位置信息结构体
struct Position {
    double x;
    double y;
    double z;
};

struct MotorSpeedStatus {
    double vx;
    double vy;
    double vz;
};

class MotorController {
public:
    MotorController(const std::string& canInterface,
                    const MotorInfo& motorXInfo,
                    const MotorInfo& motorYInfo,
                    const MotorInfo& motorZInfo);

    ~MotorController();

    bool setupCanInterface(const std::string& canInterface);
    void shutdownCanInterface(const std::string& canInterface);

    void recoverMotor(const std::string& motorId);
    void recoverAllMotors();

    void enableMotors();
    void powerOffMotors();

    // 兼容旧接口：主动请求一次速度回传
    void motorSpeedReturn();

    void haltMotors();
    void clearMotorHalt();

    void setMotorToPositionMode();
    void setMotorToSpeedMode();

    void runMotorsSynchronized(double xTravel, double yTravel, double zTravel, double totalTime);
    void runMotors(double xTravel, double yTravel, double zTravel, double totalTime);

    // 单位沿用当前工程约定：mm/s
    void runMotorsSpeed(int32_t vx, int32_t vy, int32_t vz);

    void startReadingPosition();
    void stopReadingPosition();

    Position getCurrentPosition();

    void requestMotorSpeed();

    double getCurrentSpeedX() const {
        std::lock_guard<std::mutex> lock(speedMutex);
        return currentSpeedX;
    }

    double getCurrentSpeedY() const {
        std::lock_guard<std::mutex> lock(speedMutex);
        return currentSpeedY;
    }

    double getCurrentSpeedZ() const {
        std::lock_guard<std::mutex> lock(speedMutex);
        return currentSpeedZ;
    }

private:
    int socketFd;
    double currentX, currentY, currentZ;
    double currentSpeedX, currentSpeedY, currentSpeedZ;
    std::atomic<bool> stopFlag;
    std::string canInterface;
    MotorInfo motorX, motorY, motorZ;
    std::thread positionThread;

    mutable std::mutex positionMutex;
    mutable std::mutex speedMutex;
    std::mutex sendMutex;

    std::chrono::steady_clock::time_point lastPrintX;
    std::chrono::steady_clock::time_point lastPrintY;
    std::chrono::steady_clock::time_point lastPrintZ;
    std::chrono::steady_clock::time_point lastSendErrorLog;
    std::chrono::steady_clock::time_point nextSendAttempt;
    unsigned int consecutiveSendFailures{0};

    const std::chrono::milliseconds refreshInterval = std::chrono::milliseconds(100);

    void startSocketCAN(const std::string& canInterface);
    void stopSocketCAN();

    double incToTravel(const MotorInfo& motor, uint32_t inc);
    uint32_t travelToInc(const MotorInfo& motor, double travel);

    std::vector<uint8_t> decimalToCANCommand(uint32_t position);
    std::vector<uint8_t> rpmToCANCommand(int rpm);
    std::vector<uint8_t> rpmToCANCommandSpeed(int rpm);

    void sendCommand(const std::string& command);
    std::string frameToCommandString(uint32_t canId, const std::vector<uint8_t>& data);

    void sendRPMs(int rpmX, int rpmY, int rpmZ);
    void sendRPMsSpeed(int rpmX, int rpmY, int rpmZ);

    void sendSpecialCommands(uint32_t canId);

    void processSpeedData(const struct can_frame& frame);

    void startReadingPositionThread();
};

#endif // MOTOR_CONTROLLER_H
