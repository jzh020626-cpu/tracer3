#include "motor_httx_pos_spe/MotorController.h"
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <cstdint>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/sockios.h>
#include <net/if.h>

namespace {
constexpr bool kPrintCanTx = false;
}

MotorController::MotorController(const std::string& canInterface,
                                 const MotorInfo& motorXInfo,
                                 const MotorInfo& motorYInfo,
                                 const MotorInfo& motorZInfo)
    : socketFd(-1),
      currentX(0.0), currentY(0.0), currentZ(0.0),
      currentSpeedX(0.0), currentSpeedY(0.0), currentSpeedZ(0.0),
      stopFlag(false),
      canInterface(canInterface),
      motorX(motorXInfo),
      motorY(motorYInfo),
      motorZ(motorZInfo) {
}

MotorController::~MotorController() {
    stopReadingPosition();
    stopSocketCAN();
}

bool MotorController::setupCanInterface(const std::string& canInterface) {
    std::cout << "[MotorController] setupCanInterface(" << canInterface << ")" << std::endl;
    startSocketCAN(canInterface);
    if (socketFd < 0) {
        std::cerr << "SocketCAN 初始化失败，listener_motor 不会进入在线状态。" << std::endl;
        return false;
    }

    std::cout << "SocketCAN 初始化成功，接口: " << canInterface << std::endl;
    recoverAllMotors();
    return true;
}

void MotorController::shutdownCanInterface(const std::string& canInterface) {
    std::cout << "[MotorController] shutdownCanInterface(" << canInterface << ")" << std::endl;
    stopSocketCAN();
}

void MotorController::recoverMotor(const std::string& motorId) {
    sendCommand(motorId + "#2B40600080000000");
    usleep(100000);

    sendCommand(motorId + "#2B40600006000000");
    usleep(100000);

    sendCommand(motorId + "#2B4060000F000000");
    usleep(100000);

    std::cout << "电机 " << motorId << " 已执行恢复序列(80->06->0F)" << std::endl;
}

void MotorController::recoverAllMotors() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        recoverMotor(motorId);
    }
}

void MotorController::enableMotors() {
    std::cout << "[MotorController] enableMotors()" << std::endl;
    sendCommand("000#0100");
    std::cout << "\n开启全部站点上报。" << std::endl;

    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#2B4060000F000000";
        sendCommand(command);
        std::cout << "电机 " << motorId << " 使能。" << std::endl;
    }
}

void MotorController::powerOffMotors() {
    std::cout << "[MotorController] powerOffMotors()" << std::endl;
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#2B40600006000000";
        sendCommand(command);
        std::cout << "电机 " << motorId << " 下电。" << std::endl;
    }

    sendCommand("000#0200");
    std::cout << "关闭全部站点上报。\n" << std::endl;
}

void MotorController::motorSpeedReturn() {
    requestMotorSpeed();
}

void MotorController::haltMotors() {
    std::cout << "[MotorController] haltMotors() keep current drive mode" << std::endl;
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string haltCommand = motorId + "#2B4060000F010000";
        sendCommand(haltCommand);
    }
}

void MotorController::clearMotorHalt() {
    std::cout << "[MotorController] clearMotorHalt()" << std::endl;
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string clearHaltCommand = motorId + "#2B4060000F000000";
        sendCommand(clearHaltCommand);
    }
}

void MotorController::setMotorToPositionMode() {
    std::cout << "[MotorController] setMotorToPositionMode()" << std::endl;
    std::vector<std::string> motorIds = {"601", "602", "603"};

    for (const auto& motorId : motorIds) {
        std::string modeCommand = motorId + "#2F60600001000000";
        sendCommand(modeCommand);

        std::string startCommand = motorId + "#2B4060000F000000";
        sendCommand(startCommand);

        std::cout << "电机 " << motorId << " 切换到位置模式" << std::endl;
    }

    usleep(100000);
    std::cout << "所有电机已进入位置模式\n" << std::endl;
}

void MotorController::setMotorToSpeedMode() {
    std::cout << "[MotorController] setMotorToSpeedMode()" << std::endl;
    std::vector<std::string> motorIds = {"601", "602", "603"};

    for (const auto& motorId : motorIds) {
        std::string zeroSpeed = motorId + "#2360FF0000000000";
        sendCommand(zeroSpeed);

        std::string modeCommand = motorId + "#2F60600003000000";
        sendCommand(modeCommand);

        std::string startCommand = motorId + "#2B4060000F000000";
        sendCommand(startCommand);

        std::cout << "电机 " << motorId << " 切换到速度模式" << std::endl;
    }

    usleep(100000);
    std::cout << "所有电机已进入速度模式(速度=0)\n" << std::endl;
}

void MotorController::runMotorsSynchronized(double xTravel, double yTravel, double zTravel, double totalTime) {
    Position currentPosition;

    {
        std::lock_guard<std::mutex> lock(positionMutex);
        currentPosition = {currentX, currentY, currentZ};
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "当前滑台位置: X=" << currentPosition.x
              << ", Y=" << currentPosition.y
              << ", Z=" << currentPosition.z << std::endl;

    if (totalTime <= 0.0) {
        std::cerr << "runMotorsSynchronized() 参数错误: totalTime <= 0" << std::endl;
        return;
    }

    double xDistanceNeeded = std::abs(xTravel - currentPosition.x);
    double yDistanceNeeded = std::abs(yTravel - currentPosition.y);
    double zDistanceNeeded = std::abs(zTravel - currentPosition.z);

    double vx = xDistanceNeeded / totalTime;
    double vy = yDistanceNeeded / totalTime;
    double vz = zDistanceNeeded / totalTime;

    int rpmX = static_cast<int>(std::lround(vx / motorX.travelPerTurn * 60.0));
    int rpmY = static_cast<int>(std::lround(vy / motorY.travelPerTurn * 60.0));
    int rpmZ = static_cast<int>(std::lround(vz / motorZ.travelPerTurn * 60.0));

    uint32_t xInc = travelToInc(motorX, xTravel);
    uint32_t yInc = travelToInc(motorY, yTravel);
    uint32_t zInc = travelToInc(motorZ, zTravel);

    if (xInc < motorX.startInc) xInc = motorX.startInc;
    if (xInc > motorX.endInc) xInc = motorX.endInc;

    if (yInc < motorY.startInc) yInc = motorY.startInc;
    if (yInc > motorY.endInc) yInc = motorY.endInc;

    if (zInc < motorZ.startInc) zInc = motorZ.startInc;
    if (zInc > motorZ.endInc) zInc = motorZ.endInc;

    if (xDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x601, decimalToCANCommand(xInc)));
    }
    if (yDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x602, decimalToCANCommand(yInc)));
    }
    if (zDistanceNeeded > 1e-3) {
        sendCommand(frameToCommandString(0x603, decimalToCANCommand(zInc)));
    }

    sendRPMs(rpmX, rpmY, rpmZ);

    if (xDistanceNeeded > 1e-3) sendSpecialCommands(0x601);
    if (yDistanceNeeded > 1e-3) sendSpecialCommands(0x602);
    if (zDistanceNeeded > 1e-3) sendSpecialCommands(0x603);
}

void MotorController::runMotors(double xTravel, double yTravel, double zTravel, double totalTime) {
    Position currentPosition;

    {
        std::lock_guard<std::mutex> lock(positionMutex);
        currentPosition = {currentX, currentY, currentZ};
    }

    if (totalTime <= 0.0) {
        std::cerr << "runMotors() 参数错误: totalTime <= 0" << std::endl;
        return;
    }

    double xDistanceNeeded = std::abs(xTravel - currentPosition.x);
    double yDistanceNeeded = std::abs(yTravel - currentPosition.y);
    double zDistanceNeeded = std::abs(zTravel - currentPosition.z);

    double vx = xDistanceNeeded / totalTime;
    double vy = yDistanceNeeded / totalTime;
    double vz = zDistanceNeeded / totalTime;

    int rpmX = static_cast<int>(std::lround(vx / motorX.travelPerTurn * 60.0));
    int rpmY = static_cast<int>(std::lround(vy / motorY.travelPerTurn * 60.0));
    int rpmZ = static_cast<int>(std::lround(vz / motorZ.travelPerTurn * 60.0));

    bool xMove = xDistanceNeeded > 1e-3;
    bool yMove = yDistanceNeeded > 1e-3;
    bool zMove = zDistanceNeeded > 1e-3;

    if (!xMove && !yMove && !zMove) {
        std::cout << "所有轴已在目标位置，无需运动。" << std::endl;
        return;
    }

    uint32_t xInc = travelToInc(motorX, xTravel);
    uint32_t yInc = travelToInc(motorY, yTravel);
    uint32_t zInc = travelToInc(motorZ, zTravel);

    if (xInc < motorX.startInc) xInc = motorX.startInc;
    if (xInc > motorX.endInc) xInc = motorX.endInc;

    if (yInc < motorY.startInc) yInc = motorY.startInc;
    if (yInc > motorY.endInc) yInc = motorY.endInc;

    if (zInc < motorZ.startInc) zInc = motorZ.startInc;
    if (zInc > motorZ.endInc) zInc = motorZ.endInc;

    if (xMove) {
        auto command = decimalToCANCommand(xInc);
        std::string commandStr = frameToCommandString(0x601, command);
        sendCommand(commandStr);
    }
    if (yMove) {
        auto command = decimalToCANCommand(yInc);
        std::string commandStr = frameToCommandString(0x602, command);
        sendCommand(commandStr);
    }
    if (zMove) {
        auto command = decimalToCANCommand(zInc);
        std::string commandStr = frameToCommandString(0x603, command);
        sendCommand(commandStr);
    }

    sendRPMs(rpmX, rpmY, rpmZ);

    if (xMove) sendSpecialCommands(0x601);
    if (yMove) sendSpecialCommands(0x602);
    if (zMove) sendSpecialCommands(0x603);
}

void MotorController::runMotorsSpeed(int32_t vx, int32_t vy, int32_t vz) {
    int rpmX = static_cast<int>(std::lround(static_cast<double>(vx) / motorX.travelPerTurn * 60.0));
    int rpmY = static_cast<int>(std::lround(static_cast<double>(vy) / motorY.travelPerTurn * 60.0));
    int rpmZ = static_cast<int>(std::lround(static_cast<double>(vz) / motorZ.travelPerTurn * 60.0));

    sendRPMsSpeed(rpmX, rpmY, rpmZ);

    if (vx == 0 && vy == 0 && vz == 0) {
        std::cout << "所有轴速度为零，无需运动。" << std::endl;
        return;
    }
}

void MotorController::startReadingPosition() {
    if (positionThread.joinable()) {
        std::cerr << "读取线程已启动，无法重复启动！" << std::endl;
        return;
    }

    if (socketFd < 0) {
        std::cerr << "SocketCAN 文件描述符无效，请确保 CAN 接口已正确打开！" << std::endl;
        return;
    }

    stopFlag.store(false);
    positionThread = std::thread(&MotorController::startReadingPositionThread, this);
}

void MotorController::processSpeedData(const struct can_frame& frame) {
    uint32_t u =
        uint32_t(frame.data[4]) |
        (uint32_t(frame.data[5]) << 8) |
        (uint32_t(frame.data[6]) << 16) |
        (uint32_t(frame.data[7]) << 24);
    int32_t speed_raw = static_cast<int32_t>(u);

    const double scale = 17920.0;
    double rpm = static_cast<double>(speed_raw) / scale;

    double speedMmPerSec = 0.0;
    if (frame.can_id == 0x581) {
        speedMmPerSec = rpm * motorX.travelPerTurn / 60.0;
    } else if (frame.can_id == 0x582) {
        speedMmPerSec = rpm * motorY.travelPerTurn / 60.0;
    } else if (frame.can_id == 0x583) {
        speedMmPerSec = rpm * motorZ.travelPerTurn / 60.0;
    } else {
        return;
    }

    std::lock_guard<std::mutex> speedLock(speedMutex);
    if (frame.can_id == 0x581) currentSpeedX = speedMmPerSec;
    else if (frame.can_id == 0x582) currentSpeedY = speedMmPerSec;
    else if (frame.can_id == 0x583) currentSpeedZ = speedMmPerSec;
}

void MotorController::startReadingPositionThread() {
    while (!stopFlag.load()) {
        if (socketFd < 0) {
            std::cerr << "SocketCAN 文件描述符无效，无法读取。" << std::endl;
            break;
        }

        struct can_frame frame;
        int nbytes = read(socketFd, &frame, sizeof(frame));
        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            } else {
                perror("SocketCAN 读取失败");
                break;
            }
        }

        uint32_t data = 0;
        for (int i = 0; i < frame.can_dlc && i < 4; ++i) {
            data |= static_cast<uint32_t>(frame.data[i]) << (8 * i);
        }

        if (frame.can_id == 0x181 || frame.can_id == 0x281 || frame.can_id == 0x381) {
            std::lock_guard<std::mutex> lock(positionMutex);

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
                continue;
            }

            if (motor) {
                *currentPosition = incToTravel(*motor, data);
                if (*currentPosition > 500) {
                    *currentPosition = 0;
                }

                auto now = std::chrono::steady_clock::now();
                if (now - *lastPrint > refreshInterval) {
                    *lastPrint = now;
                }
            }
        }
        else if (frame.can_id == 0x581 || frame.can_id == 0x582 || frame.can_id == 0x583) {
            processSpeedData(frame);
        }
    }

    std::cout << "读取线程已安全退出。" << std::endl;
}

void MotorController::stopReadingPosition() {
    stopFlag.store(true);
    if (positionThread.joinable()) {
        positionThread.join();
        std::cout << "读取线程已停止。\n" << std::endl;
    } else {
        std::cerr << "读取线程未运行，无法停止！" << std::endl;
    }
}

Position MotorController::getCurrentPosition() {
    std::lock_guard<std::mutex> lock(positionMutex);
    return {currentX, currentY, currentZ};
}

void MotorController::startSocketCAN(const std::string& canInterface) {
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
        std::cerr << "检测到已打开的 SocketCAN，已关闭旧连接。" << std::endl;
    }

    socketFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socketFd < 0) {
        perror("SocketCAN 初始化失败");
        return;
    }

    struct ifreq ifr;
    strncpy(ifr.ifr_name, canInterface.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(socketFd, SIOCGIFINDEX, &ifr) < 0) {
        perror("SocketCAN 接口检查失败");
        close(socketFd);
        socketFd = -1;
        return;
    }
    const int ifIndex = ifr.ifr_ifindex;

    if (ioctl(socketFd, SIOCGIFFLAGS, &ifr) < 0) {
        perror("SocketCAN 接口状态检查失败");
        close(socketFd);
        socketFd = -1;
        return;
    }
    if ((ifr.ifr_flags & IFF_UP) == 0) {
        std::cerr << "CAN 接口 " << canInterface
                  << " 未处于 UP 状态；请先由启动脚本完成特权初始化。" << std::endl;
        close(socketFd);
        socketFd = -1;
        return;
    }

    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifIndex;

    if (bind(socketFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("SocketCAN 绑定失败");
        close(socketFd);
        socketFd = -1;
        return;
    }

    int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socketFd, F_SETFL, flags | O_NONBLOCK);
    } else {
        perror("设置非阻塞模式失败");
    }

    std::cout << "SocketCAN " << canInterface << " 已成功启动。" << std::endl;
}

void MotorController::stopSocketCAN() {
    if (socketFd >= 0) {
        close(socketFd);
        socketFd = -1;
    }
}

double MotorController::incToTravel(const MotorInfo& motor, uint32_t inc) {
    if (inc <= motor.startInc) {
        return 0.0;
    }
    if (inc >= motor.endInc) {
        double degrees = static_cast<double>(motor.endInc - motor.startInc) / motor.incPerDegree;
        return (degrees / 360.0) * motor.travelPerTurn;
    }

    double degrees = static_cast<double>(inc - motor.startInc) / motor.incPerDegree;
    return (degrees / 360.0) * motor.travelPerTurn;
}

uint32_t MotorController::travelToInc(const MotorInfo& motor, double travel) {
    double totalInc = (travel / motor.travelPerTurn) * 360.0 * motor.incPerDegree;
    return motor.startInc + static_cast<uint32_t>(totalInc);
}

std::vector<uint8_t> MotorController::decimalToCANCommand(uint32_t position) {
    std::vector<uint8_t> canCommand(8);
    canCommand[0] = 0x23;
    canCommand[1] = 0x7A;
    canCommand[2] = 0x60;
    canCommand[3] = 0x00;
    canCommand[4] = position & 0xFF;
    canCommand[5] = (position >> 8) & 0xFF;
    canCommand[6] = (position >> 16) & 0xFF;
    canCommand[7] = (position >> 24) & 0xFF;
    return canCommand;
}

std::vector<uint8_t> MotorController::rpmToCANCommand(int rpm) {
    std::vector<uint8_t> canCommand(8);
    int32_t rawSpeed = static_cast<int32_t>(std::lround(static_cast<double>(rpm) * 17920.0));
    uint32_t hexSpeed = static_cast<uint32_t>(rawSpeed);

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
    int32_t rawSpeed = static_cast<int32_t>(std::lround(static_cast<double>(rpm) * 17920.0));
    uint32_t hexSpeed = static_cast<uint32_t>(rawSpeed);

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
    const auto now = std::chrono::steady_clock::now();
    if (nextSendAttempt.time_since_epoch().count() != 0 && now < nextSendAttempt) {
        return;
    }

    auto recordFailure = [&](const char* reason, int errorCode, int queuedBytes) {
        ++consecutiveSendFailures;
        const auto backoff = consecutiveSendFailures >= 5
            ? std::chrono::seconds(1)
            : std::chrono::milliseconds(100);
        nextSendAttempt = now + backoff;

        if (lastSendErrorLog.time_since_epoch().count() == 0 ||
            now - lastSendErrorLog >= std::chrono::seconds(1)) {
            std::cerr << "SocketCAN send deferred: " << reason
                      << ", iface=" << canInterface
                      << ", command=" << command
                      << ", queued_bytes=" << queuedBytes
                      << ", failures=" << consecutiveSendFailures;
            if (errorCode != 0) {
                std::cerr << ", errno=" << errorCode
                          << " (" << std::strerror(errorCode) << ")";
            }
            std::cerr << std::endl;
            lastSendErrorLog = now;
        }
    };

    if (socketFd < 0) {
        recordFailure("socket unavailable", ENODEV, -1);
        return;
    }

    const auto separator = command.find('#');
    const std::string canIdText = command.substr(0, separator);
    const std::string payload = separator == std::string::npos
        ? std::string()
        : command.substr(separator + 1);
    if (separator == std::string::npos || canIdText.empty() ||
        payload.size() > CAN_MAX_DLEN * 2 || payload.size() % 2 != 0) {
        recordFailure("invalid CAN command", EINVAL, -1);
        return;
    }

    char* idEnd = nullptr;
    errno = 0;
    const unsigned long parsedCanId = std::strtoul(canIdText.c_str(), &idEnd, 16);
    if (errno != 0 || idEnd == canIdText.c_str() || *idEnd != '\0' ||
        parsedCanId > CAN_EFF_MASK) {
        recordFailure("invalid CAN id", EINVAL, -1);
        return;
    }

    struct can_frame frame {};
    frame.can_id = parsedCanId <= CAN_SFF_MASK
        ? static_cast<canid_t>(parsedCanId)
        : static_cast<canid_t>(parsedCanId) | CAN_EFF_FLAG;
    frame.can_dlc = static_cast<__u8>(payload.size() / 2);

    for (std::size_t i = 0; i < frame.can_dlc; ++i) {
        const std::string byteText = payload.substr(i * 2, 2);
        char* byteEnd = nullptr;
        errno = 0;
        const unsigned long parsedByte = std::strtoul(byteText.c_str(), &byteEnd, 16);
        if (errno != 0 || byteEnd == byteText.c_str() || *byteEnd != '\0' ||
            parsedByte > 0xff) {
            recordFailure("invalid CAN payload", EINVAL, -1);
            return;
        }
        frame.data[i] = static_cast<__u8>(parsedByte);
    }

    int queuedBytes = -1;
    (void)ioctl(socketFd, SIOCOUTQ, &queuedBytes);

    struct pollfd descriptor {};
    descriptor.fd = socketFd;
    descriptor.events = POLLOUT;
    const int pollResult = poll(&descriptor, 1, 0);
    if (pollResult <= 0 || (descriptor.revents & POLLOUT) == 0) {
        recordFailure("send queue is not writable", pollResult < 0 ? errno : EAGAIN,
                      queuedBytes);
        return;
    }

    const ssize_t sent = send(socketFd, &frame, sizeof(frame),
                              MSG_DONTWAIT | MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(sizeof(frame))) {
        recordFailure("nonblocking send failed", sent < 0 ? errno : EIO, queuedBytes);
        return;
    }

    consecutiveSendFailures = 0;
    nextSendAttempt = {};
    if (kPrintCanTx) {
        std::cout << "[SEND] " << canInterface << " " << command << std::endl;
    }
}

std::string MotorController::frameToCommandString(uint32_t canId, const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setw(3) << std::setfill('0') << canId << "#";

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
    std::vector<uint8_t> command1Data(8);
    command1Data[0] = 0x2b;
    command1Data[1] = 0x40;
    command1Data[2] = 0x60;
    command1Data[3] = 0x00;
    command1Data[4] = 0x2F;
    command1Data[5] = 0x00;
    command1Data[6] = 0x00;
    command1Data[7] = 0x00;

    std::string command1Str = frameToCommandString(canId, command1Data);
    sendCommand(command1Str);

    std::vector<uint8_t> command2Data(8);
    command2Data[0] = 0x2b;
    command2Data[1] = 0x40;
    command2Data[2] = 0x60;
    command2Data[3] = 0x00;
    command2Data[4] = 0x3F;
    command2Data[5] = 0x00;
    command2Data[6] = 0x00;
    command2Data[7] = 0x00;

    std::string command2Str = frameToCommandString(canId, command2Data);
    sendCommand(command2Str);
}

void MotorController::requestMotorSpeed() {
    std::vector<std::string> motorIds = {"601", "602", "603"};
    for (const auto& motorId : motorIds) {
        std::string command = motorId + "#406c600000000000";
        sendCommand(command);
    }
}
