cmake_minimum_required(VERSION 3.0.0)
project(ThreadPoolTest VERSION 0.1.0)
add_definitions(-std=c++11)

include(CTest)
enable_testing()

add_executable(ThreadPoolTest threadpoolstudy.cpp)

target_link_libraries(ThreadPoolTest pthread)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
