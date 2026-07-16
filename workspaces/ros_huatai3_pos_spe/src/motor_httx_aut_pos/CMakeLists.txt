cmake_minimum_required(VERSION 3.8)
project(motor_httx_aut_pos)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(base_interfaces_demo REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(can_msgs REQUIRED)
find_package(canopen_core REQUIRED)

# Humble专用CAN驱动配置
find_package(canopen_core REQUIRED)
message(STATUS "Using canopen_core for ${ROS_DISTRO}")

# 可执行文件1：talker_aut_pos.cpp
add_executable(talker_aut_pos
  src/talker_aut_pos.cpp
)
target_include_directories(talker_aut_pos PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(talker_aut_pos
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
  talker_aut_pos
  listener_motor
  DESTINATION lib/${PROJECT_NAME}
)

# 安装包含目录
install(
  DIRECTORY include/
  DESTINATION include
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
