// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node {
// public:
//     MotorCommandPublisher() : Node("motor_command_publisher") {
//         publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>("motor_commands", 10);
        
//         std::cout << "电机指令发布器已启动" << std::endl;
//         std::cout << "输入格式说明:" << std::endl;
//         std::cout << "位置模式: mode pos x y z time [rel]" << std::endl;
//         std::cout << "速度模式: mode speed vx vy vz" << std::endl;
//         std::cout << "停止命令: stop" << std::endl;
        
//         timer_ = this->create_wall_timer(
//             100ms, std::bind(&MotorCommandPublisher::timer_callback, this));
//     }

// private:
//     void timer_callback() {
//         std::string input;
//         std::getline(std::cin, input);
        
//         if (input.empty()) return;
        
//         auto message = base_interfaces_demo::msg::MotorCommand();
        
//         if (input == "stop") {
//             message.command_type = "stop";
//             publisher_->publish(message);
//             return;
//         }
        
//         std::istringstream iss(input);
//         std::string mode;
//         iss >> mode;
        
//         if (mode == "mode") {
//             std::string sub_mode;
//             iss >> sub_mode;
            
//             if (sub_mode == "pos") {
//                 message.command_type = "position";
//                 iss >> message.x >> message.y >> message.z >> message.time;
                
//                 std::string rel;
//                 if (iss >> rel && rel == "rel") {
//                     message.is_relative = true;
//                 } else {
//                     message.is_relative = false;
//                 }
                
//                 publisher_->publish(message);
//             }
//             else if (sub_mode == "speed") {
//                 message.command_type = "speed";
//                 iss >> message.vx >> message.vy >> message.vz;
//                 publisher_->publish(message);
//             }
//         }
//     }
    
//     rclcpp::TimerBase::SharedPtr timer_;
//     rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr publisher_;
// };

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
//     rclcpp::spin(std::make_shared<MotorCommandPublisher>());
//     rclcpp::shutdown();
//     return 0;
// }




// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>
// #include <csignal>
// #include <sstream>
// #include <algorithm>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node {
// public:
//     MotorCommandPublisher() : Node("motor_command_publisher") {
//         publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>("motor_commands", 10);
        
//         print_usage();
        
//         timer_ = this->create_wall_timer(
//             100ms, std::bind(&MotorCommandPublisher::timer_callback, this));
//     }

// private:
//     void print_usage() {
//         std::cout << "\n=== 电机控制指令系统 ===" << std::endl;
//         std::cout << "1. 位置控制: pos <x> <y> <z> <time(s)> [rel]" << std::endl;
//         std::cout << "   (示例: pos 100 50 20 5.0 rel - 相对移动)" << std::endl;
//         std::cout << "   (示例: pos 100 50 20 5.0    - 绝对移动)" << std::endl;
//         std::cout << "2. 速度控制: speed <vx> <vy> <vz>" << std::endl;
//         std::cout << "   (示例: speed 10 0 -5)" << std::endl;
//         std::cout << "3. 紧急停止: stop" << std::endl;
//         std::cout << "4. 安全退出: exit" << std::endl;
//         std::cout << "5. 帮助: help" << std::endl;
//         std::cout << "========================\n" << std::endl;
//     }

//     std::vector<std::string> split_input(const std::string& input) {
//         std::istringstream iss(input);
//         std::vector<std::string> tokens;
//         std::string token;
        
//         while (iss >> token) {
//             tokens.push_back(token);
//         }
        
//         return tokens;
//     }

//     void timer_callback() {
//         std::string input;
//         std::getline(std::cin, input);
        
//         // 移除首尾空格
//         input.erase(0, input.find_first_not_of(" \t\n\r"));
//         input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
//         if (input.empty()) return;
        
//         auto tokens = split_input(input);
//         if (tokens.empty()) return;
        
//         auto message = base_interfaces_demo::msg::MotorCommand();
        
//         // 转换为小写处理命令
//         std::string cmd = tokens[0];
//         std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
//         if (cmd == "stop") {
//             message.command_type = "stop";
//             message.x=0.0;
//             message.y=0.0;
//             message.z=0.0;
//             message.time=0.0;
//             message.is_relative="false";
//             message.vx=0;
//             message.vy=0;
//             message.vz=0;
//             publisher_->publish(message);
//             std::cout << "[系统] 已发送紧急停止指令" << std::endl;
//             return;
//         }
//         else if (cmd == "help") {
//             print_usage();
//             return;
//         }
//         else if (cmd == "exit") {
//             // 先发送停止指令
//             message.command_type = "stop";
//             message.x=0.0;
//             message.y=0.0;
//             message.z=0.0;
//             message.time=0.0;
//             message.is_relative="false";
//             message.vx=0;
//             message.vy=0;
//             message.vz=0;
//             publisher_->publish(message);
//             std::cout << "[系统] 正在安全退出..." << std::endl;
//             rclcpp::shutdown();
//             return;
//         }
//         else if (cmd == "pos") {
//             if (tokens.size() < 5) {
//                 std::cerr << "[错误] 位置指令参数不足！需要 x y z time" << std::endl;
//                 return;
//             }
            
//             message.command_type = "position";
//             try {
//                 message.x = std::stod(tokens[1]);
//                 message.y = std::stod(tokens[2]);
//                 message.z = std::stod(tokens[3]);
//                 message.time = std::stod(tokens[4]);
                
//                 if (tokens.size() > 5) {
//                     std::string rel = tokens[5];
//                     std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);
//                     message.is_relative = (rel == "rel");
//                 } else {
//                     message.is_relative = false;
//                 }
                
//                 publisher_->publish(message);
//                 std::cout << "[系统] 已发送位置指令: "
//                           << (message.is_relative ? "相对" : "绝对") << "位置 ("
//                           << message.x << ", " << message.y << ", " << message.z 
//                           << ") 时间:" << message.time << "s" << std::endl;
//             } catch (const std::exception& e) {
//                 std::cerr << "[错误] 参数格式不正确: " << e.what() << std::endl;
//             }
//             return;
//         }
//         else if (cmd == "speed") {
//             if (tokens.size() < 4) {
//                 std::cerr << "[错误] 速度指令参数不足！需要 vx vy vz" << std::endl;
//                 return;
//             }
            
//             message.command_type = "speed";
//             try {
//                 message.vx = std::stoi(tokens[1]);
//                 message.vy = std::stoi(tokens[2]);
//                 message.vz = std::stoi(tokens[3]);
                
//                 publisher_->publish(message);
//                 std::cout << "[系统] 已发送速度指令: ("
//                           << message.vx << ", " << message.vy << ", " << message.vz 
//                           << ")" << std::endl;
//             } catch (const std::exception& e) {
//                 std::cerr << "[错误] 参数格式不正确: " << e.what() << std::endl;
//             }
//             return;
//         }
//         else {
//             std::cerr << "[错误] 未知指令！输入 'help' 查看帮助" << std::endl;
//         }
//     }
    
//     rclcpp::TimerBase::SharedPtr timer_;
//     rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr publisher_;
// };

// void signal_handler(int) {
//     rclcpp::shutdown();
// }

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
    
//     // 设置信号处理
//     signal(SIGINT, signal_handler);
//     signal(SIGTERM, signal_handler);
    
//     auto node = std::make_shared<MotorCommandPublisher>();
//     rclcpp::spin(node);
    
//     // 确保资源释放
//     rclcpp::shutdown();
//     std::cout << "[系统] 已安全退出" << std::endl;
//     return 0;
// }



// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>
// #include <csignal>
// #include <sstream>
// #include <algorithm>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node {
// public:
//     MotorCommandPublisher() : Node("motor_command_publisher") {
//         publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>("motor_commands", 10);
        
//         print_usage();
        
//         timer_ = this->create_wall_timer(
//             100ms, std::bind(&MotorCommandPublisher::timer_callback, this));
//     }

// private:
//     void print_usage() {
//         std::cout << "\n=== 电机控制指令系统 ===" << std::endl;
//         std::cout << "1. 位置控制: pos <x> <y> <z> <time(s)> [rel]" << std::endl;
//         std::cout << "   (示例: pos 100 50 20 5.0 rel - 相对移动)" << std::endl;
//         std::cout << "   (示例: pos 100 50 20 5.0    - 绝对移动)" << std::endl;
//         std::cout << "2. 速度控制: speed <vx> <vy> <vz>" << std::endl;
//         std::cout << "   (示例: speed 10 0 -5)" << std::endl;
//         std::cout << "3. 紧急停止: stop" << std::endl;
//         std::cout << "4. 安全退出: exit" << std::endl;
//         std::cout << "5. 帮助: help" << std::endl;
//         std::cout << "========================\n" << std::endl;
//     }

//     std::vector<std::string> split_input(const std::string& input) {
//         std::istringstream iss(input);
//         std::vector<std::string> tokens;
//         std::string token;
        
//         while (iss >> token) {
//             tokens.push_back(token);
//         }
        
//         return tokens;
//     }

//     void timer_callback() {
//         std::string input;
//         std::getline(std::cin, input);
        
//         // 移除首尾空格
//         input.erase(0, input.find_first_not_of(" \t\n\r"));
//         input.erase(input.find_last_not_of(" \t\n\r") + 1);
        
//         if (input.empty()) return;
        
//         auto tokens = split_input(input);
//         if (tokens.empty()) return;
        
//         auto message = base_interfaces_demo::msg::MotorCommand();
        
//         // 转换为小写处理命令
//         std::string cmd = tokens[0];
//         std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
//         if (cmd == "stop") {
//             message.command_type = "stop";
//             publisher_->publish(message);
//             std::cout << "[系统] 已发送紧急停止指令" << std::endl;
//             return;
//         }
//         else if (cmd == "help") {
//             print_usage();
//             return;
//         }
//         else if (cmd == "exit") {
//             // 先发送停止指令
//             message.command_type = "stop";
//             publisher_->publish(message);
//             std::cout << "[系统] 正在安全退出..." << std::endl;
//             rclcpp::shutdown();
//             return;
//         }
//         else if (cmd == "pos") {
//             if (tokens.size() < 5) {
//                 std::cerr << "[错误] 位置指令参数不足！需要 x y z time" << std::endl;
//                 return;
//             }
            
//             message.command_type = "position";
//             try {
//                 message.x = std::stod(tokens[1]);
//                 message.y = std::stod(tokens[2]);
//                 message.z = std::stod(tokens[3]);
//                 message.time = std::stod(tokens[4]);
                
//                 if (tokens.size() > 5) {
//                     std::string rel = tokens[5];
//                     std::transform(rel.begin(), rel.end(), rel.begin(), ::tolower);
//                     message.is_relative = (rel == "rel");
//                 } else {
//                     message.is_relative = false;
//                 }
                
//                 publisher_->publish(message);
//                 std::cout << "[系统] 已发送位置指令: "
//                           << (message.is_relative ? "相对" : "绝对") << "位置 ("
//                           << message.x << ", " << message.y << ", " << message.z 
//                           << ") 时间:" << message.time << "s" << std::endl;
//             } catch (const std::exception& e) {
//                 std::cerr << "[错误] 参数格式不正确: " << e.what() << std::endl;
//             }
//             return;
//         }
//         else if (cmd == "speed") {
//             if (tokens.size() < 4) {
//                 std::cerr << "[错误] 速度指令参数不足！需要 vx vy vz" << std::endl;
//                 return;
//             }
            
//             message.command_type = "speed";
//             try {
//                 message.vx = std::stoi(tokens[1]);
//                 message.vy = std::stoi(tokens[2]);
//                 message.vz = std::stoi(tokens[3]);
                
//                 publisher_->publish(message);
//                 std::cout << "[系统] 已发送速度指令: ("
//                           << message.vx << ", " << message.vy << ", " << message.vz 
//                           << ")" << std::endl;
//             } catch (const std::exception& e) {
//                 std::cerr << "[错误] 参数格式不正确: " << e.what() << std::endl;
//             }
//             return;
//         }
//         else {
//             std::cerr << "[错误] 未知指令！输入 'help' 查看帮助" << std::endl;
//         }
//     }
    
//     rclcpp::TimerBase::SharedPtr timer_;
//     rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr publisher_;
// };

// void signal_handler(int) {
//     rclcpp::shutdown();
// }

// int main(int argc, char * argv[]) {
//     rclcpp::init(argc, argv);
    
//     // 设置信号处理
//     signal(SIGINT, signal_handler);
//     signal(SIGTERM, signal_handler);
    
//     auto node = std::make_shared<MotorCommandPublisher>();
//     rclcpp::spin(node);
    
//     // 确保资源释放
//     rclcpp::shutdown();
//     std::cout << "[系统] 已安全退出" << std::endl;
//     return 0;
// }



// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>
// #include <sstream>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"
// #include "base_interfaces_demo/msg/motor_status.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node
// {
// public:
//   MotorCommandPublisher()
//   : Node("motor_command_publisher")
//   {
//     //创建目标位置发布者
//     publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>("huatai3_pos_spe_pd", 10);

//     // 创建当前位置订阅者
//     // position_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
//     //     "huatai3_pos_spe_p", 10,
//     //     [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//     //         this->handlePositionUpdate(msg);
//     //     });
//     //创建当前位置订阅者
//     position_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
//         "huatai3_pos_spe_p", 10, 
//         [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//             this->handlePositionUpdate(msg);
//         });
    
//     RCLCPP_INFO(this->get_logger(), "电机控制命令发布器已启动");
//     RCLCPP_INFO(this->get_logger(), "可用命令类型: position(位置模式), speed(速度模式), stop(停止), exit(退出)");
    
//     // 启动用户输入线程
//     input_thread_ = std::thread(&MotorCommandPublisher::read_user_input, this);
//   }

//   ~MotorCommandPublisher() {
//     if (input_thread_.joinable()) {
//       input_thread_.join();
//     }
//   }

// private:
//   void read_user_input()
//   {
//     while (rclcpp::ok()) {
//       std::cout << "\n请输入命令类型(position/speed/stop/exit): ";
//       std::string command_type;
//       std::getline(std::cin, command_type);
      
//       auto message = base_interfaces_demo::msg::MotorCommand();
//       message.command_type = command_type;
      
//       if (command_type == "position") {
//         std::cout << "请输入 X(mm) Y(mm) Z(mm) 时间(s) [是否相对运动(0/1)]: ";
//         std::string input;
//         std::getline(std::cin, input);
        
//         std::istringstream iss(input);
//         if (!(iss >> message.x >> message.y >> message.z >> message.time)) {
//           std::cerr << "输入格式错误！" << std::endl;
//           continue;
//         }
        
//         // 检查相对运动标志
//         int relative_flag = 0;
//         if (iss >> relative_flag) {
//           message.is_relative = (relative_flag != 0);
//         }
        
//       } else if (command_type == "speed") {
//         std::cout << "请输入 X速度(mm/s) Y速度(mm/s) Z速度(mm/s): ";
//         std::string input;
//         std::getline(std::cin, input);
        
//         std::istringstream iss(input);
//         if (!(iss >> message.vx >> message.vy >> message.vz)) {
//           std::cerr << "输入格式错误！" << std::endl;
//           continue;
//         }
//       } else if (command_type == "stop" || command_type == "exit") {
//         // 停止和退出命令不需要额外参数
//       } else {
//         std::cerr << "未知命令类型！" << std::endl;
//         continue;
//       }
      
//       publisher_->publish(message);
//       RCLCPP_INFO(this->get_logger(), "已发布命令: 类型=%s", command_type.c_str());
      
//       if (command_type == "exit") {
//         rclcpp::shutdown();
//         break;
//       }
//     }
//   }

//   void handlePositionUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//         RCLCPP_INFO(this->get_logger(), "当前电机位置: X=%.2f Y=%.2f Z=%.2f  状态: %s ",
//                 msg->x, msg->y, msg->z, 
//                 msg->reached_target ? "已到达" : "运动中");
               
//     // if (msg->reached_target) {
//     //     std::lock_guard<std::mutex> lock(queue_mutex_);
//     //     if (!command_queue_.empty()) {
//     //         command_queue_.pop();
//     //         if (!command_queue_.empty()) {
//     //             sendNextCommand();
//     //         } else {
//     //             RCLCPP_INFO(this->get_logger(), "所有轨迹点执行完成");
//     //         }
//     //     }
//     // }
//   }

//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr position_subscription_;
//   std::thread input_thread_;
// };

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<MotorCommandPublisher>());
//   rclcpp::shutdown();
//   return 0;
// }



#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

#include "rclcpp/rclcpp.hpp"
#include "base_interfaces_demo/msg/motor_command.hpp"
#include "base_interfaces_demo/msg/motor_status.hpp"

using namespace std::chrono_literals;

class MotorCommandPublisher : public rclcpp::Node
{
public:
  MotorCommandPublisher()
  : Node("motor_command_publisher1")
  {
    // 创建命令发布者
    command_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>(
      "huatai3_pos_spe_pd", 10);
    
    // 创建状态订阅者
    status_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
      "huatai3_pos_spe_p", 10, 
      [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
        this->handlePositionUpdate(msg);
      });
    
    RCLCPP_INFO(this->get_logger(), "电机控制命令发布器已启动");
    RCLCPP_INFO(this->get_logger(), "可用命令类型: position(位置模式), speed(速度模式), stop(停止), exit(退出)");
    
    // 启动用户输入线程
    input_thread_ = std::thread(&MotorCommandPublisher::read_user_input, this);
  }

  ~MotorCommandPublisher() {
    if (input_thread_.joinable()) {
      input_thread_.join();
    }
  }

private:
  void read_user_input()
  {
    while (rclcpp::ok()) {
      std::cout << "\n请输入命令类型(position/speed/stop/exit): ";
      std::string command_type;
      std::getline(std::cin, command_type);
      
      auto message = base_interfaces_demo::msg::MotorCommand();
      message.command_type = command_type;
      
      if (command_type == "position") {
        std::cout << "请输入 X(mm) Y(mm) Z(mm) 时间(s) [是否相对运动(0/1)]: ";
        std::string input;
        std::getline(std::cin, input);
        
        std::istringstream iss(input);
        if (!(iss >> message.x >> message.y >> message.z >> message.time)) {
          std::cerr << "输入格式错误！" << std::endl;
          continue;
        }
        
        // 检查相对运动标志
        int relative_flag = 0;
        if (iss >> relative_flag) {
          message.is_relative = (relative_flag != 0);
        }
        
      } else if (command_type == "speed") {
        std::cout << "请输入 X速度(mm/s) Y速度(mm/s) Z速度(mm/s): ";
        std::string input;
        std::getline(std::cin, input);
        
        std::istringstream iss(input);
        if (!(iss >> message.vx >> message.vy >> message.vz)) {
          std::cerr << "输入格式错误！" << std::endl;
          continue;
        }
      } else if (command_type == "stop" || command_type == "exit") {
        // 停止和退出命令不需要额外参数
      } else {
        std::cerr << "未知命令类型！" << std::endl;
        continue;
      }
      
      command_publisher_->publish(message);
      RCLCPP_INFO(this->get_logger(), "已发布命令: 类型=%s", command_type.c_str());
      
      if (command_type == "exit") {
        rclcpp::shutdown();
        break;
      }
    }
  }

  void handlePositionUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
    // std::cout << "\r";  // 清除当前行
    // std::cout << std::fixed << std::setprecision(2);
    // std::cout << "当前位置: X=" << msg->x << "mm, Y=" << msg->y 
    //           << "mm, Z=" << msg->z << "mm";
    // std::cout << " | 状态: " << (msg->reached_target ? "已到达" : "运动中");
    // std::cout << " | 当前速度: vx=" << msg->vx << "mm/s, vy=" << msg->vy
    //           << "mm/s, vz=" << msg->vz << "mm/s";
    // std::cout << std::flush;

  }

  rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
  rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
  std::thread input_thread_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotorCommandPublisher>());
  rclcpp::shutdown();
  return 0;
}
