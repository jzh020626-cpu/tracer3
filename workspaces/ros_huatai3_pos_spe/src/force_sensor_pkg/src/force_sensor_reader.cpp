#include "force_sensor_reader.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <regex>

namespace force_sensor_pkg {

const uint32_t ForceSensorReader::CRC32_TABLE[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
    0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
    0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
    0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
    0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
    0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
    0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
    0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
    0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
    0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
    0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
    0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
    0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
    0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
    0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
    0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
    0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
    0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
    0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
    0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
    0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
    0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
    0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
    0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
    0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
    0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
    0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
    0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
    0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
    0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
    0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
    0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
    0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

ForceSensorReader::ForceSensorReader(const std::string& port, int baud_rate, bool debug, bool enable_crc_check)
    : serial_fd_(-1),
      is_port_open_(false),
      port_name_(port),
      baud_rate_(baud_rate),
      debug_(debug),
      enable_crc_check_(enable_crc_check),
      rx_buffer_(RX_BUFFER_SIZE, 0),
      rx_counter_(0),
      deal_data_buffer_(PROCESS_BUFFER_SIZE, 0),
      ordinal_buffer_(PROCESS_BUFFER_SIZE, 0),
      eng_values_(M812X_CHN_NUMBER, 0.0f),
      is_real_time_(false),
      package_counter_(0),
      deal_data_index_(0),
      data_ptr_out_(0xFFFFFFFF) {}

ForceSensorReader::~ForceSensorReader() {
    closePort();
}

bool ForceSensorReader::openPort() {
    serial_fd_ = open(port_name_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_fd_ == -1) {
        std::cerr << "Failed to open port " << port_name_ << ": " << strerror(errno) << std::endl;
        is_port_open_ = false;
        return false;
    }

    struct termios options;
    tcgetattr(serial_fd_, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag = (options.c_cflag & ~CSIZE) | CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag |= CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;
    tcflush(serial_fd_, TCIFLUSH);
    if (tcsetattr(serial_fd_, TCSANOW, &options) != 0) {
        std::cerr << "Failed to set port attributes: " << strerror(errno) << std::endl;
        close(serial_fd_);
        is_port_open_ = false;
        return false;
    }

    is_port_open_ = true;
    std::cout << "Opened port " << port_name_ << " at " << baud_rate_ << " baud" << std::endl;
    return true;
}

void ForceSensorReader::closePort() {
    if (is_port_open_ && serial_fd_ != -1) {
        if (is_real_time_) {
            stopRealtime();
        }
        close(serial_fd_);
        is_port_open_ = false;
        std::cout << "Port closed" << std::endl;
    }
}

bool ForceSensorReader::writeCommand(const std::string& command, bool& success, std::string& response) {
    if (!is_port_open_) {
        std::cerr << "Port is not open" << std::endl;
        return false;
    }

    for (int retry = 0; retry < 3; ++retry) {
        tcflush(serial_fd_, TCOFLUSH);
        if (write(serial_fd_, command.c_str(), command.size()) != static_cast<ssize_t>(command.size())) {
            std::cerr << "Error writing to port: " << strerror(errno) << std::endl;
            return false;
        }
        std::cout << "Sent command: " << command.substr(0, command.size() - 2) << std::endl;
        usleep(300000); // 300ms

        std::vector<uint8_t> buffer(128);
        ssize_t bytes_read = read(serial_fd_, buffer.data(), buffer.size());
        if (bytes_read > 0) {
            response = std::string(buffer.begin(), buffer.begin() + bytes_read);
            std::cout << "Response: " << response.substr(0, response.size() - 2) << std::endl;
            if (debug_) {
                std::stringstream ss;
                for (ssize_t i = 0; i < bytes_read; ++i) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
                }
                std::cout << "Raw response bytes: " << ss.str() << std::endl;
            }
            if (response.find("ERROR") != std::string::npos) {
                std::cerr << "Command failed" << std::endl;
                return false;
            }
            success = true;
            return true;
        }
        usleep(100000); // 100ms
    }
    std::cerr << "Failed to get response after retries" << std::endl;
    return false;
}

bool ForceSensorReader::setSamplingRate(int rate) {
    if (!is_port_open_) {
        std::cerr << "Port is not open" << std::endl;
        return false;
    }
    if (rate < 1 || rate > 1000) {
        std::cerr << "Invalid sampling rate. Must be between 1 and 1000 Hz." << std::endl;
        return false;
    }
    std::string command = "AT+SMPF=" + std::to_string(rate) + "\r\n";
    bool success = false;
    std::string response;
    if (!writeCommand(command, success, response) || !success) {
        return false;
    }
    std::regex pattern(R"(ACK\+SMPF=(\d+)\$OK)");
    std::smatch match;
    if (std::regex_search(response, match, pattern) && std::stoi(match[1]) == rate) {
        std::cout << "Sampling rate set to " << rate << " Hz" << std::endl;
        return true;
    }
    std::cerr << "Failed to verify sampling rate. Response: " << response << std::endl;
    return false;
}

int ForceSensorReader::querySamplingRate() {
    if (!is_port_open_) {
        std::cerr << "Port is not open" << std::endl;
        return 0;
    }
    bool success = false;
    std::string response;
    if (!writeCommand("AT+SMPF=?\r\n", success, response) || !success) {
        return 0;
    }
    std::regex pattern(R"(ACK\+SMPF=(\d+)\$OK)");
    std::smatch match;
    if (std::regex_search(response, match, pattern)) {
        int rate = std::stoi(match[1]);
        std::cout << "Current sampling rate: " << rate << " Hz" << std::endl;
        return rate;
    }
    std::cerr << "Failed to parse sampling rate. Response: " << response << std::endl;
    return 0;
}

void ForceSensorReader::setCrcCheck(bool enable) {
    enable_crc_check_ = enable;
    std::cout << "CRC check " << (enable ? "enabled" : "disabled") << std::endl;
}

uint32_t ForceSensorReader::getCrc32(const std::vector<uint8_t>& data, size_t length) {
    uint32_t n_reg = 0xFFFFFFFF;
    for (size_t n = 0; n < length; ++n) {
        n_reg ^= data[n];
        for (int i = 0; i < 4; ++i) {
            uint32_t n_temp = CRC32_TABLE[(n_reg >> 24) & 0xFF];
            n_reg = (n_reg << 8) & 0xFFFFFFFF;
            n_reg ^= n_temp;
        }
    }
    return n_reg;
}

void ForceSensorReader::startRealtime() {
    if (!is_port_open_) {
        std::cerr << "Please open the port first" << std::endl;
        return;
    }
    rx_counter_ = 0;
    rx_buffer_.assign(RX_BUFFER_SIZE, 0);
    data_ptr_out_ = rx_counter_;
    package_counter_ = 0;
    deal_data_index_ = 0;
    is_real_time_ = true;
    bool success = false;
    std::string response;
    writeCommand("AT+GSD\r\n", success, response);
    std::cout << "Started real-time data acquisition" << std::endl;
}

void ForceSensorReader::stopRealtime() {
    if (is_real_time_) {
        bool success = false;
        std::string response;
        writeCommand("AT+GSD=STOP\r\n", success, response);
        is_real_time_ = false;
        std::cout << "Stopped real-time data acquisition" << std::endl;
    }
}

void ForceSensorReader::readData() {
    if (!is_port_open_) {
        std::cerr << "Port is not open" << std::endl;
        return;
    }
    std::vector<uint8_t> data(RX_BUFFER_SIZE);
    ssize_t bytes_read = read(serial_fd_, data.data(), RX_BUFFER_SIZE);
    if (bytes_read > 0) {
        if (debug_) {
            std::stringstream ss;
            for (ssize_t i = 0; i < bytes_read; ++i) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
            }
            std::cout << "Received " << bytes_read << " bytes: " << ss.str() << std::endl;
        }
        for (ssize_t i = 0; i < bytes_read; ++i) {
            if (rx_counter_ >= RX_BUFFER_SIZE) {
                rx_counter_ = 0;
            }
            rx_buffer_[rx_counter_++] = data[i];
        }
        if (is_real_time_) {
            int result = realTimeDataProcess();
            if (result == 1) {
                displayData();
            } else if (result == -1) {
                std::cerr << "Failed to process data frame" << std::endl;
            }
        }
    } else if (bytes_read < 0) {
        std::cerr << "Error reading from port: " << strerror(errno) << std::endl;
    }
}

void ForceSensorReader::displayData() {
    // std::cout << "\nEngineering Values:" << std::endl;
    // for (size_t i = 0; i < eng_values_.size(); ++i) {
    //     std::cout << "Channel " << (i + 1) << ": " << std::fixed << std::setprecision(6) << eng_values_[i] << std::endl;
    // }
}

const std::vector<float>& ForceSensorReader::getEngValues() const {
    return eng_values_;
}

int ForceSensorReader::realTimeDataProcess() {
    if (rx_buffer_.empty() || data_ptr_out_ == 0xFFFFFFFF) {
        if (debug_) {
            std::cerr << "No data to process or invalid pointer" << std::endl;
        }
        return 0;
    }

    size_t header_index = data_ptr_out_;
    size_t rx_counter_current = rx_counter_;
    size_t rx_length_temp = (rx_counter_current >= header_index) ?
        rx_counter_current - header_index :
        RX_BUFFER_SIZE - header_index + rx_counter_current;

    if (rx_length_temp < 34) {
        if (debug_) {
            std::cerr << "Insufficient data length: " << rx_length_temp << std::endl;
        }
        return -1;
    }

    bool data_header_flag = false;
    for (size_t i = 0; i < rx_length_temp; ++i) {
        if (header_index == RX_BUFFER_SIZE - 1) {
            if (rx_buffer_[header_index] == 0xAA && rx_buffer_[0] == 0x55) {
                data_header_flag = true;
                break;
            }
        } else if (header_index + 1 < RX_BUFFER_SIZE && rx_buffer_[header_index] == 0xAA && rx_buffer_[header_index + 1] == 0x55) {
            data_header_flag = true;
            break;
        }
        header_index = (header_index + 1) % RX_BUFFER_SIZE;
    }

    if (!data_header_flag) {
        if (debug_) {
            std::cerr << "No valid header found" << std::endl;
        }
        return -1;
    }

    size_t high_index = (header_index + 2) % RX_BUFFER_SIZE;
    size_t low_index = (header_index + 3) % RX_BUFFER_SIZE;
    size_t package_length = (rx_buffer_[high_index] << 8) + rx_buffer_[low_index];
    if (debug_) {
        std::cout << "Package length: " << package_length << std::endl;
    }

    rx_length_temp = (rx_counter_current >= header_index) ?
        rx_counter_current - header_index :
        RX_BUFFER_SIZE - header_index + rx_counter_current;

    if (rx_length_temp < package_length + 4) {
        if (debug_) {
            std::cerr << "Data length too short for package: " << rx_length_temp << " < " << (package_length + 4) << std::endl;
        }
        return -1;
    }

    size_t move_index = (header_index + 6) % RX_BUFFER_SIZE;
    for (size_t i = 0; i < package_length - 2; ++i) {
        ordinal_buffer_[i] = rx_buffer_[move_index];
        move_index = (move_index + 1) % RX_BUFFER_SIZE;
    }
    data_ptr_out_ = move_index;

    if (debug_) {
        std::stringstream ss;
        for (size_t i = 0; i < package_length - 2; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ordinal_buffer_[i]);
        }
        std::cout << "Ordinal buffer (" << (package_length - 2) << " bytes): " << ss.str() << std::endl;
    }

    uint8_t check_sum = std::accumulate(ordinal_buffer_.begin(), ordinal_buffer_.begin() + (package_length - 3), 0) & 0xFF;
    uint32_t dw_check_crc32 = getCrc32(ordinal_buffer_, package_length - 6);
    size_t crc_index = package_length - 6;
    uint32_t dw_rx_check_crc32 = 0;
    std::memcpy(&dw_rx_check_crc32, &ordinal_buffer_[crc_index], sizeof(uint32_t));
    dw_rx_check_crc32 = __builtin_bswap32(dw_rx_check_crc32); // Big-endian to host

    if (debug_) {
        std::cout << "Calculated CRC32: " << std::hex << std::setw(8) << std::setfill('0') << dw_check_crc32
                  << ", Received CRC32: " << std::hex << std::setw(8) << std::setfill('0') << dw_rx_check_crc32 << std::endl;
        std::cout << "Calculated Checksum: " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(check_sum)
                  << ", Received Checksum: " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ordinal_buffer_[package_length - 3]) << std::endl;
    }

    if (enable_crc_check_) {
        if (dw_check_crc32 != dw_rx_check_crc32 && check_sum != ordinal_buffer_[package_length - 3]) {
            if (debug_) {
                std::cerr << "CRC32 or checksum validation failed" << std::endl;
            }
            return -1;
        }
    } else if (debug_) {
        std::cout << "CRC check disabled, skipping validation" << std::endl;
    }

    size_t dw_actual_data_len = (enable_crc_check_ && dw_check_crc32 == dw_rx_check_crc32) ? package_length - 6 : package_length - 3;

    if (deal_data_index_ >= PROCESS_BUFFER_SIZE - dw_actual_data_len) {
        if (debug_) {
            std::cerr << "Deal data buffer overflow" << std::endl;
        }
        deal_data_index_ = 0;
        package_counter_ = 0;
        return -1;
    }

    std::memcpy(&deal_data_buffer_[deal_data_index_], ordinal_buffer_.data(), dw_actual_data_len);
    deal_data_index_ += dw_actual_data_len;
    package_counter_++;

    if (deal_data_index_ == 0) {
        if (debug_) {
            std::cerr << "No data to process after copying" << std::endl;
        }
        return -1;
    }

    size_t point_counter = deal_data_index_ / (4 * M812X_CHN_NUMBER);
    if (point_counter >= PROCESS_BUFFER_SIZE || point_counter <= 0) {
        if (debug_) {
            std::cerr << "Invalid point counter: " << point_counter << std::endl;
        }
        deal_data_index_ = 0;
        package_counter_ = 0;
        return -1;
    }

    size_t index = 0;
    eng_values_.assign(M812X_CHN_NUMBER, 0.0f);
    for (size_t i = 0; i < point_counter; ++i) {
        for (size_t k = 0; k < M812X_CHN_NUMBER; ++k) {
            float value;
            std::memcpy(&value, &deal_data_buffer_[index], sizeof(float));
            eng_values_[k] += value;
            if (debug_) {
                std::stringstream ss;
                for (size_t j = 0; j < 4; ++j) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(deal_data_buffer_[index + j]);
                }
                std::cout << "Channel " << (k + 1) << " raw bytes: " << ss.str() << ", Value: "
                          << std::fixed << std::setprecision(6) << value << std::endl;
            }
            index += 4;
        }
    }

    for (size_t k = 0; k < M812X_CHN_NUMBER; ++k) {
        eng_values_[k] = point_counter > 0 ? eng_values_[k] / point_counter : 0.0f;
    }

    deal_data_index_ = 0;
    package_counter_ = 0;
    return 1;
}

ForceSensorNode::ForceSensorNode()
    : Node("force_sensor_node"), sensor_("/dev/ttyUSB0", 115200, false, false) {
    publisher_ = create_publisher<std_msgs::msg::Float32MultiArray>("huatai3_force", 10);
    timer_ = create_wall_timer(std::chrono::milliseconds(10), std::bind(&ForceSensorNode::timerCallback, this));
    RCLCPP_INFO(get_logger(), "Force sensor node initialized");

    if (!sensor_.openPort()) {
        RCLCPP_ERROR(get_logger(), "Failed to open serial port.");
        return;
    }

    int rate = sensor_.querySamplingRate();
    if (rate == 0) {
        RCLCPP_WARN(get_logger(), "Failed to query sampling rate. Continuing with default.");
    } else if (!sensor_.setSamplingRate(100)) {
        RCLCPP_WARN(get_logger(), "Failed to set sampling rate. Continuing with default.");
    } else {
        RCLCPP_INFO(get_logger(), "Sampling rate set successfully.");
    }

    sensor_.startRealtime();
}

ForceSensorNode::~ForceSensorNode() {
    cleanup();
}

void ForceSensorNode::timerCallback() {
    sensor_.readData();
    const auto& eng_values = sensor_.getEngValues();
    if (!eng_values.empty() && (eng_values[0] != 0.0f || eng_values[1] != 0.0f || eng_values[2] != 0.0f)) {
        auto msg = std_msgs::msg::Float32MultiArray();
        msg.data.assign(eng_values.begin(), eng_values.begin() + 3);
        publisher_->publish(msg);
        RCLCPP_INFO(get_logger(), "Published force data: x=%.6f, y=%.6f, z=%.6f",
                    msg.data[0], msg.data[1], msg.data[2]);
    }
}

void ForceSensorNode::cleanup() {
    sensor_.closePort();
    RCLCPP_INFO(get_logger(), "Force sensor node shutdown");
}

} // namespace force_sensor_pkg

int main(int argc, char* argv[]) {
    rclcpp::init(argc, argv);
    struct termios old_settings, new_settings;
    int terminal_fd = STDIN_FILENO;
    tcgetattr(terminal_fd, &old_settings);
    new_settings = old_settings;
    new_settings.c_lflag &= ~(ICANON | ECHO);
    new_settings.c_cc[VMIN] = 1;
    new_settings.c_cc[VTIME] = 0;
    tcsetattr(terminal_fd, TCSANOW, &new_settings);
    int flags = fcntl(terminal_fd, F_GETFL);
    fcntl(terminal_fd, F_SETFL, flags | O_NONBLOCK);

    auto node = std::make_shared<force_sensor_pkg::ForceSensorNode>();
    if (!node->getSensor().isPortOpen()) {
        std::cerr << "Node initialization failed." << std::endl;
        tcsetattr(terminal_fd, TCSANOW, &old_settings);
        rclcpp::shutdown();
        return 1;
    }

    std::cout << "Press 'q' to quit." << std::endl;
    while (rclcpp::ok()) {
        rclcpp::spin_some(node);
        char c;
        if (read(terminal_fd, &c, 1) > 0 && (c == 'q' || c == 'Q')) {
            std::cout << "Closing port..." << std::endl;
            break;
        }
        usleep(50000); // 50ms
    }

    tcsetattr(terminal_fd, TCSANOW, &old_settings);
    rclcpp::shutdown();
    std::cout << "Exiting..." << std::endl;
    return 0;
}
