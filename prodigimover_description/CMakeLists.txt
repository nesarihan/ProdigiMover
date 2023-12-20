cmake_minimum_required(VERSION 3.0.2)
project(prodigimover_description)

## Compile as C++11, supported in ROS Kinetic and newer
# add_compile_options(-std=c++11)

find_package(catkin REQUIRED COMPONENTS
  urdf
  xacro
)


catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES prodigimover_description
#  CATKIN_DEPENDS urdf xacro
#  DEPENDS system_lib
)

###########
## Build ##
###########

## Specify additional locations of header files
## Your package locations should be listed before other locations
include_directories( include 

  ${catkin_INCLUDE_DIRS}
)

#############
## Install ##
#############

install(DIRECTORY meshes rviz urdf
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)

#############
## Testing ##
#############
