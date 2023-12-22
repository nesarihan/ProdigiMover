cmake_minimum_required(VERSION 3.0.2)
project(prodigimover_bringup)

add_compile_options(-std=c++11)

find_package(catkin REQUIRED COMPONENTS
  geometry_msgs
  nav_msgs
  prodigimover_msgs
  roscpp
  rospy
  rosserial_python
  sensor_msgs
  std_msgs
  tf2
  tf2_ros
)

catkin_package(
 INCLUDE_DIRS include
 LIBRARIES prodigimover_bringup
 CATKIN_DEPENDS geometry_msgs nav_msgs prodigimover_msgs roscpp rospy rosserial_python sensor_msgs std_msgs tf2 tf2_ros
# DEPENDS system_lib
)

###########
## Build ##
###########

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  )

add_library(prodigimover_base src/prodigimover_base.cpp)
add_executable(prodigimover_base_node src/prodigimover_base_node.cpp)
target_link_libraries(prodigimover_base_node prodigimover_base ${catkin_LIBRARIES})
add_dependencies(prodigimover_base prodigimover_msgs_generate_messages_cpp)
