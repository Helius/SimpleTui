cmake_minimum_required(VERSION 3.14)
project(SimpleTui CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit compile_commands.json in the build directory for clangd / IDE tooling.
# Disable with -DSIMPLETUI_EXPORT_COMPILE_COMMANDS=OFF.
option(SIMPLETUI_EXPORT_COMPILE_COMMANDS "Generate compile_commands.json for clangd" ON)
if(SIMPLETUI_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

add_library(SimpleTui INTERFACE)
target_include_directories(SimpleTui INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

find_package(Threads REQUIRED)
add_executable(SimpleTuiRun src/main.cpp)
target_link_libraries(SimpleTuiRun PRIVATE SimpleTui Threads::Threads)

option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
