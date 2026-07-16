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
//     initPositionCommands();

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
//     // timer_ = this->create_wall_timer(
//     //   10000ms,  // 每秒发送一次命令
//     //   [this](const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
//     //     this->publishNextPositionCommand(msg);
//     //   });
 
//   }

// private:
//   struct PositionCommand {
//     double x;
//     double y;
//     double z;
//     bool is_relative;
//   };


//   void initPositionCommands() {
//     // 添加多组速度命令到队列
//     position_commands_.push({100.0, 100.0, 100.0}); 
//     position_commands_.push({50.0, 50.0, 50.0});  
   
//   }

//   void publishNextPositionCommand() {
//     if (position_commands_.empty()) {
//       RCLCPP_INFO(this->get_logger(), "速度命令队列已执行完毕");
//       //rclcpp::shutdown();
//       return;
//     }

//     // 获取下一个速度命令
   
//         PositionCommand cmd = position_commands_.front();
//         position_commands_.pop();

    

//     // 创建并发布消息
//     auto message = base_interfaces_demo::msg::MotorCommand();
//     message.command_type = "position";
//     message.x = cmd.x;
//     message.y = cmd.y;
//     message.z = cmd.z;
//     message.is_relative=cmd.is_relative;

//     command_publisher_->publish(message);
//     RCLCPP_INFO(this->get_logger(), 
//       "已发布速度命令 %d: vx=%.2f, vy=%.2f, vz=%.2f mm/s", 
//       ++command_count_, cmd.x, cmd.y, cmd.z);
//     std::cout << " | 运动模式: " << (cmd.is_relative? "相对运动" : "绝对运动");
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
//   std::queue<PositionCommand> position_commands_;
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
// #include <queue>
// #include <vector>
// #include <thread>
// #include <mutex>

// #include "rclcpp/rclcpp.hpp"
// #include "base_interfaces_demo/msg/motor_command.hpp"
// #include "base_interfaces_demo/msg/motor_status.hpp"

// using namespace std::chrono_literals;

// class MotorCommandPublisher : public rclcpp::Node
// {
// public:
//   MotorCommandPublisher()
//   : Node("motor_command_publisher"), command_count_(0), current_command_completed_(true)
//   {
    
//     // 初始化预定义的位置命令列表
//     initPositionCommands();

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
//     RCLCPP_INFO(this->get_logger(), "位置控制模式已激活");

//     // 启动命令处理线程
//     command_thread_ = std::thread([this]() {
//       this->processCommandQueue();
//     });
//   }

//   ~MotorCommandPublisher() {
//     if (command_thread_.joinable()) {
//       command_thread_.join();
//     }
//   }

// private:
//   struct PositionCommand {
//     double x;
//     double y;
//     double z;
//     bool is_relative;
//     std::string description;
//   };

//   void initPositionCommands() {
//     // 添加预定义的位置命令到队列
//     // position_commands_.push({"position"});
//     position_commands_.push({100.0, 100.0, 100.0, false});
//     position_commands_.push({200.0, 150.0, 120.0, false});
//     position_commands_.push({150.0, 180.0, 100.0, false});
//     position_commands_.push({50.0, 50.0, 50.0, false});

//     RCLCPP_INFO(this->get_logger(), "已加载 %ld 个预定义位置命令", position_commands_.size());
//   }

//   void processCommandQueue() {
//     while (rclcpp::ok()) {
//       if (current_command_completed_ && !position_commands_.empty()) {
//         PositionCommand cmd;
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           cmd = position_commands_.front();
//           position_commands_.pop();
//         }
        
//         current_command_completed_ = false;
//         publishPositionCommand(cmd);
        
//         RCLCPP_INFO(this->get_logger(), "正在执行命令: %s", cmd.description.c_str());
//       }
//       std::this_thread::sleep_for(100ms);
//     }
//   }

//   void publishPositionCommand(const PositionCommand& cmd) {
//     auto message = base_interfaces_demo::msg::MotorCommand();
//     message.command_type = "position";
//     message.x = cmd.x;
//     message.y = cmd.y;
//     message.z = cmd.z;
//     message.is_relative = cmd.is_relative;

//     command_publisher_->publish(message);
//     RCLCPP_INFO(this->get_logger(), 
//       "已发布位置命令 %d: x=%.2f, y=%.2f, z=%.2f mm (%s)", 
//       ++command_count_, cmd.x, cmd.y, cmd.z, 
//       cmd.is_relative ? "相对运动" : "绝对运动");
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

//     // 如果电机到达目标位置，标记当前命令完成
//     if (msg->reached_target && !current_command_completed_) {
//       current_command_completed_ = true;
//       RCLCPP_INFO(this->get_logger(), "命令执行完成");
//     }
//   }

//   // 预定义的位置命令队列
//   std::queue<PositionCommand> position_commands_;
//   std::mutex queue_mutex_;
//   std::atomic<bool> current_command_completed_;
//   int command_count_;

//   // ROS2接口
//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
  
//   // 命令处理线程
//   std::thread command_thread_;
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
//     command_in_progress_(false),
//     target_reached_(false),
//     should_stop_(false),
//     should_exit_(false),
//     current_target_x_(0.0),
//     current_target_y_(0.0),
//     current_target_z_(0.0),
//     position_tolerance_(0.5)  // 位置容差0.5mm
//   {
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
    
//     // 预设位置命令列表
//     initialize_position_commands();
    
//     // 启动命令处理线程
//     command_processor_thread_ = std::thread(&MotorCommandPublisher::process_command_queue, this);
    
//     // 启动用户命令监听线程
//     command_listener_thread_ = std::thread(&MotorCommandPublisher::listen_for_commands, this);
//   }

//   ~MotorCommandPublisher() {
//     should_exit_ = true;
//     if (command_processor_thread_.joinable()) {
//       command_processor_thread_.join();
//     }
//     if (command_listener_thread_.joinable()) {
//       command_listener_thread_.join();
//     }
//   }

// private:
//   struct PositionCommand {
//     double x;
//     double y;
//     double z;
//     double time;
//     bool is_relative;
//   };

//   void initialize_position_commands() {
//     // 在这里预设位置命令列表
//     position_command_queue_ = {
//       {1.0, 1.0, 1.0, 20.0, false},   // 绝对位置1
//       {150.0, 80.0, 30.0, 3.0, false},   // 绝对位置2
//       {20.0, 10.0, 5.0, 2.0, true},      // 相对位置1
//       {-10.0, -5.0, -2.0, 2.0, true}     // 相对位置2
//     };
    
//     RCLCPP_INFO(this->get_logger(), "已预设 %zu 个位置命令", position_command_queue_.size());
//   }

//   void process_command_queue() {
//     // 等待1秒确保订阅者连接
//     std::this_thread::sleep_for(1s);
    
//     while (rclcpp::ok() && !should_exit_ && !position_command_queue_.empty()) {
//       // 检查停止命令
//       if (should_stop_) {
//         std::unique_lock<std::mutex> lock(status_mutex_);
//         stop_cv_.wait(lock, [this]() { return !should_stop_ || should_exit_; });
//         if (should_exit_) break;
     
//       }
      
//       PositionCommand cmd;
//       {
//         std::lock_guard<std::mutex> lock(queue_mutex_);
//         cmd = position_command_queue_.front();
//         position_command_queue_.pop_front();
//       }
      
//       // 计算绝对目标位置
//       if (cmd.is_relative) {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         current_target_x_ += cmd.x;
//         current_target_y_ += cmd.y;
//         current_target_z_ += cmd.z;
//       } else {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         current_target_x_ = cmd.x;
//         current_target_y_ = cmd.y;
//         current_target_z_ = cmd.z;
//       }
      
//       // 发布当前命令
//       auto message = base_interfaces_demo::msg::MotorCommand();
//       message.command_type = "position";
//       message.x = cmd.x;
//       message.y = cmd.y;
//       message.z = cmd.z;
//       message.time = cmd.time;
//       message.is_relative = cmd.is_relative;
      
//       {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         command_in_progress_ = true;
//         target_reached_ = false;
//       }
      
//       command_publisher_->publish(message);
//       RCLCPP_INFO(this->get_logger(), "已发布位置命令: x=%.2f, y=%.2f, z=%.2f, time=%.2f, relative=%d",
//                  message.x, message.y, message.z, message.time, message.is_relative);
      
//       //等待命令完成
//       std::unique_lock<std::mutex> lock(status_mutex_);
//       status_cv_.wait_for(lock, std::chrono::seconds(static_cast<int>(cmd.time * 2)), [this]() { 
//         return target_reached_ || should_stop_ || should_exit_; 
//       });
      
//       if (!target_reached_ && !should_stop_ && !should_exit_) {
//         RCLCPP_WARN(this->get_logger(), "命令超时未完成，继续执行下一条命令");
//       }
      
//       if (should_exit_) break;
      
//       // 等待500ms确保状态稳定
//       std::this_thread::sleep_for(500ms);
    
//     }
    
//     // 所有命令执行完成后停止节点
//     if (rclcpp::ok() && !should_exit_) {
//       RCLCPP_INFO(this->get_logger(), "所有预设命令已完成");
//       rclcpp::shutdown();
//     }
//   }

//   void listen_for_commands() {
//     while (rclcpp::ok() && !should_exit_) {
//       std::string command;
//       std::cout << "\n请输入命令(stop/resume/exit): ";
//       std::getline(std::cin, command);
      
//       if (command == "stop") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_stop_ = true;
//         }
//         // 发布停止命令
//         auto stop_msg = base_interfaces_demo::msg::MotorCommand();
//         stop_msg.command_type = "stop";
//         command_publisher_->publish(stop_msg);
//         RCLCPP_INFO(this->get_logger(), "已发布停止命令");
//       } 
//       else if (command == "resume") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_stop_ = false;
//         }
//         stop_cv_.notify_one();
//         RCLCPP_INFO(this->get_logger(), "已恢复命令执行");
//       }
//       else if (command == "exit") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_exit_ = true;
//           should_stop_ = false;
//         }
//         // 发布退出命令
//         auto exit_msg = base_interfaces_demo::msg::MotorCommand();
//         exit_msg.command_type = "exit";
//         command_publisher_->publish(exit_msg);
//         stop_cv_.notify_one();
//         status_cv_.notify_one();
//         RCLCPP_INFO(this->get_logger(), "已发布退出命令");
//         rclcpp::shutdown();
//         break;
//       }
//       // else if (command == "exit") {
//       //   {
//       //       std::lock_guard<std::mutex> lock(status_mutex_);
//       //       should_exit_ = true;
//       //       should_stop_ = false;  // 确保停止等待
//       //       target_reached_ = true; // 强制标记目标已到达
//       //       command_in_progress_ = false;
//       //   }
//       //   // 通知所有可能阻塞的线程
//       //   stop_cv_.notify_all();
//       //   status_cv_.notify_all();
        
//       //   // 发布退出命令（可选）
//       //   auto exit_msg = base_interfaces_demo::msg::MotorCommand();
//       //   exit_msg.command_type = "exit";
//       //   command_publisher_->publish(exit_msg);
        
//       //   RCLCPP_INFO(this->get_logger(), "已发布退出命令");
//       //   break;
//       // }
//       else {
//         RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
//       }
//     }
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

//     if (command_in_progress_ && !should_stop_) {
//       std::lock_guard<std::mutex> lock(status_mutex_);
//       // 双重验证：既要看reached_target标志，也要看实际位置是否在容差范围内
//       bool position_ok = (std::abs(msg->x - current_target_x_) < position_tolerance_) &&
//                          (std::abs(msg->y - current_target_y_) < position_tolerance_) &&
//                          (std::abs(msg->z - current_target_z_) < position_tolerance_);
      
//       if (msg->reached_target && position_ok) {
//         target_reached_ = true;
//         command_in_progress_ = false;
//         status_cv_.notify_one();
//       }
//     }
//   }

//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
//   std::thread command_processor_thread_;
//   std::thread command_listener_thread_;
  
//   std::deque<PositionCommand> position_command_queue_;
//   std::mutex queue_mutex_;
  
//   bool command_in_progress_;
//   bool target_reached_;
//   bool should_stop_;
//   bool should_exit_;
//   double current_target_x_;
//   double current_target_y_;
//   double current_target_z_;
//   double position_tolerance_;
//   std::mutex status_mutex_;
//   std::condition_variable status_cv_;
//   std::condition_variable stop_cv_;
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
//     command_in_progress_(false),
//     target_reached_(false),
//     should_stop_(false),
//     should_exit_(false),
//     current_target_x_(0.0),
//     current_target_y_(0.0),
//     current_target_z_(0.0),
//     position_tolerance_(0.5)  // 位置容差0.5mm
//   {
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
    
//     // 预设位置命令列表
//     initialize_position_commands();
    
//     // 启动命令处理线程
//     command_processor_thread_ = std::thread(&MotorCommandPublisher::process_command_queue, this);
    
//     // 启动用户命令监听线程
//     command_listener_thread_ = std::thread(&MotorCommandPublisher::listen_for_commands, this);
//   }

//   ~MotorCommandPublisher() {
//     should_exit_ = true;
//     stop_cv_.notify_all();
//     status_cv_.notify_all();
    
//     if (command_processor_thread_.joinable()) {
//       command_processor_thread_.join();
//     }
//     if (command_listener_thread_.joinable()) {
//       command_listener_thread_.join();
//     }
//   }

// private:
//   struct PositionCommand {
//     double x;
//     double y;
//     double z;
//     double time;
//     bool is_relative;
//   };

//   void initialize_position_commands() {
//     position_command_queue_ = {
//       {1.0, 1.0, 1.0, 20.0, false},   // 绝对位置1
//       {150.0, 80.0, 30.0, 3.0, false},   // 绝对位置2
//       {20.0, 10.0, 5.0, 2.0, true},      // 相对位置1
//       {-10.0, -5.0, -2.0, 2.0, true}     // 相对位置2
//     };
    
//     RCLCPP_INFO(this->get_logger(), "已预设 %zu 个位置命令", position_command_queue_.size());
//   }

//   void process_command_queue() {
//     std::this_thread::sleep_for(1s);
    
//     while (rclcpp::ok() && !should_exit_ && !position_command_queue_.empty()) {
//       // 检查停止命令
//       if (should_stop_) {
//         std::unique_lock<std::mutex> lock(status_mutex_);
//         stop_cv_.wait(lock, [this]() { return !should_stop_ || should_exit_; });
//         if (should_exit_) break;
        
//         // 恢复后清空队列
//         {
//           std::lock_guard<std::mutex> lock(queue_mutex_);
//           position_command_queue_.clear();
//         }
//         continue;
//       }
      
//       PositionCommand cmd;
//       {
//         std::lock_guard<std::mutex> lock(queue_mutex_);
//         cmd = position_command_queue_.front();
//         position_command_queue_.pop_front();
//       }
      
//       // 计算绝对目标位置
//       if (cmd.is_relative) {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         current_target_x_ += cmd.x;
//         current_target_y_ += cmd.y;
//         current_target_z_ += cmd.z;
//       } else {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         current_target_x_ = cmd.x;
//         current_target_y_ = cmd.y;
//         current_target_z_ = cmd.z;
//       }
      
//       // 发布当前命令
//       auto message = base_interfaces_demo::msg::MotorCommand();
//       message.command_type = "position";
//       message.x = cmd.x;
//       message.y = cmd.y;
//       message.z = cmd.z;
//       message.time = cmd.time;
//       message.is_relative = cmd.is_relative;
      
//       {
//         std::lock_guard<std::mutex> lock(status_mutex_);
//         command_in_progress_ = true;
//         target_reached_ = false;
//       }
      
//       command_publisher_->publish(message);
//       RCLCPP_INFO(this->get_logger(), "已发布位置命令: x=%.2f, y=%.2f, z=%.2f, time=%.2f, relative=%d",
//                  message.x, message.y, message.z, message.time, message.is_relative);
      
//       // 更细粒度的等待逻辑
//       auto start_time = std::chrono::steady_clock::now();
//       while (rclcpp::ok() && !should_exit_ && !should_stop_ && !target_reached_) {
//         std::unique_lock<std::mutex> lock(status_mutex_);
//         if (status_cv_.wait_for(lock, 100ms, [this]() { 
//           return target_reached_ || should_stop_ || should_exit_;
//         })) {
//           break;
//         }
        
//         // 检查超时
//         if (std::chrono::steady_clock::now() - start_time > 
//             std::chrono::seconds(static_cast<int>(cmd.time * 2))) {
//           RCLCPP_WARN(this->get_logger(), "命令超时未完成");
//           break;
//         }
//       }
      
//       if (should_stop_ || should_exit_) break;
//       std::this_thread::sleep_for(500ms);
//     }
    
//     if (rclcpp::ok() && !should_exit_) {
//       RCLCPP_INFO(this->get_logger(), "所有预设命令已完成");
//       rclcpp::shutdown();
//     }
//   }

//   void listen_for_commands() {
//     while (rclcpp::ok() && !should_exit_) {
//       std::string command;
//       std::cout << "\n请输入命令(stop/resume/exit): ";
//       std::getline(std::cin, command);
      
//       if (command == "stop") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_stop_ = true;
//           target_reached_ = true;  // 强制标记目标已到达
//           command_in_progress_ = false;
//         }
//         // 发布急停命令
//         auto stop_msg = base_interfaces_demo::msg::MotorCommand();
//         stop_msg.command_type = "emergency_stop";
//         command_publisher_->publish(stop_msg);
        
//         // 唤醒所有线程
//         stop_cv_.notify_all();
//         status_cv_.notify_all();
        
//         RCLCPP_INFO(this->get_logger(), "已发布急停命令");
//       } 
//       else if (command == "resume") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_stop_ = false;
//         }
//         stop_cv_.notify_all();
//         RCLCPP_INFO(this->get_logger(), "已恢复命令执行");
//       }
//       else if (command == "exit") {
//         {
//           std::lock_guard<std::mutex> lock(status_mutex_);
//           should_exit_ = true;
//           should_stop_ = false;
//           target_reached_ = true;
//           command_in_progress_ = false;
//         }
//         // 发布退出命令
//         auto exit_msg = base_interfaces_demo::msg::MotorCommand();
//         exit_msg.command_type = "exit";
//         command_publisher_->publish(exit_msg);
        
//         // 唤醒所有线程
//         stop_cv_.notify_all();
//         status_cv_.notify_all();
        
//         RCLCPP_INFO(this->get_logger(), "已发布退出命令");
//         rclcpp::shutdown();
//         break;
//       }
//       else {
//         RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
//       }
//     }
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

//     if (command_in_progress_ && !should_stop_) {
//       std::lock_guard<std::mutex> lock(status_mutex_);
//       bool position_ok = (std::abs(msg->x - current_target_x_) < position_tolerance_) &&
//                          (std::abs(msg->y - current_target_y_) < position_tolerance_) &&
//                          (std::abs(msg->z - current_target_z_) < position_tolerance_);
      
//       if (msg->reached_target && position_ok) {
//         target_reached_ = true;
//         command_in_progress_ = false;
//         status_cv_.notify_all();
//       }
//     }
//   }

//   rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
//   rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
//   std::thread command_processor_thread_;
//   std::thread command_listener_thread_;
  
//   std::deque<PositionCommand> position_command_queue_;
//   std::mutex queue_mutex_;
  
//   bool command_in_progress_;
//   bool target_reached_;
//   bool should_stop_;
//   bool should_exit_;
//   double current_target_x_;
//   double current_target_y_;
//   double current_target_z_;
//   double position_tolerance_;
//   std::mutex status_mutex_;
//   std::condition_variable status_cv_;
//   std::condition_variable stop_cv_;
// };

// int main(int argc, char * argv[])
// {
//   rclcpp::init(argc, argv);
//   auto node = std::make_shared<MotorCommandPublisher>();
  
//   // 使用独立的executor
//   rclcpp::executors::SingleThreadedExecutor executor;
//   executor.add_node(node);
  
//   // 启动spin线程
//   std::thread spin_thread([&executor]() {
//     executor.spin();
//   });
  
//   // 等待spin线程结束
//   if (spin_thread.joinable()) {
//     spin_thread.join();
//   }
  
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
    command_in_progress_(false),
    target_reached_(false),
    should_stop_(false),
    should_exit_(false),
    current_target_x_(0.0),
    current_target_y_(0.0),
    current_target_z_(0.0),
    position_tolerance_(0.5)  // 位置容差0.5mm
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
    
    // 预设位置命令列表
    initialize_position_commands();
    
    // 启动命令处理线程
    command_processor_thread_ = std::thread(&MotorCommandPublisher::process_command_queue, this);
    
    // 启动用户命令监听线程
    command_listener_thread_ = std::thread(&MotorCommandPublisher::listen_for_commands, this);
  }

  ~MotorCommandPublisher() {
    should_exit_ = true;
    if (command_processor_thread_.joinable()) {
      command_processor_thread_.join();
    }
    if (command_listener_thread_.joinable()) {
      command_listener_thread_.join();
    }
  }

private:
  struct PositionCommand {
    double x;
    double y;
    double z;
    double time;
    bool is_relative;
  };

  void initialize_position_commands() {
    // 在这里预设位置命令列表
    position_command_queue_ = {
      {1.0, 1.0, 1.0, 20.0, false},   // 绝对位置1
      {150.0, 80.0, 30.0, 3.0, false},   // 绝对位置2
      {20.0, 10.0, 5.0, 2.0, true},      // 相对位置1
      {-10.0, -5.0, -2.0, 2.0, true}     // 相对位置2
    };
    
    RCLCPP_INFO(this->get_logger(), "已预设 %zu 个位置命令", position_command_queue_.size());
  }

  void process_command_queue() {
    // 等待1秒确保订阅者连接
    std::this_thread::sleep_for(1s);
    
    while (rclcpp::ok() && !should_exit_ && !position_command_queue_.empty()) {
      // 检查停止命令
      if (should_stop_) {
        std::unique_lock<std::mutex> lock(status_mutex_);
        stop_cv_.wait(lock, [this]() { return !should_stop_ || should_exit_; });
        if (should_exit_) break;
      }
      
      PositionCommand cmd;
      {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        cmd = position_command_queue_.front();
        position_command_queue_.pop_front();
      }
      
      // 计算绝对目标位置
      if (cmd.is_relative) {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_target_x_ += cmd.x;
        current_target_y_ += cmd.y;
        current_target_z_ += cmd.z;
      } else {
        std::lock_guard<std::mutex> lock(status_mutex_);
        current_target_x_ = cmd.x;
        current_target_y_ = cmd.y;
        current_target_z_ = cmd.z;
      }
      
      // 发布当前命令
      auto message = base_interfaces_demo::msg::MotorCommand();
      message.command_type = "position";
      message.x = cmd.x;
      message.y = cmd.y;
      message.z = cmd.z;
      message.time = cmd.time;
      message.is_relative = cmd.is_relative;
      
      {
        std::lock_guard<std::mutex> lock(status_mutex_);
        command_in_progress_ = true;
        target_reached_ = false;
      }
      
      command_publisher_->publish(message);
      RCLCPP_INFO(this->get_logger(), "已发布位置命令: x=%.2f, y=%.2f, z=%.2f, time=%.2f, relative=%d",
                 message.x, message.y, message.z, message.time, message.is_relative);
      
      // 等待命令完成
      std::unique_lock<std::mutex> lock(status_mutex_);
      status_cv_.wait_for(lock, std::chrono::seconds(static_cast<int>(cmd.time * 2)), [this]() { 
        return target_reached_ || should_stop_ || should_exit_; 
      });
      
      if (!target_reached_ && !should_stop_ && !should_exit_) {
        RCLCPP_WARN(this->get_logger(), "命令超时未完成，继续执行下一条命令");
      }
      
      if (should_exit_) break;
      
      // 等待500ms确保状态稳定
      std::this_thread::sleep_for(500ms);
    }
    
    // 所有命令执行完成后停止节点
    if (rclcpp::ok() && !should_exit_) {
      RCLCPP_INFO(this->get_logger(), "所有预设命令已完成");
      rclcpp::shutdown();
    }
  }

  void listen_for_commands() {
    while (rclcpp::ok() && !should_exit_) {
      std::string command;
      std::cout << "\n请输入命令(stop/resume/exit): ";
      std::getline(std::cin, command);
      
      if (command == "stop") {
        {
          std::lock_guard<std::mutex> lock(status_mutex_);
          should_stop_ = true;
        }
        // 发布停止命令
        auto stop_msg = base_interfaces_demo::msg::MotorCommand();
        stop_msg.command_type = "stop";
        command_publisher_->publish(stop_msg);
        RCLCPP_INFO(this->get_logger(), "已发布停止命令");
      } 
      else if (command == "resume") {
        {
          std::lock_guard<std::mutex> lock(status_mutex_);
          should_stop_ = false;
        }
        stop_cv_.notify_one();
        RCLCPP_INFO(this->get_logger(), "已恢复命令执行");
      }
      else if (command == "exit") {
        {
          std::lock_guard<std::mutex> lock(status_mutex_);
          should_exit_ = true;
          should_stop_ = false;
        }
        // 发布退出命令
        auto exit_msg = base_interfaces_demo::msg::MotorCommand();
        exit_msg.command_type = "exit";
        command_publisher_->publish(exit_msg);
        stop_cv_.notify_one();
        status_cv_.notify_one();
        RCLCPP_INFO(this->get_logger(), "已发布退出命令");
        rclcpp::shutdown();
        break;
      }
      else {
        RCLCPP_WARN(this->get_logger(), "未知命令: %s", command.c_str());
      }
    }
  }

  void handlePositionUpdate(const base_interfaces_demo::msg::MotorStatus::SharedPtr msg) {
    std::cout << "\r";  // 清除当前行
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "当前位置: X=" << msg->x << "mm, Y=" << msg->y 
              << "mm, Z=" << msg->z << "mm";
    std::cout << " | 状态: " << (msg->reached_target ? "已到达" : "运动中");
    std::cout << " | 当前速度: vx=" << msg->vx << "mm/s, vy=" << msg->vy
              << "mm/s, vz=" << msg->vz << "mm/s";
    std::cout << std::flush;

    if (command_in_progress_ && !should_stop_) {
      std::lock_guard<std::mutex> lock(status_mutex_);
      // 双重验证：既要看reached_target标志，也要看实际位置是否在容差范围内
      bool position_ok = (std::abs(msg->x - current_target_x_) < position_tolerance_) &&
                         (std::abs(msg->y - current_target_y_) < position_tolerance_) &&
                         (std::abs(msg->z - current_target_z_) < position_tolerance_);
      
      if (msg->reached_target && position_ok) {
        target_reached_ = true;
        command_in_progress_ = false;
        status_cv_.notify_one();
      }
    }
  }

  rclcpp::Publisher<base_interfaces_demo::msg::MotorCommand>::SharedPtr command_publisher_;
  rclcpp::Subscription<base_interfaces_demo::msg::MotorStatus>::SharedPtr status_subscription_;
  std::thread command_processor_thread_;
  std::thread command_listener_thread_;
  
  std::deque<PositionCommand> position_command_queue_;
  std::mutex queue_mutex_;
  
  bool command_in_progress_;
  bool target_reached_;
  bool should_stop_;
  bool should_exit_;
  double current_target_x_;
  double current_target_y_;
  double current_target_z_;
  double position_tolerance_;
  std::mutex status_mutex_;
  std::condition_variable status_cv_;
  std::condition_variable stop_cv_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MotorCommandPublisher>());
  rclcpp::shutdown();
  return 0;
}
