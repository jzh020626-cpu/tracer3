cmake_minimum_required(VERSION 3.8)
project(force_sensor_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Specify include directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include/force_sensor_pkg
)

# Add executable
add_executable(force_sensor_node src/force_sensor_reader.cpp)

# Link dependencies
ament_target_dependencies(force_sensor_node rclcpp std_msgs)

# Install executable
install(TARGETS
  force_sensor_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install header files
install(DIRECTORY include/
  DESTINATION include/
  FILES_MATCHING PATTERN "*.hpp"
)

ament_package()