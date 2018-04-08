cmake_minimum_required(VERSION 3.9)
project(NLOPT)

set(CMAKE_CXX_STANDARD 11)

add_executable(NLOPT main.cpp)
target_link_libraries(NLOPT nlopt)