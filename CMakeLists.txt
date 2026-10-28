cmake_minimum_required(VERSION 3.14)
project(simple_tui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(simple_tui INTERFACE)
target_include_directories(simple_tui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
add_executable(simple_tui_run src/main.cpp)
target_link_libraries(simple_tui_run PRIVATE simple_tui Threads::Threads)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
