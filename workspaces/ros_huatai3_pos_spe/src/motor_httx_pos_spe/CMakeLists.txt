cmake_minimum_required(VERSION 3.8)
project(motor_httx_pos_spe)

# 使用C++17标准（ROS2推荐）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译器警告选项
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 添加自定义工作空间路径
list(APPEND CMAKE_PREFIX_PATH "$ENV{HOME}/socket_can_ws/install")

# 查找核心依赖包
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(base_interfaces_demo REQUIRED)
find_package(can_msgs REQUIRED)
find_package(canopen_core REQUIRED)

# Humble专用CAN驱动配置
find_package(canopen_core REQUIRED)
message(STATUS "Using canopen_core for ${ROS_DISTRO}")

# 可执行文件1：talker.cpp
add_executable(talker
  src/talker.cpp
)
target_include_directories(talker PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(talker
  rclcpp
  std_msgs
  base_interfaces_demo
)

# 可执行文件2：listener_motor
add_executable(listener_motor
  src/listener_motor.cpp
  src/MotorController.cpp
)
target_include_directories(listener_motor PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(listener_motor
  rclcpp
  std_msgs
  base_interfaces_demo
  can_msgs
  canopen_core  # 添加canopen_core依赖
)


# 链接CAN驱动库（Humble使用canopen_core）
# target_link_libraries(motor_control_node
#   canopen_core::canopen_core
# )
target_link_libraries(listener_motor
  ${rclcpp_LIBRARIES}
  ${canopen_core_LIBRARIES}
)

# 安装目标
install(TARGETS 
  talker
  listener_motor
  DESTINATION lib/${PROJECT_NAME}
)

# 安装包含目录
install(
  DIRECTORY include/
  DESTINATION include
)


# 测试配置
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()