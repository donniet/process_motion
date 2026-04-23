cmake_minimum_required(VERSION 3.10.0)
project(process_motion VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE True)
set(CMAKE_BIULD_TYPE Debug)


find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(include)
link_libraries(pthread dl)

add_executable(process_motion src/main.cpp)
target_link_libraries(process_motion PRIVATE Boost::program_options cec)
add_executable(test_countdown src/test_countdown.cpp)


include(CTest)
enable_testing()

