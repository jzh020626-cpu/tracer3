#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <termios.h>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

namespace force_sensor_pkg {

class ForceSensorReader {
public:
    ForceSensorReader(const std::string& port = "/dev/ttyUSB1", int baud_rate = 115200,
                      bool debug = true, bool enable_crc_check = true);
    ~ForceSensorReader();

    bool openPort();
    void closePort();
    bool writeCommand(const std::string& command, bool& success, std::string& response);
    bool setSamplingRate(int rate);
    int querySamplingRate();
    void setCrcCheck(bool enable);
    void startRealtime();
    void stopRealtime();
    void readData();
    void displayData();
    const std::vector<float>& getEngValues() const;
    bool isPortOpen() const { return is_port_open_; } // Add getter for is_port_open_

private:
    int realTimeDataProcess();
    uint32_t getCrc32(const std::vector<uint8_t>& data, size_t length);

    // Constants
    static constexpr size_t RX_BUFFER_SIZE = 256;  //16384
    static constexpr size_t PROCESS_BUFFER_SIZE = 128;  //16384
    static constexpr size_t M812X_CHN_NUMBER = 6;
    static const uint32_t CRC32_TABLE[256];

    // Serial port
    int serial_fd_;
    bool is_port_open_;
    std::string port_name_;
    int baud_rate_;
    bool debug_;
    bool enable_crc_check_;

    // Buffers
    std::vector<uint8_t> rx_buffer_;
    size_t rx_counter_;
    std::vector<uint8_t> deal_data_buffer_;
    std::vector<uint8_t> ordinal_buffer_;
    std::vector<float> eng_values_;
    bool is_real_time_;
    size_t package_counter_;
    size_t deal_data_index_;
    size_t data_ptr_out_;
};

class ForceSensorNode : public rclcpp::Node {
public:
    ForceSensorNode();
    ~ForceSensorNode();
    const ForceSensorReader& getSensor() const { return sensor_; } // Add getter for sensor_

private:
    void timerCallback();
    void cleanup();

    ForceSensorReader sensor_;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace force_sensor_pkg
