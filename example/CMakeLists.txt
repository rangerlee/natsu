cmake_minimum_required(VERSION 2.8)
project(example)

include_directories(${PROJECT_SOURCE_DIR}/../inc)
link_directories(${PROJECT_SOURCE_DIR}/../)
add_definitions(-std=c++11)
add_executable(example main.cpp)

target_link_libraries(example natsu hiredis curl z)

