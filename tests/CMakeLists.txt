include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.12
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(doctest)

add_executable(test_simple_tui test_simple_tui.cpp)
target_include_directories(test_simple_tui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_simple_tui PRIVATE simple_tui doctest::doctest)

include(CTest)
add_test(NAME test_simple_tui COMMAND test_simple_tui)
