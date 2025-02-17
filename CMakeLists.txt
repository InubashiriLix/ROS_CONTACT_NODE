cmake_minimum_required(VERSION 3.8)
project(contact_infantryDL)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(gary_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

find_package(spdlog REQUIRED)

include_directories(
  include
)

add_subdirectory(serial/lib)

add_executable(
  contact
  src/ContactNode.cpp
  src/CommPort.cpp
  src/Checksum.cpp
)

target_link_libraries(
  contact
  serial
  spdlog::spdlog
  -lpthread
)

ament_target_dependencies(
  contact
  rclcpp
  gary_msgs
  sensor_msgs
)

install(
  TARGETS
  contact
  DESTINATION lib/${PROJECT_NAME}
)

ament_package()
