// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>
// #include <sstream>
// #include <iomanip>
// #include <queue>
// #include <vector>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"
// #include "base_interfaces_demo/msg/motor_status.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node
// {
// public:
//   MotorCommandPublisher()
//   : Node("motor_command_publisher"), command_count_(0)
//   {
//     // 初始化速度命令队列
//     initSpeedCommands();

//     // 创建命令发布者
//     command_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>(
//       "huatai3_pos_spe_pd", 10);
    
//     // 创建状态订阅者
//     status_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
//       "huatai3_pos_spe_p", 10, 
//       [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//         this->handlePositionUpdate(msg);
//       });
    
//     RCLCPP_INFO(this->get_logger(), "电机控制命令发布器已启动");
//     RCLCPP_INFO(this->get_logger(), "自动速度控制模式已激活");

//     // 创建定时器自动发送速度命令
//     timer_ = this->create_wall_timer(
//       10000ms,  // 每秒发送一次命令
//       [this]() {
//         this->publishNextSpeedCommand();
//       });
//   }

// private:
//   struct SpeedCommand {
//     double vx;
//     double vy;
//     double vz;
//   };

//   void initSpeedCommands() {
//     // 添加多组速度命令到队列
//     speed_commands_.push({2.0, 2.0, 2.0});   // X轴正向10mm/s
   
//   }

//   void publishNextSpeedCommand() {
//     if (speed_commands_.empty()) {
//       RCLCPP_INFO(this->get_logger(), "速度命令队列已执行完毕");
//       //rclcpp::shutdown();
//       return;
//     }

//     // 获取下一个速度命令
//     SpeedCommand cmd = speed_commands_.front();
//     speed_commands_.pop();

//     // 创建并发布消息
//     auto message = base_interfaces_demo::msg::MotorCommand();
//     message.command_type = "speed";
//     message.vx = cmd.vx;
//     message.vy = cmd.vy;
//     message.vz = cmd.vz;

//     command_publisher_->publish(message);
//     RCLCPP_INFO(this->get_logger(), 
//       "已发布速度命令 %d: vx=%.2f, vy=%.2f, vz=%.2f mm/s", 
//       ++command_count_, cmd.vx, cmd.vy, cmd.vz);
//   }

//   void handlePositionUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//     std::cout << "\r";  // 清除当前行
//     std::cout << std::fixed << std::setprecision(2);
//     std::cout << "当前位置: X=" << msg->x << "mm, Y=" << msg->y 
//               << "mm, Z=" << msg->z << "mm";
//     std::cout << " | 状态: " << (msg->reached_target ? "已到达" : "运动中");
//     std::cout << " | 当前速度: vx=" << msg->vx << "mm/s, vy=" << msg->vy
//               << "mm/s, vz=" << msg->vz << "mm/s";
//     std::cout << std::flush;
//   }

//   // 速度命令队列
//   std::queue<SpeedCommand> speed_commands_;
//   int command_count_;

//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
//   rclcpp::TimerBase::SharedPtr timer_;
// };

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   rclcpp::spin(std::make_shared<MotorCommandPublisher>());
//   rclcpp::shutdown();
//   return 0;
// }


// #include <chrono>
// #include <functional>
// #include <memory>
// #include <string>
// #include <iostream>
// #include <sstream>
// #include <iomanip>
// #include <deque>
// #include <mutex>
// #include <condition_variable>
// #include <cmath>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"
// #include "base_interfaces_demo/msg/motor_status.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node
// {
// public:
//   MotorCommandPublisher()
//   : Node("motor_command_publisher"), 
//     speed_command_in_progress_(false),
//     should_stop_(false),
//     should_exit_(false)
//   {
//     // 创建命令发布者
//     command_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>(
//       "huatai3_pos_spe_pd", 10);
    
//     // 创建状态订阅者
//     status_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
//       "huatai3_pos_spe_p", 10, 
//       [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//         this->handleStatusUpdate(msg);
//       });
    
//     RCLCPP_INFO(this->get_logger(), "电机控制命令发布器已启动");
    
//     // 初始化预设速度命令列表
//     initialize_speed_commands();
    
//     // 启动速度命令处理线程
//     speed_command_processor_thread_ = std::thread(&MotorCommandPublisher::process_speed_command_queue, this);
    
//     // 启动命令监听线程(用于stop和exit命令)
//     command_listener_thread_ = std::thread(&MotorCommandPublisher::listen_for_commands, this);
//   }

//   ~MotorCommandPublisher() {
//     should_exit_ = true;
//     if (speed_command_processor_thread_.joinable()) {
//       speed_command_processor_thread_.join();
//     }
//     if (command_listener_thread_.joinable()) {
//       command_listener_thread_.join();
//     }
//   }

// private:
//   struct SpeedCommand {
//     double vx;
//     double vy;
//     double vz;
//     double duration; // 持续时间(秒)
//   };

//   void initialize_speed_commands() {
//     // 在这里预设速度命令列表
//     speed_command_queue_ = {
//       {2.0, 2.0, 2.0, 30.0},   // 第一组速度命令
//       {5.0, 2.0, 1.0, 2.0},    // 第二组速度命令
//       {0.0, 0.0, 0.0, 1.0}     // 最后停止
//     };
    
//     RCLCPP_INFO(this->get_logger(), "已预设 %zu 个速度命令", speed_command_queue_.size());
//   }

//   void process_speed_command_queue() {
//     // 等待1秒确保订阅者连接
//     std::this_thread::sleep_for(1s);
    
//     while (rclcpp::ok() && !should_exit_ && !speed_command_queue_.empty()) {
//       // 检查停止命令
//       if (should_stop_) {
//         // 立即发送停止命令
//         auto stop_msg = base_interfaces_demo::msg::MotorCommand();
//         stop_msg.command_type = "stop";
//         command_publisher_->publish(stop_msg);
//         RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");
        
//         // 清空命令队列
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           speed_command_queue_.clear();
//         }
//         break;
//       }
      
//       SpeedCommand cmd;
//       {
//         std::lock_guard<std::mutex> lock(queue_mutex_);
//         cmd = speed_command_queue_.front();
//         speed_command_in_progress_ = true;
//       }
      
//       auto message = base_interfaces_demo::msg::MotorCommand();
//       message.command_type = "speed";
//       message.vx = cmd.vx;
//       message.vy = cmd.vy;
//       message.vz = cmd.vz;
      
//       command_publisher_->publish(message);
//       RCLCPP_INFO(this->get_logger(), "已发布速度命令: vx=%.2f, vy=%.2f, vz=%.2f, duration=%.2f",
//                  cmd.vx, cmd.vy, cmd.vz, cmd.duration);
      
//       // 等待命令持续时间结束或被停止
//       auto start_time = std::chrono::steady_clock::now();
//       while (rclcpp::ok() && !should_exit_ && !should_stop_ && 
//              (std::chrono::steady_clock::now() - start_time) < std::chrono::duration<double>(cmd.duration)) {
//         std::this_thread::sleep_for(10ms);
//       }
      
//       if (should_stop_) {
//         // 立即发送停止命令
//         auto stop_msg = base_interfaces_demo::msg::MotorCommand();
//         stop_msg.command_type = "stop";
//         command_publisher_->publish(stop_msg);
//         RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");
        
//         // 清空命令队列
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           speed_command_queue_.clear();
//         }
//         break;
//       }
      
//       if (should_exit_) {
//         // 发送退出命令
//         auto exit_msg = base_interfaces_demo::msg::MotorCommand();
//         exit_msg.command_type = "exit";
//         command_publisher_->publish(exit_msg);
//         RCLCPP_INFO(this->get_logger(), "已发布退出命令");
//         break;
//       }
      
//       // 从队列中移除已完成的命令
//       {
//         std::lock_guard<std::mutex> lock(queue_mutex_);
//         speed_command_queue_.pop_front();
//       }
      
//       speed_command_in_progress_ = false;
//     }
    
//     // 所有命令执行完成后停止节点
//     if (rclcpp::ok() && !should_exit_) {
//       RCLCPP_INFO(this->get_logger(), "所有预设速度命令已完成");
//     }
//     rclcpp::shutdown();
//   }

//   void listen_for_commands() {
//     while (rclcpp::ok() && !should_exit_) {
//       std::string command;
//       std::cout << "\n请输入命令(stop/exit): ";
//       std::getline(std::cin, command);
      
//       if (command == "stop") {
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           should_stop_ = true;
//         }
//         RCLCPP_INFO(this->get_logger(), "已接收停止命令");
//       }
//       else if (command == "exit") {
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           should_exit_ = true;
//           should_stop_ = false;
//         }
//         RCLCPP_INFO(this->get_logger(), "已接收退出命令");
//         break;
//       }
//       else {
//         RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
//       }
//     }
//   }

//   void handleStatusUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//     std::cout << "\r";  // 清除当前行
//     std::cout << std::fixed << std::setprecision(2);
//     std::cout << "当前位置: X=" << msg->x << "mm, Y=" << msg->y 
//               << "mm, Z=" << msg->z << "mm";
//     std::cout << " | 状态: " << (msg->reached_target ? "已到达" : "运动中");
//     std::cout << " | 当前速度: vx=" << msg->vx << "mm/s, vy=" << msg->vy
//               << "mm/s, vz=" << msg->vz << "mm/s";
//     std::cout << std::flush;
//   }

//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
//   std::thread speed_command_processor_thread_;
//   std::thread command_listener_thread_;
  
//   std::deque<SpeedCommand> speed_command_queue_;
//   std::mutex queue_mutex_;
  
//   bool speed_command_in_progress_;
//   bool should_stop_;
//   bool should_exit_;
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
#include <deque>
#include <mutex>
#include <condition_variable>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "base_interfaces_demo/msg/motor_command.hpp"
#include "base_interfaces_demo/msg/motor_status.hpp"

using namespace std::chrono_literals;

class MotorCommandPublisher : public rclcpp::Node
{
public:
  MotorCommandPublisher()
  : Node("motor_command_publisher1"), 
    speed_command_in_progress_(false),
    should_stop_(false),
    should_exit_(false)
  {
    // 创建命令发布者
    command_publisher_ = this->create_publisher<base_interfaces_demo::msg::MotorCommand>(
      "huatai3_pos_spe_pd", 10);
    
    // 创建状态订阅者
    status_subscription_ = this->create_subscription<base_interfaces_demo::msg::MotorStatus>(
      "huatai3_pos_spe_p", 10, 
      [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
        this->handleStatusUpdate(msg);
      });
    
    RCLCPP_INFO(this->get_logger(), "电机控制命令发布器已启动");
    
    // 初始化预设速度命令列表
    initialize_speed_commands();
    
    // 启动速度命令处理线程
    speed_command_processor_thread_ = std::thread(&MotorCommandPublisher::process_speed_command_queue, this);
    
    // 启动命令监听线程(用于stop和exit命令)
    command_listener_thread_ = std::thread(&MotorCommandPublisher::listen_for_commands, this);
  }

  ~MotorCommandPublisher() {
    should_exit_ = true;
    if (speed_command_processor_thread_.joinable()) {
      speed_command_processor_thread_.join();
    }
    if (command_listener_thread_.joinable()) {
      command_listener_thread_.join();
    }
  }

private:
  struct SpeedCommand {
    double vx;
    double vy;
    double vz;
    double duration; // 持续时间(秒)
  };

  void initialize_speed_commands() {
    // 在这里预设速度命令列表
    speed_command_queue_ = {
      {-2.0, -2.0, -2.0, 30.0},   // 第一组速度命令
      {5.0, 2.0, 1.0, 2.0},    // 第二组速度命令
      {0.0, 0.0, 0.0, 1.0}     // 最后停止
    };
    
    RCLCPP_INFO(this->get_logger(), "已预设 %zu 个速度命令", speed_command_queue_.size());
  }

  // void process_speed_command_queue() {
  //   // 等待1秒确保订阅者连接
  //   std::this_thread::sleep_for(1s);
    
  //   while (rclcpp::ok() && !should_exit_ && !speed_command_queue_.empty()) {
  //     // 检查停止命令
  //     if (should_stop_) {
  //       // 立即发送停止命令
  //       auto stop_msg = base_interfaces_demo::msg::MotorCommand();
  //       stop_msg.command_type = "stop";
  //       command_publisher_->publish(stop_msg);
  //       RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");
        
  //       // 清空命令队列
  //       {
  //         std::lock_guard<std::mutex> lock(queue_mutex_);
  //         speed_command_queue_.clear();
  //       }
  //       break;
  //     }
      
  //     SpeedCommand cmd;
  //     {
  //       std::lock_guard<std::mutex> lock(queue_mutex_);
  //       cmd = speed_command_queue_.front();
  //       speed_command_in_progress_ = true;
  //     }
      
  //     auto message = base_interfaces_demo::msg::MotorCommand();
  //     message.command_type = "speed";
  //     message.vx = cmd.vx;
  //     message.vy = cmd.vy;
  //     message.vz = cmd.vz;
      
  //     command_publisher_->publish(message);
  //     RCLCPP_INFO(this->get_logger(), "已发布速度命令: vx=%.2f, vy=%.2f, vz=%.2f, duration=%.2f",
  //                cmd.vx, cmd.vy, cmd.vz, cmd.duration);
      
  //     // 等待命令持续时间结束或被停止
  //     auto start_time = std::chrono::steady_clock::now();
  //     while (rclcpp::ok() && !should_exit_ && !should_stop_ && 
  //            (std::chrono::steady_clock::now() - start_time) < std::chrono::duration<double>(cmd.duration)) {
  //       std::this_thread::sleep_for(10ms);
  //     }
      
  //     if (should_stop_) {
  //       // 立即发送停止命令
  //       auto stop_msg = base_interfaces_demo::msg::MotorCommand();
  //       stop_msg.command_type = "stop";
  //       command_publisher_->publish(stop_msg);
  //       RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");

  //       speed_command_in_progress_ = true;
        
  //       // 清空命令队列
  //       // {
  //       //   std::lock_guard<std::mutex> lock(queue_mutex_);
  //       //   speed_command_queue_.clear();
  //       // }
  //       // break;
  //     }
      
  //     if (should_exit_) {
  //       // 发送退出命令
  //       auto exit_msg = base_interfaces_demo::msg::MotorCommand();
  //       exit_msg.command_type = "exit";
  //       command_publisher_->publish(exit_msg);
  //       RCLCPP_INFO(this->get_logger(), "已发布退出命令");
  //       //break;
  //     }
      
  //     // 从队列中移除已完成的命令
  //     {
  //       std::lock_guard<std::mutex> lock(queue_mutex_);
  //       speed_command_queue_.pop_front();
  //     }
      
  //     speed_command_in_progress_ = false;
  //   }
    
  //   // 所有命令执行完成后停止节点
  //   if (rclcpp::ok() && !should_exit_) {
  //     RCLCPP_INFO(this->get_logger(), "所有预设速度命令已完成");
  //   }
  //   rclcpp::shutdown();
  // }
  void process_speed_command_queue() {
    // 等待1秒确保订阅者连接
    std::this_thread::sleep_for(1s);
    
    while (rclcpp::ok() && !should_exit_ && !speed_command_queue_.empty()) {
        // 检查停止或退出命令
        if (should_stop_ || should_exit_) {
            auto stop_msg = base_interfaces_demo::msg::MotorCommand();
            stop_msg.command_type = "stop";
            command_publisher_->publish(stop_msg);
            RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");
            
            // 清空命令队列
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                speed_command_queue_.clear();
            }
            break;
        }
        
        // 处理当前命令...
        SpeedCommand cmd;
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            cmd = speed_command_queue_.front();
            speed_command_in_progress_ = true;
        }
        
        auto message = base_interfaces_demo::msg::MotorCommand();
        message.command_type = "speed";
        message.vx = cmd.vx;
        message.vy = cmd.vy;
        message.vz = cmd.vz;
        command_publisher_->publish(message);
        
        RCLCPP_INFO(this->get_logger(), "已发布速度命令: vx=%.2f, vy=%.2f, vz=%.2f, duration=%.2f",
                   cmd.vx, cmd.vy, cmd.vz, cmd.duration);
        
        // 等待命令持续时间结束或被停止
        auto start_time = std::chrono::steady_clock::now();
        while (rclcpp::ok() && !should_exit_ && !should_stop_ && 
               (std::chrono::steady_clock::now() - start_time) < std::chrono::duration<double>(cmd.duration)) {
            std::this_thread::sleep_for(10ms);
        }
        
        if (should_stop_ || should_exit_) {
            auto stop_msg = base_interfaces_demo::msg::MotorCommand();
            stop_msg.command_type = "stop";
            command_publisher_->publish(stop_msg);
            RCLCPP_INFO(this->get_logger(), "已立即发布停止命令");
            
            // 清空命令队列
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                speed_command_queue_.clear();
            }
            break;
        }
        
        // 从队列中移除已完成的命令
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            speed_command_queue_.pop_front();
        }
        
        speed_command_in_progress_ = false;
    }
    
    // 所有命令执行完成后停止节点
    if (rclcpp::ok() && !should_exit_) {
        RCLCPP_INFO(this->get_logger(), "所有预设速度命令已完成");
    }
}

  // void listen_for_commands() {
  //   while (rclcpp::ok() && !should_exit_) {
  //     std::string command;
  //     std::cout << "\n请输入命令(stop/exit): ";
  //     std::getline(std::cin, command);
      
  //     if (command == "stop") {
  //       {
  //         std::lock_guard<std::mutex> lock(queue_mutex_);
  //         should_stop_ = true;
  //       }
  //       RCLCPP_INFO(this->get_logger(), "已接收停止命令");
        
  //       // 立即发送停止命令
  //       auto stop_msg = base_interfaces_demo::msg::MotorCommand();
  //       stop_msg.command_type = "stop";
  //       command_publisher_->publish(stop_msg);
  //     }
  //     else if (command == "exit") {


  //       // 发送退出命令
  //       auto exit_msg = base_interfaces_demo::msg::MotorCommand();
  //       exit_msg.command_type = "exit";
  //       command_publisher_->publish(exit_msg);
  //       rclcpp::shutdown();

  //       {
  //         std::lock_guard<std::mutex> lock(queue_mutex_);
  //         should_exit_ = true;
  //         //should_stop_ = true;
        
  //       }
  //       RCLCPP_INFO(this->get_logger(), "已接收退出命令");
        
  //       std::this_thread::sleep_for(1000ms);
        
        
        
  //       break;
  //     }
  //     else {
  //       RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
  //     }
  //   }
  // }
  void listen_for_commands() {
    while (rclcpp::ok() && !should_exit_) {
        std::string command;
        std::cout << "\n请输入命令(stop/exit): ";
        std::getline(std::cin, command);

        if (command == "stop") {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                should_stop_ = true;
            }
            RCLCPP_INFO(this->get_logger(), "已接收停止命令");
            
            // 立即发送停止命令
            auto stop_msg = base_interfaces_demo::msg::MotorCommand();
            stop_msg.command_type = "stop";
            command_publisher_->publish(stop_msg);
        }
        else if (command == "exit") {
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                should_exit_ = true;
                should_stop_ = true;  // 确保 speed_command_processor_thread_ 也能退出
            }
            RCLCPP_INFO(this->get_logger(), "已接收退出命令");
            
            // 发送退出命令
            auto exit_msg = base_interfaces_demo::msg::MotorCommand();
            exit_msg.command_type = "exit";
            command_publisher_->publish(exit_msg);
            
            break;  // 退出循环
        }
        else {
            RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
        }
    }
}


  void handleStatusUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
    std::cout << "\r";  // 清除当前行
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "当前位置: X=" << msg->x << "mm, Y=" << msg->y 
              << "mm, Z=" << msg->z << "mm";
    std::cout << " | 状态: " << (msg->reached_target ? "已到达" : "运动中");
    std::cout << " | 当前速度: vx=" << msg->vx << "mm/s, vy=" << msg->vy
              << "mm/s, vz=" << msg->vz << "mm/s";
    std::cout << std::flush;
  }

  rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
  rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
  std::thread speed_command_processor_thread_;
  std::thread command_listener_thread_;
  
  std::deque<SpeedCommand> speed_command_queue_;
  std::mutex queue_mutex_;
  
  bool speed_command_in_progress_;
  bool should_stop_;
  bool should_exit_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotorCommandPublisher>());
  rclcpp::shutdown();
  return 0;
}




