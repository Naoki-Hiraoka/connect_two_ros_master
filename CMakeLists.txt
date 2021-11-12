cmake_minimum_required(VERSION 3.0.2)
project(connect_two_ros_master)

add_compile_options(-std=c++11)
set(CMAKE_BUILD_TYPE Release)

find_package(catkin REQUIRED COMPONENTS
  topic_tools)

## System dependencies are found with CMake's conventions
find_package(Boost REQUIRED program_options)

catkin_package(
#  INCLUDE_DIRS include
#  LIBRARIES connect_two_ros_master
#  CATKIN_DEPENDS other_catkin_pkg
#  DEPENDS system_lib
)

###########
## Build ##
###########

include_directories(
  ${catkin_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

add_executable(connect_two_master src/connect_two_master.cpp)
target_link_libraries(connect_two_master
  ${catkin_LIBRARIES}
  ${Boost_LIBRARIES}
  rt
  )

#############
## Install ##
#############

install(TARGETS connect_two_master
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

#############
## Testing ##
#############

## Add gtest based cpp test target and link libraries
# catkin_add_gtest(${PROJECT_NAME}-test test/test_connect_two_ros_master.cpp)
# if(TARGET ${PROJECT_NAME}-test)
#   target_link_libraries(${PROJECT_NAME}-test ${PROJECT_NAME})
# endif()

## Add folders to be run by python nosetests
# catkin_add_nosetests(test)
